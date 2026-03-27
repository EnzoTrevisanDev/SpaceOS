/* kernel/cpu/idt.h
   A IDT e uma tabela de 256 entradas.
   Cada entrada descreve o que fazer quando uma interrupcao acontece.
   O formato de cada entrada e ditado pela arquitetura x86 — nao tem como mudar. */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Cada entrada da IDT ocupa exatamente 8 bytes.
   O x86 exige esse layout especifico — cada campo tem posicao fixa. */
typedef struct {
    uint16_t base_low;    /* bits 0-15 do endereco da funcao handler  */
    uint16_t selector;    /* segmento de codigo — sempre 0x08 no nosso kernel */
    uint8_t  zero;        /* sempre zero, reservado pela arquitetura x86 */
    uint8_t  flags;       /* tipo e privilegio da interrupcao */
    uint16_t base_high;   /* bits 16-31 do endereco da funcao handler */
} __attribute__((packed)) idt_entry_t;
/* __attribute__((packed)) diz ao GCC: nao adicione padding entre os campos.
   Sem isso o compilador pode alinhar os campos e quebrar o layout exigido pelo x86. */

/* O ponteiro que a instrucao LIDT carrega — diz a CPU onde a IDT esta na memoria */
typedef struct {
    uint16_t limite;   /* tamanho da IDT em bytes - 1 */
    uint32_t base;     /* endereco da IDT na RAM */
} __attribute__((packed)) idt_ptr_t;

/* 256 entradas — uma para cada possivel interrupcao do x86 */
#define IDT_ENTRADAS 256

void idt_init(void);
void idt_set(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif