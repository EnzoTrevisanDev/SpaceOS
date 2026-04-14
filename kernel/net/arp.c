#include "arp.h"
#include "ethernet.h"
#include "net_util.h"
#include "../disk/kstring.h"
#include "../proc/sched.h"

/* IP global do nosso host (definido em ipv4.c) */
extern uint32_t net_our_ip;

/* ---- Formato ARP ------------------------------------------------- */
typedef struct {
    uint16_t htype;    /* hardware type: 1 = Ethernet */
    uint16_t ptype;    /* protocol type: 0x0800 = IPv4 */
    uint8_t  hlen;     /* hardware addr len: 6 */
    uint8_t  plen;     /* protocol addr len: 4 */
    uint16_t op;       /* 1 = request, 2 = reply */
    uint8_t  sha[6];   /* sender hardware addr */
    uint32_t spa;      /* sender protocol addr */
    uint8_t  tha[6];   /* target hardware addr */
    uint32_t tpa;      /* target protocol addr */
} __attribute__((packed)) arp_packet_t;

/* ---- Cache ARP --------------------------------------------------- */
typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

/* ---- Helpers ----------------------------------------------------- */

static void arp_cache_add(uint32_t ip, const uint8_t *mac) {
    /* Procura entrada existente ou livre */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == ip) {
            arp_cache[i].ip    = ip;
            k_memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }
    /* Cache cheio — substitui a posicao 0 (FIFO simples) */
    arp_cache[0].ip = ip;
    k_memcpy(arp_cache[0].mac, mac, 6);
    arp_cache[0].valid = 1;
}

int arp_cache_lookup(uint32_t ip, uint8_t *mac_out) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            k_memcpy(mac_out, arp_cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

static void arp_send_request(uint32_t target_ip) {
    arp_packet_t pkt;
    k_memset(&pkt, 0, sizeof(pkt));

    pkt.htype = htons(1);           /* Ethernet */
    pkt.ptype = htons(0x0800);      /* IPv4 */
    pkt.hlen  = 6;
    pkt.plen  = 4;
    pkt.op    = htons(1);           /* request */

    const uint8_t *mac = eth_get_mac();
    if (mac) k_memcpy(pkt.sha, mac, 6);
    pkt.spa = htonl(net_our_ip);
    k_memset(pkt.tha, 0, 6);
    pkt.tpa = htonl(target_ip);

    eth_send(ETH_BROADCAST, ETH_PROTO_ARP,
             (const uint8_t *)&pkt, sizeof(pkt));
}

/* ---- API publica ------------------------------------------------- */

void arp_recv(netbuf_t *buf) {
    if (!buf || buf->len < (uint16_t)sizeof(arp_packet_t)) {
        netbuf_free(buf);
        return;
    }

    arp_packet_t *pkt = (arp_packet_t *)(buf->data + buf->offset);

    /* Armazena mapeamento do sender sempre que recebemos um ARP */
    uint32_t sender_ip  = ntohl(pkt->spa);
    if (sender_ip) arp_cache_add(sender_ip, pkt->sha);

    /* Se for request para nos, responde */
    if (ntohs(pkt->op) == 1 && ntohl(pkt->tpa) == net_our_ip) {
        arp_packet_t reply;
        reply.htype = htons(1);
        reply.ptype = htons(0x0800);
        reply.hlen  = 6;
        reply.plen  = 4;
        reply.op    = htons(2);  /* reply */

        const uint8_t *mac = eth_get_mac();
        if (mac) k_memcpy(reply.sha, mac, 6);
        reply.spa = htonl(net_our_ip);
        k_memcpy(reply.tha, pkt->sha, 6);
        reply.tpa = pkt->spa;

        eth_send(pkt->sha, ETH_PROTO_ARP,
                 (const uint8_t *)&reply, sizeof(reply));
    }

    netbuf_free(buf);
}

int arp_resolve(uint32_t ip, uint8_t *mac_out, uint32_t timeout_ms) {
    /* Verifica cache primeiro */
    if (arp_cache_lookup(ip, mac_out)) return 0;

    /* Envia ARP request */
    arp_send_request(ip);

    /* Busy-poll esperando a resposta */
    uint32_t start = sched_ticks();
    uint32_t ticks_max = timeout_ms / 10;  /* ~100Hz timer */

    while (sched_ticks() - start < ticks_max) {
        eth_poll();
        if (arp_cache_lookup(ip, mac_out)) return 0;
        __asm__ volatile ("pause");
    }

    return -1;
}

void arp_gratuitous(void) {
    if (!net_our_ip) return;
    arp_send_request(net_our_ip);  /* Target IP = nosso proprio IP */
}
