#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

//flasgs que indicam quais campos da estrutura sao validos
#define MULTIBOOT_FLAG_MEM 0x001 //mem_lower e mem_upper validos
#define MULTIBOOT_FLAG_MMAP 0x040 //mapa de memoria detalhado disponivel

//esturura principal GRUB para o kernel
//GRUB colo endereco dessa estrutura no registrador EBX
typedef struct{
    uint32_t flags;          /* quais campos abaixo sao validos */
    uint32_t mem_lower;      /* KB de RAM abaixo de 1MB */
    uint32_t mem_upper;      /* KB de RAM acima de 1MB */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;    /* tamanho total do mapa de memoria em bytes */
    uint32_t mmap_addr;      /* endereco do primeiro entry do mapa */
    /* ... outros campos que nao precisamos agora mas vou adicionar no futuro */
} __attribute__((packed)) multiboot_info_t;

/* entrada do mapa de memoria detalhado */
typedef struct {
    uint32_t size;           /* tamanho desta estrutura (sem contar esse campo) */
    uint32_t base_low;       /* endereco base — parte baixa */
    uint32_t base_high;      /* endereco base — parte alta (64-bit) */
    uint32_t length_low;     /* tamanho da regiao — parte baixa */
    uint32_t length_high;    /* tamanho da regiao — parte alta */
    uint32_t type;           /* 1 = disponivel, qualquer outro = reservado */
} __attribute__((packed)) mmap_entry_t;

#define MMAP_TYPE_DISPONIVEL 1   /* unico tipo que podemos usar... */


#endif
