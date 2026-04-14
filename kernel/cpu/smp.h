#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include "spinlock.h"

/* Forward declaration */
struct processo;

#define MAX_CPUS 4

typedef struct {
    uint8_t  apic_id;
    uint8_t  active;            /* 1 = nucleo online */
    uint32_t *stack;            /* stack dedicada do AP (16KB) */
    struct processo *current;   /* processo atual neste nucleo */
} cpu_info_t;

extern cpu_info_t cpus[MAX_CPUS];
extern int cpu_count;           /* total de nucleos detectados (BSP + APs) */

/*
 * smp_init — detecta APs via MADT, copia trampoline para 0x8000,
 *            envia INIT+SIPI para cada AP.
 *
 * APs inicializam em modo real, carregam GDT, entram em modo protegido
 * e chamam ap_main() em C (via boot/ap_trampoline.asm).
 *
 * BSP continua a execucao normal apos smp_init().
 */
void smp_init(void);

/*
 * smp_current_cpu — retorna indice do nucleo atual em cpus[].
 * Usa APIC ID para identificar o nucleo.
 */
uint8_t smp_current_cpu(void);

/*
 * ap_main — ponto de entrada C para APs apos trampoline.
 * Registra o AP em cpus[], entra em loop de trabalho.
 */
void ap_main(void);

#endif
