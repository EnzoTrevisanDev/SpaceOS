#include "apic.h"
#include "../drivers/acpi.h"
#include "../mem/paging.h"
#include "../shell/shell.h"

static uint32_t lapic_base = 0xFEE00000;
static int apic_initialized = 0;

/* ---- Acesso MMIO ao LAPIC ---------------------------------------- */

static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *)(lapic_base + reg);
}

static inline void lapic_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(lapic_base + reg) = val;
}

/* ---- Inicializacao ----------------------------------------------- */

void apic_init(void) {
    /* Pega base do LAPIC do ACPI (ou usa fallback 0xFEE00000) */
    lapic_base = acpi_lapic_base();

    /*
     * Mapeia a pagina MMIO do LAPIC no espaco de enderecos do kernel.
     * identity-map: virt == fis (ambos 0xFEE00000).
     * PAGE_PRESENTE | PAGE_ESCRITA — sem PAGE_USUARIO (so Ring 0).
     */
    paging_mapear(lapic_base, lapic_base,
                  0x001 | 0x002);  /* PAGE_PRESENTE | PAGE_ESCRITA */

    /* Invalida TLB para o endereco mapeado */
    __asm__ volatile ("invlpg (%0)" : : "r"(lapic_base) : "memory");

    /* Habilita LAPIC: escreve no registrador Spurious Interrupt Vector
       bit 8 = Enable, bits 7-0 = vetor para interrupcoes spurious (0xFF) */
    lapic_write(LAPIC_SPURIOUS, LAPIC_SPURIOUS_ENABLE | LAPIC_SPURIOUS_VECTOR);

    /* Zera o TPR (Task Priority Register) — aceita todas as interrupcoes */
    lapic_write(LAPIC_TPR, 0);

    apic_initialized = 1;
}

void apic_eoi(void) {
    if (apic_initialized)
        lapic_write(LAPIC_EOI, 0);
}

uint8_t apic_id(void) {
    if (!apic_initialized) return 0;
    return (uint8_t)((lapic_read(LAPIC_ID) >> 24) & 0xFF);
}

int apic_ready(void) {
    return apic_initialized;
}

void apic_delay(uint32_t n) {
    /* Busy-wait simples — sem timer calibrado */
    for (volatile uint32_t i = 0; i < n * 1000; i++)
        __asm__ volatile ("nop");
}

/* ---- Envio de IPI (Inter-Processor Interrupt) -------------------- */

/*
 * Aguarda o LAPIC terminar de enviar o IPI anterior
 * (bit 12 do ICR_LO = Delivery Status — 1 = pendente)
 */
static void lapic_wait_ipi(void) {
    uint32_t timeout = 100000;
    while ((lapic_read(LAPIC_ICR_LO) & (1 << 12)) && timeout--)
        __asm__ volatile ("pause");
}

void apic_send_init(uint8_t apic_id_target) {
    if (!apic_initialized) return;

    /* ICR_HI: APIC ID do destino nos bits 31-24 */
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id_target << 24);

    /*
     * ICR_LO: INIT IPI
     *   bits 10-8 = 101 (INIT)
     *   bit 14    = 1   (Assert)
     *   bit 15    = 0   (Edge)
     */
    lapic_write(LAPIC_ICR_LO, ICR_INIT | ICR_LEVEL_ASSERT);
    lapic_wait_ipi();

    /* Deassert INIT */
    lapic_write(LAPIC_ICR_LO, ICR_INIT);
    lapic_wait_ipi();

    apic_delay(10);  /* espera ~10ms */
}

void apic_send_sipi(uint8_t apic_id_target, uint8_t vector) {
    if (!apic_initialized) return;

    /* Dois SIPIs conforme especificacao Intel */
    for (int i = 0; i < 2; i++) {
        lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id_target << 24);
        /*
         * ICR_LO: SIPI
         *   bits 10-8 = 110 (Startup)
         *   bits 7-0  = vector (pagina fisica = vector * 0x1000)
         */
        lapic_write(LAPIC_ICR_LO, ICR_SIPI | ICR_LEVEL_ASSERT | vector);
        lapic_wait_ipi();
        apic_delay(1);
    }
}
