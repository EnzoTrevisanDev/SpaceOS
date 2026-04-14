#include "acpi.h"
#include "../cpu/io.h"
#include "../shell/shell.h"

/* ---- Estruturas ACPI (packed) ------------------------------------ */

typedef struct {
    char     signature[8];   /* "RSD PTR " (sem \0) */
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    /* ACPI 2.0+ campos abaixo */
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  ext_checksum;
    uint8_t  _reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

/* MADT (Multiple APIC Description Table) */
typedef struct {
    acpi_sdt_header_t hdr;   /* signature = "APIC" */
    uint32_t lapic_base;     /* endereco fisico do LAPIC */
    uint32_t flags;          /* bit 0 = dual 8259 presente */
} __attribute__((packed)) acpi_madt_t;

/* ---- Estado interno ---------------------------------------------- */

static uint32_t lapic_base_addr = 0xFEE00000;  /* fallback padrao */
static int acpi_initialized = 0;

/* ---- Helpers ----------------------------------------------------- */

static uint8_t acpi_checksum(const void *ptr, uint32_t len) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) sum += p[i];
    return sum;
}

/* Compara 4 ou 8 chars de assinatura */
static int sig4(const char *a, const char *b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}
static int sig8(const char *a, const char *b) {
    return sig4(a,b) && sig4(a+4,b+4);
}

/* ---- Inicializacao ----------------------------------------------- */

int acpi_init(void) {
    /* Scan da regiao EBDA + area ROM (0x80000-0xFFFFF) por "RSD PTR " */
    const char *scan_start = (const char *)0x000E0000;
    const char *scan_end   = (const char *)0x000FFFFF;

    acpi_rsdp_t *rsdp = 0;

    for (const char *p = scan_start; p < scan_end - 8; p += 16) {
        if (sig8(p, "RSD PTR ")) {
            acpi_rsdp_t *candidate = (acpi_rsdp_t *)p;
            if (acpi_checksum(candidate, 20) == 0) {
                rsdp = candidate;
                break;
            }
        }
    }

    if (!rsdp) {
        /* Tenta EBDA */
        uint16_t ebda_seg = *(volatile uint16_t *)0x040E;
        const char *ebda = (const char *)((uint32_t)ebda_seg << 4);
        for (const char *p = ebda; p < ebda + 1024 - 8; p += 16) {
            if (sig8(p, "RSD PTR ")) {
                acpi_rsdp_t *candidate = (acpi_rsdp_t *)p;
                if (acpi_checksum(candidate, 20) == 0) {
                    rsdp = candidate;
                    break;
                }
            }
        }
    }

    if (!rsdp) return -1;

    /* Parseia RSDT para encontrar MADT */
    acpi_sdt_header_t *rsdt = (acpi_sdt_header_t *)rsdp->rsdt_address;
    if (!sig4(rsdt->signature, "RSDT")) return -1;
    if (acpi_checksum(rsdt, rsdt->length) != 0) return -1;

    /* Entradas do RSDT: array de uint32_t logo apos o header */
    uint32_t *entries = (uint32_t *)(rsdt + 1);
    uint32_t n_entries = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;

    for (uint32_t i = 0; i < n_entries; i++) {
        acpi_sdt_header_t *tbl = (acpi_sdt_header_t *)entries[i];
        if (!tbl) continue;

        if (sig4(tbl->signature, "APIC")) {
            /* MADT encontrada */
            acpi_madt_t *madt = (acpi_madt_t *)tbl;
            lapic_base_addr = madt->lapic_base;
            acpi_initialized = 1;
            return 0;
        }
    }

    /* MADT nao encontrada, mas ACPI inicializou com fallback */
    acpi_initialized = 1;
    return 0;
}

void acpi_shutdown(void) {
    /*
     * Shutdown no QEMU: escreve 0x2000 na porta 0x604 (ACPI power management).
     * Em hardware real seria necessario parsear DSDT para obter SLP_TYPa/_S5_
     * e escrever nos registradores PM1a_CNT_BLK — nao implementado aqui.
     */
    outw(0x604, 0x2000);   /* QEMU shutdown */
    outw(0xB004, 0x2000);  /* Bochs/older QEMU */
    outw(0x4004, 0x3400);  /* VirtualBox */

    /* Se chegou aqui, nenhum dos metodos funcionou */
    while (1) __asm__ volatile ("hlt");
}

uint32_t acpi_lapic_base(void) {
    return lapic_base_addr;
}

int acpi_ready(void) {
    return acpi_initialized;
}
