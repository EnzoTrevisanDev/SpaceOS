#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>

/*
 * DHCP cliente — obtém IP automaticamente.
 * Protocolo: DISCOVER → OFFER → REQUEST → ACK
 * UDP porta 67 (server) / 68 (client)
 *
 * Apos sucesso, preenche:
 *   net_our_ip, net_gateway_ip, net_netmask, net_dns_ip  (em ipv4.h)
 *
 * Retorna 0 em sucesso, -1 em timeout.
 */
int dhcp_init(void);

#endif
