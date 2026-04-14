#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <stdint.h>

/*
 * Conversao de byte order: x86 e little-endian, rede e big-endian.
 * htons/ntohs: host ↔ network para 16 bits
 * htonl/ntohl: host ↔ network para 32 bits
 */

static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline uint16_t ntohs(uint16_t v) { return htons(v); }

static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF000000) >> 24)
         | ((v & 0x00FF0000) >>  8)
         | ((v & 0x0000FF00) <<  8)
         | ((v & 0x000000FF) << 24);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

/* Constroi um IPv4 de 4 octetos */
#define MAKE_IP(a, b, c, d) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

/* Extrai octetos de um IPv4 (host order) */
#define IP_A(ip) (((ip) >> 24) & 0xFF)
#define IP_B(ip) (((ip) >> 16) & 0xFF)
#define IP_C(ip) (((ip) >>  8) & 0xFF)
#define IP_D(ip) (((ip)      ) & 0xFF)

#endif
