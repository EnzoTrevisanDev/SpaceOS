#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include "netbuf.h"

#define ARP_CACHE_SIZE  16

/*
 * arp_resolve — resolve um IP para MAC via ARP.
 * Envia request e faz busy-poll ate receber reply ou timeout.
 * timeout_ms: tempo maximo de espera em millisegundos.
 * Retorna 0 e preenche mac_out em sucesso, -1 em timeout.
 */
int arp_resolve(uint32_t ip, uint8_t *mac_out, uint32_t timeout_ms);

/* arp_recv — chamado por eth_recv para processar frames ARP */
void arp_recv(netbuf_t *buf);

/* arp_gratuitous — anuncia nosso IP na rede (usado apos DHCP) */
void arp_gratuitous(void);

/* arp_cache_lookup — busca MAC no cache. Retorna 1 se encontrado. */
int arp_cache_lookup(uint32_t ip, uint8_t *mac_out);

#endif
