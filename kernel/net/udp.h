#ifndef UDP_H
#define UDP_H

#include <stdint.h>
#include "netbuf.h"

#define UDP_HDR_LEN  8
#define UDP_MAX_HANDLERS  8

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;  /* opcional em IPv4 — usamos 0 */
} __attribute__((packed)) udp_header_t;

/* Callback para UDP recebido numa porta */
typedef void (*udp_handler_t)(uint32_t src_ip, uint16_t src_port,
                               const uint8_t *data, uint16_t len);

/*
 * udp_send — envia datagrama UDP.
 * src_port e dst_port: host order.
 * dst_ip: host order.
 */
int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
             const uint8_t *data, uint16_t len);

/*
 * udp_bind — registra handler para porta local.
 * Maximo UDP_MAX_HANDLERS handlers simultaneos.
 * Retorna 0 em sucesso.
 */
int  udp_bind(uint16_t port, udp_handler_t handler);
void udp_unbind(uint16_t port);

/* Chamado por ip_recv */
void udp_recv(netbuf_t *buf);

#endif
