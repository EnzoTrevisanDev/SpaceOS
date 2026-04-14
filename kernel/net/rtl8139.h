#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include "netbuf.h"

/*
 * Driver RTL8139 — chipset Ethernet classico, emulado no QEMU.
 * PCI: vendor=0x10EC, device=0x8139
 *
 * Receive: polling do registrador ISR (sem interrupcao).
 * Transmit: round-robin entre 4 descritores TX.
 *
 * Para usar com QEMU:
 *   -device rtl8139,netdev=net0 -netdev user,id=net0
 */

/* PCI IDs */
#define RTL8139_VENDOR  0x10EC
#define RTL8139_DEVICE  0x8139

/* Callback chamado para cada frame Ethernet recebido */
typedef void (*net_recv_cb_t)(netbuf_t *buf);

/*
 * rtl8139_init — detecta via PCI, configura RX ring, habilita TX/RX.
 * Retorna 0 em sucesso, -1 se nao encontrado.
 */
int rtl8139_init(void);

/*
 * rtl8139_send — transmite um frame Ethernet (header ja incluido).
 * data: frame completo; len: tamanho em bytes (max 1500 payload + 14 header).
 * Retorna 0 em sucesso, -1 em erro.
 */
int rtl8139_send(const uint8_t *data, uint16_t len);

/*
 * rtl8139_poll — verifica se chegou algum frame e processa.
 * Deve ser chamado periodicamente (no loop de DHCP, DNS, TCP recv).
 * Retorna numero de frames processados.
 */
int rtl8139_poll(void);

/* Registra callback para frames recebidos */
void rtl8139_set_recv_cb(net_recv_cb_t cb);

/* Retorna MAC address do dispositivo (6 bytes) */
const uint8_t *rtl8139_get_mac(void);

/* Retorna 1 se inicializado */
int rtl8139_ready(void);

#endif
