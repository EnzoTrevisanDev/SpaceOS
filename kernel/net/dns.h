#ifndef DNS_H
#define DNS_H

#include <stdint.h>

/*
 * DNS cliente — resolve hostname para IPv4.
 * Envia uma query UDP ao net_dns_ip:53 e aguarda resposta.
 * Sem cache, sem recursao complexa.
 *
 * Retorna 0 e preenche ip_out (host order) em sucesso.
 * Retorna -1 em timeout ou erro.
 */
int dns_resolve(const char *hostname, uint32_t *ip_out);

#endif
