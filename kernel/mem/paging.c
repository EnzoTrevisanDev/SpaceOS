#include "paging.h"
#include "pmm.h"
#include "../shell/shell.h"

// page directory do kernel - alinhado em 4KB!!
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

/* Identity-map primeiros 64MB: 16 page tables × 4MB cada = 64KB de BSS */
#define PT_COUNT 16
static uint32_t page_tables[PT_COUNT][1024] __attribute__((aligned(4096)));

void paging_init(void){
    /* Zera o page directory */
    for(int i = 0; i < 1024; i++){
        page_directory[i] = 0x00000002; /* nao-presente + escrita permitida */
    }

    /* Identity-map os primeiros 64MB (cobre RAM fisica + ACPI tables do QEMU) */
    for (int pt = 0; pt < PT_COUNT; pt++) {
        for (int i = 0; i < 1024; i++) {
            page_tables[pt][i] = (uint32_t)((pt * 1024 + i) * 4096)
                                 | PAGE_PRESENTE | PAGE_ESCRITA;
        }
        page_directory[pt] = (uint32_t)page_tables[pt] | PAGE_PRESENTE | PAGE_ESCRITA;
    }

    /* Convencao: kernel vive tambem em 0xC0000000 (entrada 768) */
    page_directory[768] = (uint32_t)page_tables[0] | PAGE_PRESENTE | PAGE_ESCRITA;

    /* Carrega CR3 e ativa paging no CR0 */
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(page_directory) : "eax"
    );
}

void paging_mapear(uint32_t virt, uint32_t fis, uint32_t flags){
    uint32_t pd_idx = virt >> 22;            /* bits 31-22 */
    uint32_t pt_idx = (virt >> 12) & 0x3FF; /* bits 21-12 */

    /* Se page table nao existe, aloca nova via PMM */
    if(!(page_directory[pd_idx] & PAGE_PRESENTE)){
        uint32_t nova_pt = pmm_alloc();
        if(!nova_pt) return;

        uint32_t *pt = (uint32_t *)nova_pt;
        for(int i = 0; i < 1024; i++) pt[i] = 0;

        page_directory[pd_idx] = nova_pt | PAGE_PRESENTE | PAGE_ESCRITA;
    }

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & ~0xFFF);
    pt[pt_idx] = (fis & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENTE;
}


void paging_mapear_usuario(uint32_t virt, uint32_t fis){
    paging_mapear(virt, fis, PAGE_PRESENTE | PAGE_ESCRITA | PAGE_USUARIO);
}

void paging_mapear_kernel(uint32_t virt, uint32_t fis){
    paging_mapear(virt, fis, PAGE_PRESENTE | PAGE_ESCRITA);
}
