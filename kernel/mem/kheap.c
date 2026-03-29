#include "kheap.h"
#include "pmm.h"
#include "../shell/shell.h"

//cabeçalho de cada broco na heap - fica antes dos dados do bloco na memoria
typedef struct bloco {
    uint32_t tamanho; // tamanho dos dados - nao inclui cabecalho
    uint8_t livre; // 1 = livre 0 = usado
    struct bloco *proximo; // proximo bloco da lista
} __attribute__((packed)) bloco_t;


#define CABECALHO sizeof(bloco_t)
#define HEAP_INICIO 0x00300000 // 4MB - apos o kernel
#define HEAP_PAGINAS 64 //64 paginas = 256KB, cabe nos 4MB



static bloco_t *heap_inicio = 0;
static uint8_t inicializado = 0;

//aloca pags fisicas e mapeia na heap
static void heap_expandir(uint32_t paginas){
    for(uint32_t i = 0; i < paginas; i++){
        uint32_t fis = pmm_alloc();
        if(!fis) return;
        
        // mapeia virtual -> fisico na regiao da heap
        uint32_t virt = HEAP_INICIO + (i * 4096);

        //identity map simples...For now
        volatile uint32_t *page = (volatile uint32_t *)virt;
        (void)page;
    }
}

/*void kheap_init(void){
    //alocar paginas para a heap usando PMM
    uint32_t total_bytes = HEAP_PAGINAS * 4096;

    //primeiro bloco ocupar toda a heap - livre
    heap_inicio = (bloco_t *)HEAP_INICIO;

    //identity mapping dos primeiros 4MB
    for (uint32_t i = 0; i < HEAP_PAGINAS; i++){
        uint32_t fis = pmm_alloc();
        if(!fis) break;

        //por enquando identidade simples -- fut paging_mapear()
        uint32_t virt = HEAP_INICIO + (i * 4096);
        extern void paging_mapear(uint32_t, uint32_t,uint32_t);
        paging_mapear(virt, fis, 0x003);
    }

    //invalida o TLB para que o CPU use os novos mapeamentos
    __asm__ volatile(
        "mov %%cr3, %%eax\n"
        "mov %%eax, %%cr3\n"
        : : : "eax"
    );

    //inicia o primeiro bloco - livre, ocupa tudo!
    heap_inicio->tamanho = total_bytes - CABECALHO;
    heap_inicio->livre = 1;
    heap_inicio->proximo = 0;
    
    inicializado = 1;
}*/
void kheap_init(void) {
    uint32_t total_bytes = HEAP_PAGINAS * 4096;

    heap_inicio = (bloco_t *)HEAP_INICIO;
    heap_inicio->tamanho = total_bytes - CABECALHO;
    heap_inicio->livre   = 1;
    heap_inicio->proximo = 0;

    inicializado = 1;
}
void *kmalloc(uint32_t tamanho) {
    if (!inicializado || tamanho == 0) return 0;

    /* Alinha tamanho em 4 bytes */
    tamanho = (tamanho + 3) & ~3;

    bloco_t *atual = heap_inicio;

    while (atual) {
        /* Encontrou bloco livre suficientemente grande */
        if (atual->livre && atual->tamanho >= tamanho) {

            /* Vale a pena dividir? So divide se sobrar espaco
               para um novo cabecalho + pelo menos 8 bytes */
            if (atual->tamanho >= tamanho + CABECALHO + 8) {
                /* Cria novo bloco com o espaco restante */
                bloco_t *novo = (bloco_t *)((uint8_t *)atual + CABECALHO + tamanho);
                novo->tamanho  = atual->tamanho - tamanho - CABECALHO;
                novo->livre    = 1;
                novo->proximo  = atual->proximo;

                atual->tamanho  = tamanho;
                atual->proximo  = novo;
            }

            atual->livre = 0;

            /* Retorna ponteiro para os DADOS — logo apos o cabecalho */
            return (void *)((uint8_t *)atual + CABECALHO);
        }

        atual = atual->proximo;
    }

    return 0;   /* sem memoria */
}

void kfree(void *ptr) {
    if (!ptr) return;

    /* O cabecalho fica imediatamente antes do ponteiro */
    bloco_t *bloco = (bloco_t *)((uint8_t *)ptr - CABECALHO);
    bloco->livre = 1;

    /* Coalescing — junta blocos livres vizinhos
       Evita fragmentacao da heap */
    bloco_t *atual = heap_inicio;
    while (atual && atual->proximo) {
        if (atual->livre && atual->proximo->livre) {
            /* Absorve o proximo bloco */
            atual->tamanho += CABECALHO + atual->proximo->tamanho;
            atual->proximo  = atual->proximo->proximo;
        } else {
            atual = atual->proximo;
        }
    }
}
