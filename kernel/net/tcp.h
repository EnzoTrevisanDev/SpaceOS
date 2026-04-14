#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include "netbuf.h"

#define TCP_MAX_SOCKETS  4
#define TCP_RX_BUF_SIZE  2048

/* Estados TCP */
#define TCP_CLOSED       0
#define TCP_SYN_SENT     1
#define TCP_ESTABLISHED  2
#define TCP_FIN_WAIT     3
#define TCP_CLOSE_WAIT   4

typedef struct {
    uint32_t remote_ip;     /* host order */
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;       /* nosso numero de sequencia */
    uint32_t ack_num;       /* proximo byte esperado do remoto */
    uint8_t  state;
    uint8_t  rx_buf[TCP_RX_BUF_SIZE];
    uint16_t rx_head;
    uint16_t rx_tail;       /* rx_buf[rx_head..rx_tail) = dados disponiveis */
} tcp_socket_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  /* bits 7-4 = offset em 4 bytes; bits 3-0 = flags */
    uint8_t  flags;        /* SYN=0x02 ACK=0x10 FIN=0x01 RST=0x04 PSH=0x08 */
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

#define TCP_HDR_LEN  20
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

/*
 * tcp_connect — conecta a remote_ip:port.
 * Retorna indice do socket (>=0) ou -1 em falha.
 */
int  tcp_connect(uint32_t ip, uint16_t port);

/*
 * tcp_send — envia dados pelo socket.
 * Retorna bytes enviados ou -1.
 */
int  tcp_send(int sock, const uint8_t *data, uint16_t len);

/*
 * tcp_recv — recebe dados (busy-poll).
 * timeout_ms: 0 = sem espera.
 * Retorna bytes recebidos ou -1 em erro/fechado.
 */
int  tcp_recv(int sock, uint8_t *buf, uint16_t max_len, uint32_t timeout_ms);

/* tcp_close — envia FIN e fecha o socket */
void tcp_close(int sock);

/* Chamado por ip_recv */
void tcp_recv_dispatch(netbuf_t *buf);

#endif
