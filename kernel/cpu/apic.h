#ifndef APIC_H
#define APIC_H

#include <stdint.h>

/*
 * APIC — Advanced Programmable Interrupt Controller
 *
 * Local APIC (LAPIC): um por nucleo, acesso via MMIO em lapic_base.
 * Usado para: EOI de interrupcoes, envio de IPI (inter-processor interrupt),
 *             timer por CPU (futuro).
 */

/* Offsets dos registradores LAPIC (relativos a base MMIO) */
#define LAPIC_ID          0x020   /* ID do LAPIC */
#define LAPIC_VERSION     0x030
#define LAPIC_TPR         0x080   /* Task Priority Register */
#define LAPIC_EOI         0x0B0   /* End Of Interrupt (escrever 0 para ACK) */
#define LAPIC_SPURIOUS    0x0F0   /* Spurious Interrupt Vector Register */
#define LAPIC_ICR_LO      0x300   /* Interrupt Command Register (low) */
#define LAPIC_ICR_HI      0x310   /* Interrupt Command Register (high) */
#define LAPIC_TIMER_LVT   0x320   /* Local Vector Table — Timer */
#define LAPIC_TIMER_INIT  0x380   /* Timer Initial Count */
#define LAPIC_TIMER_CUR   0x390   /* Timer Current Count */
#define LAPIC_TIMER_DIV   0x3E0   /* Timer Divide Configuration */

/* Flags do ICR para envio de IPI */
#define ICR_INIT          0x00000500
#define ICR_SIPI          0x00000600
#define ICR_LEVEL_ASSERT  0x00004000
#define ICR_DEST_FIELD    0x00000000

/* Spurious vector: bit 8 habilita APIC, bits 7-0 = numero da interrupcao */
#define LAPIC_SPURIOUS_ENABLE  0x100
#define LAPIC_SPURIOUS_VECTOR  0xFF   /* interrupcao 255 = spurious */

/*
 * apic_init — habilita o LAPIC no BSP.
 *   - Mapeia o MMIO do LAPIC via paging_mapear
 *   - Habilita LAPIC escrevendo no registrador SPURIOUS
 *   - PIC (8259) permanece ativo para teclado (IRQ1)
 */
void apic_init(void);

/* Envia EOI ao LAPIC (deve ser chamado no fim de toda ISR do LAPIC) */
void apic_eoi(void);

/*
 * apic_send_init + apic_send_sipi — sequencia de startup de AP.
 *   apic_id: APIC ID do AP alvo (obtido do MADT).
 *   vector:  pagina fisica / 0x1000 (ex: 0x08 para 0x8000).
 */
void apic_send_init(uint8_t apic_id);
void apic_send_sipi(uint8_t apic_id, uint8_t vector);

/* Retorna APIC ID do nucleo atual */
uint8_t apic_id(void);

/* Retorna 1 se APIC foi inicializado */
int apic_ready(void);

/* Espera em loops (~microsegundos) sem precisar de timer */
void apic_delay(uint32_t n);

#endif
