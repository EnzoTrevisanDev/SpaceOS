#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>
#include "netbuf.h"

/* Protocolos IP */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

#define IP_HDR_LEN      20   /* sem opcoes */

typedef struct {
    uint8_t  version_ihl;    /* versao (4) + IHL (5) = 0x45 */
    uint8_t  dscp_ecn;
    uint16_t total_length;   /* big-endian */
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;         /* big-endian */
    uint32_t dst_ip;         /* big-endian */
} __attribute__((packed)) ip_header_t;

/* Configuracao de rede (definidos em ipv4.c, preenchidos pelo DHCP) */
extern uint32_t net_our_ip;      /* host order */
extern uint32_t net_gateway_ip;  /* host order */
extern uint32_t net_netmask;     /* host order */
extern uint32_t net_dns_ip;      /* host order */

/*
 * ip_send — envia datagrama IP.
 * dst_ip: host order.
 * Resolve MAC via ARP se necessario.
 */
int ip_send(uint32_t dst_ip, uint8_t proto,
            const uint8_t *payload, uint16_t len);

/* ip_recv — chamado por eth_recv para datagramas IP */
void ip_recv(netbuf_t *buf);

/* Checksum RFC 1071 (one's complement) */
uint16_t ip_checksum(const void *data, uint32_t len);

#endif
