#include "slab.h"
#include "pmm.h"
#include "kheap.h"
#include "../shell/shell.h"

//cabecalho de cada slab - inicio da pagina
typedef struct slab{
    uint32_t livre_count; /* quantos objetos livres neste slab */
    uint32_t *free_list; /* ponteiro para o proximo slot livre */
    struct slab *proximo; /* proximo slab da cache */
    uint8_t dados[]; /* objetos comecam aqui */  
} slab_t;

#define SLAB_CABECALO sizeof(slab_t)

//lista global de toadas as caches criadas
static slab_cache_t *caches = 0;

//caches padrao do kernel
slab_cache_t *cache_32;
slab_cache_t *cache_64;
slab_cache_t *cache_128;
slab_cache_t *cache_256;
//cria um novo slab para um cache e adiciona na lista
static slab_t *slab_novo(slab_cache_t *cache){
    //aloca uma pagina fisica para o slab
    uint32_t fis = pmm_alloc();
    if(!fis) return 0;

    //mapeia a pagina - identity map nos primeiro 4MB
    //futuro - paging_mapear para endd acima
    slab_t *slab = (slab_t *)fis;

    //calcula quantos objs cabem no cabecalho
    uint32_t espaco = 4096 - SLAB_CABECALO;
    cache->objs_por_slab = espaco / cache->obj_size;
    slab->livre_count = cache->objs_por_slab;
    slab->proximo = 0;
    
    //inicia free list - lista encadeada dentros dos proprios slots
    //cada slot livre contem o end do proximo slot livre
    uint8_t *base = (uint8_t *)slab + SLAB_CABECALO;
    slab->free_list = (uint32_t *)base;

    for (uint32_t i = 0; i < cache->objs_por_slab - 1; i++){
        uint32_t *slot_atual = (uint32_t *)(base + i * cache->obj_size);
        uint32_t *slot_prox = (uint32_t *)(base + (i + 1) * cache->obj_size);
        *slot_atual = (uint32_t)slot_prox; //aponta para o proximo
    }

    //ultimo slot aponta para NULL
    uint32_t *ultimo = (uint32_t *)(base + (cache->objs_por_slab - 1) * cache->obj_size);
    *ultimo = 0;

    return slab;
}

slab_cache_t *slab_criar(uint32_t tamanho){
    //tamanho minimo de 8 bytes - precisa guardar o ponteiro dentro do slot
    if (tamanho < 8) tamanho = 8;

    //alinha para muiltiplo de 4
    tamanho = (tamanho + 3) & ~3;

    slab_cache_t *cache = kmalloc(sizeof(slab_cache_t));
    if(!cache) return 0;

    cache->obj_size = tamanho;
    cache->objs_por_slab = 0;
    cache->slab_lista = 0;
    cache->prox = caches;
    caches = cache;

    //cria o primeiro slab imediatamente
    cache->slab_lista = slab_novo(cache);
    return cache;
}

void *slab_alloc(slab_cache_t *cache){
    if(!cache) return 0;

    slab_t *slab = (slab_t *)cache->slab_lista;

    //procura um slab com espaco livre
    while (slab && slab->livre_count == 0){
        slab = slab->proximo;
    }
    //nenhum slab tem espaco - cria um novo
    if(!slab) {
        slab = slab_novo(cache);
        if(!slab) return 0;

        //adiciona no inicio da lista
        slab->proximo = (slab_t *)cache->slab_lista;
        cache->slab_lista = slab;
    }
    
    //pega o primeiro slot da free list
    void *obj = (void *)slab->free_list;
    slab->free_list = (uint32_t *)(*slab->free_list); //avanca free list
    slab->livre_count--;

    return obj;
}

void slab_free(slab_cache_t *cache, void *ptr){
    if (!cache || !ptr) return;

    /* Encontra qual slab contem esse objeto
       O slab esta no inicio da pagina de 4KB que contem o objeto */
    slab_t *slab = (slab_t *)((uint32_t)ptr & ~0xFFF);

    /* Devolve o slot para o inicio da free list */
    uint32_t *slot  = (uint32_t *)ptr;
    *slot           = (uint32_t)slab->free_list;
    slab->free_list = slot;
    slab->livre_count++;

}

void slab_init(void) {
    /* Cria caches padrao para os tamanhos mais comuns no kernel */
    cache_32  = slab_criar(32);
    cache_64  = slab_criar(64);
    cache_128 = slab_criar(128);
    cache_256 = slab_criar(256);
}

void slab_info(void) {
    shell_writeln("=== Slab caches ativas ===");
    slab_cache_t *c = caches;
    while (c) {
        shell_write("  Cache ");
        shell_write_num(c->obj_size);
        shell_write("B — ");
        shell_write_num(c->objs_por_slab);
        shell_writeln(" objs/slab");
        c = c->prox;
    }
}