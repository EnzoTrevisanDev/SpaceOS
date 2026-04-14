#include "udp.h"
#include "ipv4.h"
#include "net_util.h"
#include "../disk/kstring.h"

typedef struct {
    uint16_t     port;
    udp_handler_t handler;
} udp_binding_t;

static udp_binding_t bindings[UDP_MAX_HANDLERS];
static uint8_t udp_out[1500];   /* buffer de saida estatico */

int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
             const uint8_t *data, uint16_t len) {
    if (len + UDP_HDR_LEN > 1480) return -1;

    udp_header_t *hdr = (udp_header_t *)udp_out;
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length   = htons((uint16_t)(UDP_HDR_LEN + len));
    hdr->checksum = 0;  /* opcional em IPv4 */

    k_memcpy(udp_out + UDP_HDR_LEN, data, len);

    return ip_send(dst_ip, IP_PROTO_UDP,
                   udp_out, (uint16_t)(UDP_HDR_LEN + len));
}

int udp_bind(uint16_t port, udp_handler_t handler) {
    for (int i = 0; i < UDP_MAX_HANDLERS; i++) {
        if (!bindings[i].handler || bindings[i].port == port) {
            bindings[i].port    = port;
            bindings[i].handler = handler;
            return 0;
        }
    }
    return -1;  /* sem slot livre */
}

void udp_unbind(uint16_t port) {
    for (int i = 0; i < UDP_MAX_HANDLERS; i++) {
        if (bindings[i].port == port) {
            bindings[i].handler = 0;
            bindings[i].port    = 0;
        }
    }
}

void udp_recv(netbuf_t *buf) {
    if (!buf) return;

    uint8_t *data  = buf->data + buf->offset;
    uint16_t avail = (uint16_t)(buf->len - buf->offset);

    if (avail < UDP_HDR_LEN) { netbuf_free(buf); return; }

    udp_header_t *hdr = (udp_header_t *)data;
    uint16_t dst_port  = ntohs(hdr->dst_port);
    uint16_t src_port  = ntohs(hdr->src_port);
    uint16_t payload_len = (uint16_t)(ntohs(hdr->length) - UDP_HDR_LEN);

    /* Recupera IP origem do header IP (antes do offset atual) */
    /* O IP header ja foi consumido; buscamos src_ip do caller */
    /* Por simplicidade, passamos 0 como src_ip quando nao disponivel */
    uint32_t src_ip = 0;

    /* Procura handler registrado */
    for (int i = 0; i < UDP_MAX_HANDLERS; i++) {
        if (bindings[i].handler && bindings[i].port == dst_port) {
            bindings[i].handler(src_ip, src_port,
                                data + UDP_HDR_LEN, payload_len);
            netbuf_free(buf);
            return;
        }
    }

    netbuf_free(buf);
}
