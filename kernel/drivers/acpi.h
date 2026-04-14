#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

/*
 * ACPI — Advanced Configuration and Power Interface (minimo)
 *
 * Implementado:
 *   - RSDP scan (localiza tabelas ACPI)
 *   - MADT parse (encontra base do LAPIC para SMP)
 *   - Shutdown via porta QEMU (0x604)
 *
 * Nao implementado (futuro):
 *   - DSDT/AML interpreter (necessario para shutdown em hardware real)
 *   - FADT/FACP para sleep states
 */

/*
 * acpi_init — localiza RSDP em 0xE0000-0xFFFFF e parseia MADT.
 * Retorna 0 se ACPI encontrado, -1 se nao.
 */
int acpi_init(void);

/*
 * acpi_shutdown — desliga o sistema.
 *   QEMU: escreve 0x2000 na porta 0x604 (nao retorna).
 *   Hardware real: precisaria parsear _S5 no DSDT (nao implementado).
 */
void acpi_shutdown(void);

/*
 * acpi_lapic_base — retorna endereco fisico da base do LAPIC.
 * (extraido do MADT durante acpi_init)
 * Retorna 0xFEE00000 como fallback se MADT nao encontrado.
 */
uint32_t acpi_lapic_base(void);

/* Retorna 1 se ACPI foi inicializado com sucesso */
int acpi_ready(void);

#endif
