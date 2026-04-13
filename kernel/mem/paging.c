#include "paging.h"
#include "pmm.h"
#include "../shell/shell.h"

// page directory do kernel - alinhado em 4KB!!
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

//page table para o primeiro 1GB do kernel
static uint32_t page_table_kernel[1024]__attribute__((aligned(4096)));

void paging_init(void){
    // passo 1 - zera o page directory inteiro!
    //todas as entradas marcadas como nao-presente
    for(int i = 0; i < 1024; i++){
        page_directory[i] = 0x00000002; // nao-presente + escrita permitida
    }

    // passo 2 - mapeia os primeiros 4MB identity mapped
    // Necessario para o kernel continuar rodando apos ativar paging,porque o codigo do kernel esta em enderecos fisicos baixos
    for (int i = 0; i < 1024; i++){
        page_table_kernel[i] = (i * 4096) | PAGE_PRESENTE | PAGE_ESCRITA;
    }
    //passo 3 - registra page table no page directory
    page_directory[0] = (uint32_t)page_table_kernel | PAGE_PRESENTE | PAGE_ESCRITA;

    //passo 4 - Convencao: kernel vive no topo do espaco virtual
    //Entrada 768 = 0xC0000000 / (1024 * 4096)
    page_directory[768] = (uint32_t)page_table_kernel | PAGE_PRESENTE | PAGE_ESCRITA;

    //passo 5 - carrega o page directory no CR3 e ativa paging no CR0
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(page_directory) : "eax"
    );

}

void paging_mapear(uint32_t virt, uint32_t fis, uint32_t flags){
    uint32_t pd_idx = virt >> 22; // bits 31-22
    uint32_t pt_idx = (virt >> 22) & 0x3FF; // bits 21-12

    //se page table nao existe, aloca nova
    if(!(page_directory[pd_idx] & PAGE_PRESENTE)){
        uint32_t nova_pt = pmm_alloc();
        if(!nova_pt) return; //sem memoria
    
    
        // zera a nova page table
        uint32_t *pt = (uint32_t *)nova_pt;
        for(int i = 0; i < 1024; i++) pt[i] = 0;

        page_directory[pt_idx] = nova_pt | PAGE_PRESENTE | PAGE_ESCRITA;
    }


    // pega o endereco da page table
    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & ~0xFFF);
    pt[pt_idx] = fis | flags | PAGE_PRESENTE;
}


void paging_mapear_usuario(uint32_t virt, uint32_t fis){
    paging_mapear(virt, fis, PAGE_PRESENTE | PAGE_ESCRITA | PAGE_USUARIO);
}

void paging_mapear_kernel(uint32_t virt, uint32_t fis){
    paging_mapear(virt, fis, PAGE_PRESENTE | PAGE_ESCRITA);
}
