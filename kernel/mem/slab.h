#ifndef SLAB_H
#define SLAB_H

#include<stdint.h>



// um cache slab - gerencia objs de um tamanho fixo
typedef struct slab_cache
{
    uint32_t  obj_size;      /* tamanho de cada objeto em bytes */
    uint32_t  objs_por_slab; /* quantos objetos cabem por pagina */
    void     *slab_lista;    /* lista de slabs com espaco livre */
    struct slab_cache *prox; /* proxima cache (lista global) */
} slab_cache_t;

/* Caches padrao — usadas pelo kernel e pelos comandos de teste */

extern slab_cache_t *cache_32;
extern slab_cache_t *cache_64;
extern slab_cache_t *cache_128;
extern slab_cache_t *cache_256;

// cria um cache para objs de tam fixo
slab_cache_t *slab_criar(uint32_t tamanho_objeto);

//aloca um obj do cache
void *slab_alloc(slab_cache_t *cache);

/* Libera um objeto de volta para a cache */
void slab_free(slab_cache_t *cache, void *ptr);

/* Inicializa o sistema de slabs com caches padrao */
void slab_init(void);

/* Mostra info das caches */
void slab_info(void);

#endif