#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

//flags de uma entrada do page directory ou page table
#define PAGE_PRESENTE 0x001 //pag existente na RAM
#define PAGE_ESCRITA 0x002 // pagina que pode ser escrita
#define PAGE_USUARIO 0x004 // pag acessivel em ring 3

//inicia o paging e ativa o MMU
void paging_init(void);

//mapeia um end virtual para fisico no espaco do kernel
void paging_mapear(uint32_t virtual, uint32_t fisico, uint32_t flags);

#endif