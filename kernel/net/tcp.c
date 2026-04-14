#include "tcp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "net_util.h"
#include "../disk/kstring.h"
#include "../proc/sched.h"

static tcp_socket_t sockets[TCP_MAX_SOCKETS];
static uint16_t next_local_port = 49152;  /* ephemeral ports */
static uint8_t tcp_out[1500];

/* ---- Checksum TCP (pseudo-header) -------------------------------- */

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const uint8_t *seg, uint16_t seg_len) {
    /* Pseudo-header: src_ip, dst_ip, zero, proto, tcp_length */
    uint32_t sum = 0;

    /* Soma pseudo-header */
    sum += (src_ip >> 16) & 0xFFFF;
    sum += (src_ip      ) & 0xFFFF;
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += (dst_ip      ) & 0xFFFF;
    sum += IP_PROTO_TCP;
    sum += seg_len;

    /* Soma segmento TCP */
    const uint16_t *p = (const uint16_t *)seg;
    uint16_t rem = seg_len;
    while (rem > 1) { sum += *p++; rem -= 2; }
    if (rem) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ---- Envio de segmento TCP --------------------------------------- */

static int tcp_send_segment(int sock, uint8_t flags,
                             const uint8_t *data, uint16_t data_len) {
    tcp_socket_t *s = &sockets[sock];

    tcp_header_t *hdr = (tcp_header_t *)tcp_out;
    hdr->src_port   = htons(s->local_port);
    hdr->dst_port   = htons(s->remote_port);
    hdr->seq_num    = htonl(s->seq_num);
    hdr->ack_num    = (flags & TCP_ACK) ? htonl(s->ack_num) : 0;
    hdr->data_offset = (uint8_t)(5 << 4);  /* 20 bytes, sem opcoes */
    hdr->flags      = flags;
    hdr->window     = htons(2048);
    hdr->checksum   = 0;
    hdr->urgent     = 0;

    if (data && data_len)
        k_memcpy(tcp_out + TCP_HDR_LEN, data, data_len);

    uint16_t seg_len = (uint16_t)(TCP_HDR_LEN + data_len);
    hdr->checksum = tcp_checksum(net_our_ip, s->remote_ip,
                                  tcp_out, seg_len);

    return ip_send(s->remote_ip, IP_PROTO_TCP, tcp_out, seg_len);
}

/* ---- Handshake --------------------------------------------------- */

int tcp_connect(uint32_t ip, uint16_t port) {
    /* Acha socket livre */
    int sock = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (sockets[i].state == TCP_CLOSED) { sock = i; break; }
    }
    if (sock < 0) return -1;

    tcp_socket_t *s = &sockets[sock];
    k_memset(s, 0, sizeof(*s));
    s->state       = TCP_SYN_SENT;
    s->remote_ip   = ip;
    s->remote_port = port;
    s->local_port  = next_local_port++;
    s->seq_num     = 0x12345678;  /* ISN arbitrario */
    s->ack_num     = 0;

    /* Envia SYN */
    tcp_send_segment(sock, TCP_SYN, 0, 0);
    s->seq_num++;  /* SYN consome um numero de seq */

    /* Aguarda SYN+ACK */
    uint32_t start = sched_ticks();
    while (s->state == TCP_SYN_SENT && (sched_ticks() - start) < 300) {
        eth_poll();
        __asm__ volatile ("pause");
    }

    if (s->state != TCP_ESTABLISHED) {
        s->state = TCP_CLOSED;
        return -1;
    }
    return sock;
}

/* ---- Send -------------------------------------------------------- */

int tcp_send(int sock, const uint8_t *data, uint16_t len) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t *s = &sockets[sock];
    if (s->state != TCP_ESTABLISHED) return -1;

    int ret = tcp_send_segment(sock, TCP_ACK | TCP_PSH, data, len);
    if (ret == 0) s->seq_num += len;
    return (ret == 0) ? len : -1;
}

/* ---- Recv -------------------------------------------------------- */

int tcp_recv(int sock, uint8_t *buf, uint16_t max_len, uint32_t timeout_ms) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t *s = &sockets[sock];

    uint32_t start = sched_ticks();
    uint32_t ticks_max = timeout_ms / 10;

    while (1) {
        /* Dados no buffer? */
        if (s->rx_head != s->rx_tail) {
            uint16_t avail;
            if (s->rx_tail >= s->rx_head)
                avail = (uint16_t)(s->rx_tail - s->rx_head);
            else
                avail = (uint16_t)(TCP_RX_BUF_SIZE - s->rx_head + s->rx_tail);

            uint16_t to_copy = (avail < max_len) ? avail : max_len;

            for (uint16_t i = 0; i < to_copy; i++) {
                buf[i] = s->rx_buf[s->rx_head];
                s->rx_head = (uint16_t)((s->rx_head + 1) % TCP_RX_BUF_SIZE);
            }
            return (int)to_copy;
        }

        if (s->state == TCP_CLOSE_WAIT) return 0;  /* FIN recebido */
        if (s->state == TCP_CLOSED)     return -1;

        if (timeout_ms == 0 || (sched_ticks() - start) >= ticks_max) return 0;

        eth_poll();
        __asm__ volatile ("pause");
    }
}

/* ---- Close ------------------------------------------------------- */

void tcp_close(int sock) {
    if (sock < 0 || sock >= TCP_MAX_SOCKETS) return;
    tcp_socket_t *s = &sockets[sock];
    if (s->state == TCP_CLOSED) return;

    tcp_send_segment(sock, TCP_FIN | TCP_ACK, 0, 0);
    s->seq_num++;
    s->state = TCP_FIN_WAIT;

    /* Aguarda ACK do FIN */
    uint32_t start = sched_ticks();
    while (s->state == TCP_FIN_WAIT && (sched_ticks() - start) < 200)
        eth_poll();

    s->state = TCP_CLOSED;
}

/* ---- Receive dispatch -------------------------------------------- */

void tcp_recv_dispatch(netbuf_t *buf) {
    if (!buf) return;

    uint8_t *data = buf->data + buf->offset;
    uint16_t avail = (uint16_t)(buf->len - buf->offset);

    if (avail < TCP_HDR_LEN) { netbuf_free(buf); return; }

    tcp_header_t *hdr = (tcp_header_t *)data;
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t src_port = ntohs(hdr->src_port);
    uint8_t  flags    = hdr->flags;
    uint32_t seq      = ntohl(hdr->seq_num);
    uint32_t ack      = ntohl(hdr->ack_num);
    uint8_t  hdr_len  = (uint8_t)((hdr->data_offset >> 4) * 4);

    uint8_t *payload     = data + hdr_len;
    uint16_t payload_len = (uint16_t)(avail - hdr_len);

    /* Recupera IP de origem (esta no IP header, antes do atual offset) */
    uint32_t src_ip = 0;
    if (buf->offset >= 20) {
        src_ip = ntohl(*(uint32_t *)(buf->data + buf->offset - 8));
    }

    /* Encontra socket correspondente */
    tcp_socket_t *s = 0;
    int sock = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (sockets[i].state != TCP_CLOSED &&
            sockets[i].local_port  == dst_port &&
            sockets[i].remote_port == src_port) {
            s = &sockets[i];
            sock = i;
            break;
        }
    }

    if (!s) { netbuf_free(buf); return; }

    switch (s->state) {
        case TCP_SYN_SENT:
            /* Esperando SYN+ACK */
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                s->ack_num = seq + 1;
                s->state   = TCP_ESTABLISHED;
                /* Envia ACK */
                tcp_send_segment(sock, TCP_ACK, 0, 0);
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_RST) {
                s->state = TCP_CLOSED;
                break;
            }
            if (flags & TCP_FIN) {
                s->ack_num++;
                s->state = TCP_CLOSE_WAIT;
                tcp_send_segment(sock, TCP_ACK, 0, 0);
            }
            if (payload_len > 0) {
                s->ack_num += payload_len;
                /* Copia payload para rx_buf circular */
                for (uint16_t i = 0; i < payload_len; i++) {
                    uint16_t next = (uint16_t)((s->rx_tail + 1) % TCP_RX_BUF_SIZE);
                    if (next != s->rx_head) {
                        s->rx_buf[s->rx_tail] = payload[i];
                        s->rx_tail = next;
                    }
                }
                /* ACK */
                tcp_send_segment(sock, TCP_ACK, 0, 0);
            }
            break;

        case TCP_FIN_WAIT:
            if (flags & (TCP_ACK | TCP_FIN)) {
                s->state = TCP_CLOSED;
            }
            break;
    }

    (void)ack; (void)src_ip;
    netbuf_free(buf);
}
