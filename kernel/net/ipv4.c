#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "net_util.h"
#include "../disk/kstring.h"

/* Declaracoes forward */
void udp_recv(netbuf_t *buf);
void tcp_recv_dispatch(netbuf_t *buf);

/* ---- Estado de rede ---------------------------------------------- */
uint32_t net_our_ip     = 0;
uint32_t net_gateway_ip = 0;
uint32_t net_netmask    = 0;
uint32_t net_dns_ip     = 0;

static uint16_t ip_id_counter = 1;

static uint8_t ip_packet[1500];  /* buffer de saida (estatico) */

/* ---- Checksum RFC 1071 ------------------------------------------- */

uint16_t ip_checksum(const void *data, uint32_t len) {
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)data;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len)
        sum += *(const uint8_t *)p;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ---- Envio ------------------------------------------------------- */

int ip_send(uint32_t dst_ip, uint8_t proto,
            const uint8_t *payload, uint16_t payload_len) {
    if (payload_len + IP_HDR_LEN > 1480) return -1;

    ip_header_t *hdr = (ip_header_t *)ip_packet;
    hdr->version_ihl  = 0x45;   /* IPv4, IHL = 5 (20 bytes, sem opcoes) */
    hdr->dscp_ecn     = 0;
    hdr->total_length = htons((uint16_t)(IP_HDR_LEN + payload_len));
    hdr->id           = htons(ip_id_counter++);
    hdr->flags_frag   = 0;
    hdr->ttl          = 64;
    hdr->protocol     = proto;
    hdr->checksum     = 0;
    hdr->src_ip       = htonl(net_our_ip);
    hdr->dst_ip       = htonl(dst_ip);

    hdr->checksum = ip_checksum(hdr, IP_HDR_LEN);

    k_memcpy(ip_packet + IP_HDR_LEN, payload, payload_len);

    /* Resolve MAC: broadcast para 255.255.255.255, ARP para demais */
    uint8_t dst_mac[6];
    uint32_t nexthop;

    if (dst_ip == 0xFFFFFFFF || dst_ip == 0) {
        /* Broadcast */
        k_memcpy(dst_mac, ETH_BROADCAST, 6);
        nexthop = dst_ip;
    } else {
        /* Verifica se e na mesma rede */
        if (net_netmask && (dst_ip & net_netmask) == (net_our_ip & net_netmask))
            nexthop = dst_ip;
        else
            nexthop = net_gateway_ip ? net_gateway_ip : dst_ip;

        if (arp_resolve(nexthop, dst_mac, 3000) != 0)
            return -1;
    }

    return eth_send(dst_mac, ETH_PROTO_IP,
                    ip_packet, (uint16_t)(IP_HDR_LEN + payload_len));
}

/* ---- Recepcao ---------------------------------------------------- */

void ip_recv(netbuf_t *buf) {
    if (!buf) return;

    uint8_t *data = buf->data + buf->offset;
    uint16_t avail = (uint16_t)(buf->len - buf->offset);

    if (avail < IP_HDR_LEN) { netbuf_free(buf); return; }

    ip_header_t *hdr = (ip_header_t *)data;

    /* So IPv4 sem opcoes */
    if ((hdr->version_ihl >> 4) != 4) { netbuf_free(buf); return; }
    if ((hdr->version_ihl & 0xF) != 5) { netbuf_free(buf); return; }

    /* Verifica checksum */
    if (ip_checksum(hdr, IP_HDR_LEN) != 0) { netbuf_free(buf); return; }

    /* So processa pacotes destinados a nos (ou broadcast) */
    uint32_t dst = ntohl(hdr->dst_ip);
    if (net_our_ip && dst != net_our_ip && dst != 0xFFFFFFFF) {
        netbuf_free(buf);
        return;
    }

    buf->offset = (uint16_t)(buf->offset + IP_HDR_LEN);

    switch (hdr->protocol) {
        case IP_PROTO_UDP:
            udp_recv(buf);
            break;
        case IP_PROTO_TCP:
            tcp_recv_dispatch(buf);
            break;
        default:
            netbuf_free(buf);
            break;
    }
}
