#include "ethernet.h"
#include "rtl8139.h"
#include "net_util.h"
#include "../disk/kstring.h"

/* Declaracoes forward (evitar includes circulares) */
void arp_recv(netbuf_t *buf);
void ip_recv(netbuf_t *buf);

static uint8_t eth_frame[1514];  /* buffer de transmissao (estatico) */

int eth_send(const uint8_t *dst_mac, uint16_t proto,
             const uint8_t *payload, uint16_t payload_len) {
    if (payload_len > 1500) return -1;

    const uint8_t *our_mac = rtl8139_get_mac();
    eth_header_t *hdr = (eth_header_t *)eth_frame;

    k_memcpy(hdr->dst, dst_mac, 6);
    k_memcpy(hdr->src, our_mac ? our_mac : (const uint8_t *)"\0\0\0\0\0\0", 6);
    hdr->ethertype = htons(proto);

    k_memcpy(eth_frame + ETH_HDR_LEN, payload, payload_len);

    return rtl8139_send(eth_frame, (uint16_t)(ETH_HDR_LEN + payload_len));
}

void eth_recv(netbuf_t *buf) {
    if (!buf || buf->len < ETH_HDR_LEN) {
        netbuf_free(buf);
        return;
    }

    eth_header_t *hdr = (eth_header_t *)(buf->data + buf->offset);
    uint16_t proto = ntohs(hdr->ethertype);

    buf->offset = (uint16_t)(buf->offset + ETH_HDR_LEN);

    switch (proto) {
        case ETH_PROTO_ARP:
            arp_recv(buf);
            break;
        case ETH_PROTO_IP:
            ip_recv(buf);
            break;
        default:
            netbuf_free(buf);
            break;
    }
}

int eth_poll(void) {
    return rtl8139_poll();
}

const uint8_t *eth_get_mac(void) {
    return rtl8139_get_mac();
}
