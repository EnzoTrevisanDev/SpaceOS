#ifndef PMM_H
#define PMM_H

#include <stdint.h>


#define PAGE_SIZE 4096 // cada pag tem 4KB

// inicia o PMM lendo o mapa da memoria do grub mboot_addr = end de estrutura do multiboot passada pelo grub
void pmm_init(uint32_t mboot_addr);

//aloca uma pagina fisica - retorna o addrs ou 0 se nao tiver
uint32_t pmm_alloc(void);

//libera uma pagina fisica
void pmm_free(uint32_t endereco);

//retorna qtds de pags estao livres

uint32_t pmm_livres(void);

//retorna qtds de pags que exitem no total

uint32_t pmm_total(void);

#endif