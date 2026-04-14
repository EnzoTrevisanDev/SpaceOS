#include "netbuf.h"
#include "../disk/kstring.h"

/* Pool estatico em .bss — enderecos fisicos fixos para DMA */
static netbuf_t netbuf_pool[NETBUF_COUNT];
static netbuf_t *free_list = 0;
static int livre_count = 0;

void netbuf_init(void) {
    free_list = 0;
    for (int i = 0; i < NETBUF_COUNT; i++) {
        netbuf_pool[i].next = free_list;
        free_list = &netbuf_pool[i];
    }
    livre_count = NETBUF_COUNT;
}

netbuf_t *netbuf_alloc(void) {
    if (!free_list) return 0;
    netbuf_t *buf = free_list;
    free_list = buf->next;
    buf->next   = 0;
    buf->len    = 0;
    buf->offset = 0;
    livre_count--;
    return buf;
}

void netbuf_free(netbuf_t *buf) {
    if (!buf) return;
    buf->next = free_list;
    free_list = buf;
    livre_count++;
}

int netbuf_livres(void) { return livre_count; }
