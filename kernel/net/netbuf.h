#ifndef NETBUF_H
#define NETBUF_H

#include <stdint.h>

/*
 * Pool de buffers de rede — alocados estaticamente em .bss.
 * Fisicamente contiguos (necessario para DMA do RTL8139).
 * 16 buffers * 1536 bytes = 24KB no .bss.
 */

#define NETBUF_SIZE   1536   /* MTU 1500 + header Ethernet 14 + margem */
#define NETBUF_COUNT  16

typedef struct netbuf {
    uint8_t  data[NETBUF_SIZE];
    uint16_t len;             /* bytes validos em data[] */
    uint16_t offset;          /* offset do payload (pula headers das camadas abaixo) */
    struct netbuf *next;      /* proximo na lista livre */
} netbuf_t;

/* Inicializa o pool — chamar uma vez antes de qualquer rede */
void netbuf_init(void);

/* Retira um buffer do pool. Retorna NULL se esgotado. */
netbuf_t *netbuf_alloc(void);

/* Devolve buffer ao pool */
void netbuf_free(netbuf_t *buf);

/* Numero de buffers livres (para debug) */
int netbuf_livres(void);

#endif
