#include "gdt.h"

static gdt_entry_t gdt[5];
static gdt_ptr_t gdt_ptr;

static void gdt_set(int num, uint32_t base, uint32_t limite, uint8_t acesso, uint8_t gran) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_mid    = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limite_low  = limite & 0xFFFF;
    gdt[num].granularidade = ((limite >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].acesso      = acesso;
}

void gdt_init(void) {
    gdt_ptr.limite = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_ptr.base   = (uint32_t)gdt;

    gdt_set(0, 0, 0,          0,    0);     /* nula — obrigatoria */
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* codigo kernel Ring 0 */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* dados kernel  Ring 0 */
    gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* codigo usuario Ring 3 */
    gdt_set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* dados usuario  Ring 3 */

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        : : "m"(gdt_ptr) : "eax"
    );
}

