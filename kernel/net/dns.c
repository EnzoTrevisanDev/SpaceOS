#include "dns.h"
#include "udp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "net_util.h"
#include "../disk/kstring.h"
#include "../proc/sched.h"

#define DNS_PORT     53
#define DNS_SRC_PORT 5353  /* porta local para queries */

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

static volatile uint32_t dns_result_ip = 0;
static volatile uint8_t  dns_got_reply = 0;
static uint16_t dns_query_id = 0x1234;

static uint8_t dns_buf[512];

/* ---- Codifica hostname no formato DNS ("www.google.com" → 3www6google3com0) */
static int dns_encode_name(uint8_t *dst, const char *hostname) {
    int pos = 0;
    const char *p = hostname;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int label_len = (int)(dot - p);
        if (label_len > 63) return -1;
        dst[pos++] = (uint8_t)label_len;
        while (p < dot) dst[pos++] = (uint8_t)*p++;
        if (*p == '.') p++;
    }
    dst[pos++] = 0;  /* raiz */
    return pos;
}

static void dns_handler(uint32_t src_ip, uint16_t src_port,
                         const uint8_t *data, uint16_t len) {
    (void)src_ip; (void)src_port;
    if (len < (uint16_t)sizeof(dns_header_t)) return;

    const dns_header_t *hdr = (const dns_header_t *)data;
    if (ntohs(hdr->id) != dns_query_id) return;
    if (!(ntohs(hdr->flags) & 0x8000)) return;  /* nao e resposta */

    uint16_t ancount = ntohs(hdr->ancount);
    if (!ancount) return;

    /* Pula o header e a question section */
    const uint8_t *p = data + sizeof(dns_header_t);
    const uint8_t *end = data + len;

    /* Pula a question: nome + qtype + qclass */
    while (p < end && *p) {
        if ((*p & 0xC0) == 0xC0) { p += 2; break; }
        p += *p + 1;
    }
    if (p < end && !*p) p++;  /* terminator */
    p += 4;  /* qtype + qclass */

    /* Parseia answers */
    for (uint16_t i = 0; i < ancount && p < end; i++) {
        /* Pula nome (pode ser ponteiro) */
        if ((*p & 0xC0) == 0xC0) p += 2;
        else { while (p < end && *p) { if ((*p&0xC0)==0xC0){p+=2;break;} p+=*p+1; } p++; }

        if (p + 10 > end) break;
        uint16_t rtype  = ntohs(*(uint16_t *)p); p += 2;
        uint16_t rclass = ntohs(*(uint16_t *)p); p += 2;
        p += 4;  /* TTL */
        uint16_t rdlen  = ntohs(*(uint16_t *)p); p += 2;

        if (rtype == 1 && rclass == 1 && rdlen == 4) {
            /* Tipo A, classe IN, 4 bytes = IPv4 */
            dns_result_ip = ntohl(*(uint32_t *)p);
            dns_got_reply = 1;
            return;
        }
        p += rdlen;
    }
}

int dns_resolve(const char *hostname, uint32_t *ip_out) {
    if (!net_dns_ip) return -1;

    dns_got_reply  = 0;
    dns_result_ip  = 0;
    dns_query_id++;

    /* Monta query DNS */
    dns_header_t *hdr = (dns_header_t *)dns_buf;
    hdr->id      = htons(dns_query_id);
    hdr->flags   = htons(0x0100);  /* RD = 1 (recursion desired) */
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    int name_len = dns_encode_name(dns_buf + sizeof(dns_header_t), hostname);
    if (name_len < 0) return -1;

    int pos = (int)sizeof(dns_header_t) + name_len;
    dns_buf[pos++] = 0x00; dns_buf[pos++] = 0x01;  /* QTYPE = A */
    dns_buf[pos++] = 0x00; dns_buf[pos++] = 0x01;  /* QCLASS = IN */

    udp_bind(DNS_SRC_PORT, dns_handler);
    udp_send(net_dns_ip, DNS_SRC_PORT, DNS_PORT, dns_buf, (uint16_t)pos);

    /* Aguarda resposta */
    uint32_t start = sched_ticks();
    while (!dns_got_reply && (sched_ticks() - start) < 300) {
        eth_poll();
        __asm__ volatile ("pause");
    }

    udp_unbind(DNS_SRC_PORT);

    if (!dns_got_reply) return -1;

    *ip_out = dns_result_ip;
    return 0;
}
