#include "dhcp.h"
#include "udp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "net_util.h"
#include "../disk/kstring.h"
#include "../proc/sched.h"
#include "../shell/shell.h"

#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_ACK       5

/* Opcoes DHCP */
#define OPT_SUBNET_MASK    1
#define OPT_ROUTER         3
#define OPT_DNS            6
#define OPT_HOSTNAME       12
#define OPT_MSG_TYPE       53
#define OPT_SERVER_ID      54
#define OPT_PARAM_REQ      55
#define OPT_END            255

typedef struct {
    uint8_t  op;       /* 1=BOOTREQUEST, 2=BOOTREPLY */
    uint8_t  htype;    /* 1 = Ethernet */
    uint8_t  hlen;     /* 6 */
    uint8_t  hops;
    uint32_t xid;      /* transaction ID */
    uint16_t secs;
    uint16_t flags;    /* bit 15 = broadcast */
    uint32_t ciaddr;   /* client IP */
    uint32_t yiaddr;   /* "your" IP (offered) */
    uint32_t siaddr;   /* server IP */
    uint32_t giaddr;   /* relay agent IP */
    uint8_t  chaddr[16]; /* client hardware address */
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;    /* 0x63825363 = DHCP magic cookie */
    uint8_t  options[312];
} __attribute__((packed)) dhcp_pkt_t;

/* ---- Estado da negociacao DHCP ----------------------------------- */
static volatile uint8_t  dhcp_got_offer = 0;
static volatile uint8_t  dhcp_got_ack   = 0;
static volatile uint32_t offered_ip     = 0;
static volatile uint32_t server_id      = 0;
static volatile uint32_t offered_gw     = 0;
static volatile uint32_t offered_mask   = 0;
static volatile uint32_t offered_dns    = 0;
static uint32_t xid = 0xDEADBEEF;

static dhcp_pkt_t dhcp_pkt;

/* ---- Montagem do pacote ------------------------------------------ */

static void build_discover(void) {
    k_memset(&dhcp_pkt, 0, sizeof(dhcp_pkt));

    dhcp_pkt.op    = 1;
    dhcp_pkt.htype = 1;
    dhcp_pkt.hlen  = 6;
    dhcp_pkt.xid   = htonl(xid);
    dhcp_pkt.flags = htons(0x8000);  /* broadcast flag */

    const uint8_t *mac = eth_get_mac();
    if (mac) k_memcpy(dhcp_pkt.chaddr, mac, 6);

    dhcp_pkt.magic = htonl(0x63825363);

    /* Opcoes */
    uint8_t *opt = dhcp_pkt.options;
    *opt++ = OPT_MSG_TYPE; *opt++ = 1; *opt++ = DHCP_DISCOVER;
    *opt++ = OPT_PARAM_REQ; *opt++ = 3;
    *opt++ = OPT_SUBNET_MASK; *opt++ = OPT_ROUTER; *opt++ = OPT_DNS;
    *opt++ = OPT_END;
}

static void build_request(void) {
    k_memset(&dhcp_pkt, 0, sizeof(dhcp_pkt));

    dhcp_pkt.op    = 1;
    dhcp_pkt.htype = 1;
    dhcp_pkt.hlen  = 6;
    dhcp_pkt.xid   = htonl(xid);
    dhcp_pkt.flags = htons(0x8000);

    const uint8_t *mac = eth_get_mac();
    if (mac) k_memcpy(dhcp_pkt.chaddr, mac, 6);

    dhcp_pkt.magic = htonl(0x63825363);

    uint8_t *opt = dhcp_pkt.options;
    *opt++ = OPT_MSG_TYPE; *opt++ = 1; *opt++ = DHCP_REQUEST;
    *opt++ = 50; *opt++ = 4;  /* requested IP */
    *opt++ = (uint8_t)(offered_ip >> 24); *opt++ = (uint8_t)(offered_ip >> 16);
    *opt++ = (uint8_t)(offered_ip >>  8); *opt++ = (uint8_t)(offered_ip);
    *opt++ = OPT_SERVER_ID; *opt++ = 4;
    *opt++ = (uint8_t)(server_id >> 24); *opt++ = (uint8_t)(server_id >> 16);
    *opt++ = (uint8_t)(server_id >>  8); *opt++ = (uint8_t)(server_id);
    *opt++ = OPT_END;
}

/* ---- Parsing da resposta ----------------------------------------- */

static void dhcp_handler(uint32_t src_ip, uint16_t src_port,
                          const uint8_t *data, uint16_t len) {
    (void)src_port;
    if (len < (uint16_t)sizeof(dhcp_pkt_t)) return;

    const dhcp_pkt_t *pkt = (const dhcp_pkt_t *)data;
    if (ntohl(pkt->xid) != xid) return;
    if (ntohl(pkt->magic) != 0x63825363) return;

    /* Parseia opcoes */
    const uint8_t *opt = pkt->options;
    const uint8_t *end = opt + 312;
    uint8_t msg_type = 0;

    while (opt < end && *opt != OPT_END) {
        uint8_t code = *opt++;
        if (code == 0) continue;  /* PAD */
        uint8_t olen = *opt++;

        switch (code) {
            case OPT_MSG_TYPE:   msg_type    = *opt; break;
            case OPT_SUBNET_MASK:
                offered_mask = ntohl(*(uint32_t *)opt); break;
            case OPT_ROUTER:
                offered_gw   = ntohl(*(uint32_t *)opt); break;
            case OPT_DNS:
                offered_dns  = ntohl(*(uint32_t *)opt); break;
            case OPT_SERVER_ID:
                server_id    = ntohl(*(uint32_t *)opt); break;
        }
        opt += olen;
    }

    if (msg_type == DHCP_OFFER && !dhcp_got_offer) {
        offered_ip    = ntohl(pkt->yiaddr);
        dhcp_got_offer = 1;
    } else if (msg_type == DHCP_ACK && dhcp_got_offer) {
        dhcp_got_ack = 1;
    }
    (void)src_ip;
}

/* ---- Init -------------------------------------------------------- */

int dhcp_init(void) {
    dhcp_got_offer = 0;
    dhcp_got_ack   = 0;
    offered_ip     = 0;
    server_id      = 0;

    udp_bind(DHCP_CLIENT_PORT, dhcp_handler);

    /* Envia DISCOVER */
    build_discover();
    udp_send(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT,
             (const uint8_t *)&dhcp_pkt, sizeof(dhcp_pkt));

    /* Aguarda OFFER */
    uint32_t start = sched_ticks();
    while (!dhcp_got_offer && (sched_ticks() - start) < 500) {
        eth_poll();
        __asm__ volatile ("pause");
    }
    if (!dhcp_got_offer) { udp_unbind(DHCP_CLIENT_PORT); return -1; }

    /* Envia REQUEST */
    build_request();
    udp_send(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT,
             (const uint8_t *)&dhcp_pkt, sizeof(dhcp_pkt));

    /* Aguarda ACK */
    start = sched_ticks();
    while (!dhcp_got_ack && (sched_ticks() - start) < 500) {
        eth_poll();
        __asm__ volatile ("pause");
    }

    udp_unbind(DHCP_CLIENT_PORT);

    if (!dhcp_got_ack) return -1;

    /* Configura rede global */
    net_our_ip     = offered_ip;
    net_gateway_ip = offered_gw;
    net_netmask    = offered_mask;
    net_dns_ip     = offered_dns;

    return 0;
}
