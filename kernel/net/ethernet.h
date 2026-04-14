#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include "netbuf.h"

#define ETH_ALEN       6       /* tamanho do MAC address */
#define ETH_PROTO_ARP  0x0806
#define ETH_PROTO_IP   0x0800
#define ETH_HDR_LEN    14

static const uint8_t ETH_BROADCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

typedef struct {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;   /* big-endian */
} __attribute__((packed)) eth_header_t;

/*
 * eth_send — monta header Ethernet e transmite via RTL8139.
 * dst_mac: MAC destino; proto: ETH_PROTO_IP ou ETH_PROTO_ARP (host order)
 */
int eth_send(const uint8_t *dst_mac, uint16_t proto,
             const uint8_t *payload, uint16_t payload_len);

/*
 * eth_recv — chamado pelo RTL8139 para cada frame recebido.
 * Dispatcha para arp_recv() ou ip_recv() conforme ethertype.
 */
void eth_recv(netbuf_t *buf);

/* eth_poll — chama rtl8139_poll() e retorna frames processados */
int eth_poll(void);

/* Retorna o MAC do nosso adaptador */
const uint8_t *eth_get_mac(void);

#endif
