#include "pmm.h"
#include "../multiboot.h"
#include "../shell/shell.h"

//simbolo definido pelo linker script - fim do kernel
extern uint32_t _end; //endereco logo apos o ultimo byte do kernel

// bitmap
//suporta ate 4GB de RAM = 1M de paginas = 128KB de bitmap
//o bitmap vem logo apos o kernel na memoria
#define MAX_PAGINAS (1024 * 256) //256k paginas = 1GB no max por enquanto...

static uint8_t bitmap[MAX_PAGINAS / 8];
static uint32_t total_paginas = 0;
static uint32_t paginas_livres = 0;

// operadores do bitmap

static void bit_setar(uint32_t pagina){
    bitmap[pagina / 8] |= (1 << (pagina % 8));
}

static void bit_limpar(uint32_t pagina){
    bitmap[pagina / 8] &= ~(1 << (pagina % 8));
}

static int bit_checar(uint32_t pagina){
    return bitmap[pagina / 8] & (1 << (pagina % 8));
}

// inicializacao

//marca um range de paginas como usadas
static void pmm_marcar_usado(uint32_t base,uint32_t tamanho){
    uint32_t pagina_inicio = base / PAGE_SIZE;
    uint32_t pagina_fim = (base + tamanho + PAGE_SIZE - 1 ) / PAGE_SIZE;

    for (uint32_t i = pagina_inicio; i < pagina_fim && i < total_paginas; i++){
        if (!bit_checar(i)) {
            bit_setar(i);
            if (paginas_livres > 0) paginas_livres--;
        }
    } 
}
//marca um range de paginas como livre
static void pmm_marcar_livre(uint32_t base, uint32_t tamanho) {
    uint32_t pagina_inicio = base / PAGE_SIZE;
    uint32_t pagina_fim    = (base + tamanho) / PAGE_SIZE;
    for (uint32_t i = pagina_inicio; i < pagina_fim && i < total_paginas; i++) {
        if (bit_checar(i)) {
            bit_limpar(i);
            paginas_livres++;
        }
    }

}



void pmm_init(uint32_t mboot_addr){
    multiboot_info_t *mboot = (multiboot_info_t *)mboot_addr;

    //passo 1 - descobre a qtd total de RAM em KB e converte em pag
    uint32_t ram_total_kb = mboot->mem_upper + 1024; //mem_upper e acima de 1MB
    total_paginas = (ram_total_kb * 1024) / PAGE_SIZE;
    if (total_paginas > MAX_PAGINAS) total_paginas = MAX_PAGINAS;

    //passo 2 - marca TUDO como usado - fail safe
    paginas_livres = 0;
    for (uint32_t i = 0; i < MAX_PAGINAS / 8; i++ ){
        bitmap[i] = 0xFF;
    }

    //passo 3 - libera apenas as regioes que o GRUB disse que estao disponeis
    if (mboot->flags & MULTIBOOT_FLAG_MMAP) {
        uint32_t offset = 0;
        while(offset < mboot->mmap_length){
            mmap_entry_t *entry = (mmap_entry_t*)(mboot->mmap_addr + offset);
            if(entry->type == MMAP_TYPE_DISPONIVEL && entry->base_high == 0){
                pmm_marcar_livre(entry->base_low, entry->length_low);
            }

            offset += entry->size + 4;
        }
    }else {
        // fallback: usa mem_upper se nao tem mapa detalhado
        pmm_marcar_livre(0x100000, mboot->mem_upper * 1024);
    }

    // passo 4 - marcar regioes criticas como usadas de volta
    pmm_marcar_usado(0x0, 0x100000); // primeiro 1MB - BIOS & HARDWARE
    pmm_marcar_usado(0x100000, (uint32_t)&_end - 0x100000 + PAGE_SIZE); //kernel em si
    pmm_marcar_usado((uint32_t)bitmap, MAX_PAGINAS / 8); // bitmap
    
    
    // mostra resultado no shell
    shell_write("[PMM] RAM Total: ");
    shell_write_num(ram_total_kb / 1024);
    shell_write("MB | Paginas livres");
    shell_write_num(paginas_livres);
    shell_write(" ");
}

// --- alloc e free ---
uint32_t pmm_alloc(void){
    for (uint32_t i = 0; i < total_paginas; i++){
        if(!bit_checar(i)){
            bit_setar(i);
            paginas_livres--;
            return i * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free(uint32_t endereco){
    uint32_t paginas = endereco / PAGE_SIZE;
    if(paginas < total_paginas && bit_checar(paginas)){
        bit_limpar(paginas);
        paginas_livres++;
    }
}

uint32_t pmm_livres(void) { return paginas_livres; }
uint32_t pmm_total(void) { return total_paginas; }


