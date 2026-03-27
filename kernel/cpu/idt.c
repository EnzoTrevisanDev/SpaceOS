/* kernel/cpu/idt.c */

#include "idt.h"

/* A tabela em si — 256 entradas de 8 bytes cada = 2KB na RAM */
static idt_entry_t idt[IDT_ENTRADAS];
static idt_ptr_t   idt_ptr;

/* Preenche uma entrada da IDT.
   num   = numero da interrupcao (0-255)
   base  = endereco da funcao que vai tratar essa interrupcao
   sel   = seletor de segmento (0x08 = segmento de codigo do kernel)
   flags = 0x8E significa: presente, ring 0, interrupt gate de 32 bits */
void idt_set(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;         /* pega os 16 bits baixos */
    idt[num].base_high = (base >> 16) & 0xFFFF; /* pega os 16 bits altos */
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

/* Inicializa a IDT — chamada la no kernel_main */
void idt_init(void) {
    idt_ptr.limite = (sizeof(idt_entry_t) * IDT_ENTRADAS) - 1;
    idt_ptr.base   = (uint32_t)idt;

    /* Zera todas as 256 entradas primeiro */
    for (int i = 0; i < IDT_ENTRADAS; i++) {
        idt_set(i, 0, 0, 0);
    }

    /* Carrega a IDT na CPU com a instrucao LIDT.
       Sem isso a CPU nao sabe que a tabela existe. */
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}