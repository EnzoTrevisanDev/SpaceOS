#include "smp.h"
#include "apic.h"
#include "../drivers/acpi.h"
#include "../mem/kheap.h"
#include "../shell/shell.h"

/* ---- Estruturas MADT para enumerar APs --------------------------- */

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_hdr_t;

/* Tipo 0: Processor Local APIC */
typedef struct {
    uint8_t  type;       /* 0 */
    uint8_t  length;     /* 8 */
    uint8_t  acpi_id;
    uint8_t  apic_id;
    uint32_t flags;      /* bit 0 = enabled */
} __attribute__((packed)) madt_lapic_t;

typedef struct {
    char     signature[4]; /* "APIC" */
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t lapic_base;
    uint32_t flags;
} __attribute__((packed)) madt_table_t;

/* ---- Trampoline de 16 bits (AP startup) -------------------------- */

/*
 * O trampoline e uma sequencia de bytes que o AP executa em modo real
 * apos receber o SIPI. Ele:
 *   1. Configura segmentos
 *   2. Carrega o GDT do kernel
 *   3. Habilita modo protegido
 *   4. Salta para ap_main via um ponteiro em 0x8FF0
 *
 * Gerado por: nasm -f bin boot/ap_trampoline.asm -o boot/ap_trampoline.bin
 * e convertido para array com: xxd -i boot/ap_trampoline.bin
 *
 * TODO: compilar e embutir o trampoline real.
 * Por enquanto, APs recebem SIPI mas ficam em halt (sem trampoline valido).
 */
static const uint8_t ap_trampoline[] = {
    /* HLT loop — placeholder ate trampoline real ser compilado */
    0xFA,        /* cli */
    0xF4,        /* hlt */
    0xEB, 0xFC,  /* jmp $ (loop infinito) */
};

/* ---- Estado global ----------------------------------------------- */

cpu_info_t cpus[MAX_CPUS];
int cpu_count = 1;  /* BSP ja conta como 1 */

/* Lock do scheduler (protege a fila de prontos) */
extern spinlock_t sched_lock;

/* ---- AP entry point ---------------------------------------------- */

void ap_main(void) {
    uint8_t my_apic_id = apic_id();

    /* Registra este AP na tabela de CPUs */
    for (int i = 1; i < MAX_CPUS; i++) {
        if (cpus[i].apic_id == my_apic_id && !cpus[i].active) {
            cpus[i].active = 1;
            break;
        }
    }

    /* Habilita LAPIC neste nucleo */
    apic_init();

    /* Loop de trabalho — por enquanto apenas idle */
    while (1) {
        __asm__ volatile ("pause");
        __asm__ volatile ("hlt");
    }
}

/* ---- Inicializacao SMP ------------------------------------------- */

static void parsear_madt(void) {
    /*
     * Localiza MADT via RSDP (ja parseado em acpi_init).
     * Scan das entradas tipo 0 (Processor Local APIC) para encontrar APs.
     */
    uint32_t rsdp_scan_start = 0x000E0000;
    uint32_t rsdp_scan_end   = 0x000FFFFF;

    madt_table_t *madt = 0;
    const char *p;

    /* Busca RSDP */
    for (p = (const char *)rsdp_scan_start; p < (const char *)rsdp_scan_end; p += 16) {
        if (p[0]=='R' && p[1]=='S' && p[2]=='D' && p[3]==' ' &&
            p[4]=='P' && p[5]=='T' && p[6]=='R' && p[7]==' ') {
            uint32_t rsdt_addr = *(uint32_t *)(p + 16);
            uint32_t *rsdt_entries = (uint32_t *)(rsdt_addr + 36);
            uint32_t rsdt_len = *(uint32_t *)(rsdt_addr + 4);
            uint32_t n = (rsdt_len - 36) / 4;

            for (uint32_t i = 0; i < n && i < 32; i++) {
                uint32_t tbl = rsdt_entries[i];
                if (!tbl) continue;
                const char *sig = (const char *)tbl;
                if (sig[0]=='A' && sig[1]=='P' && sig[2]=='I' && sig[3]=='C') {
                    madt = (madt_table_t *)tbl;
                    break;
                }
            }
            break;
        }
    }

    if (!madt) return;

    /* Parseia entradas do MADT */
    uint8_t bsp_apic_id = apic_id();
    const uint8_t *entry = (const uint8_t *)madt + sizeof(madt_table_t);
    const uint8_t *end   = (const uint8_t *)madt + madt->length;

    while (entry < end) {
        madt_entry_hdr_t *hdr = (madt_entry_hdr_t *)entry;
        if (hdr->type == 0 && hdr->length >= 8) {
            madt_lapic_t *lapic = (madt_lapic_t *)entry;
            /* bit 0 = enabled, nao e o BSP */
            if ((lapic->flags & 1) && lapic->apic_id != bsp_apic_id) {
                if (cpu_count < MAX_CPUS) {
                    cpus[cpu_count].apic_id = lapic->apic_id;
                    cpus[cpu_count].active  = 0;
                    cpus[cpu_count].current = 0;
                    cpu_count++;
                }
            }
        }
        if (hdr->length == 0) break;
        entry += hdr->length;
    }
}

void smp_init(void) {
    /* Inicializa BSP na posicao 0 */
    cpus[0].apic_id = apic_id();
    cpus[0].active  = 1;
    cpus[0].current = 0;
    cpus[0].stack   = 0;

    for (int i = 1; i < MAX_CPUS; i++) {
        cpus[i].apic_id = 0;
        cpus[i].active  = 0;
        cpus[i].current = 0;
        cpus[i].stack   = 0;
    }

    if (!apic_ready()) {
        shell_writeln("SMP: APIC nao inicializado, skipping");
        return;
    }

    /* Detecta APs via MADT */
    parsear_madt();

    if (cpu_count <= 1) {
        shell_writeln("SMP: sistema single-core");
        return;
    }

    /* Copia trampoline para 0x8000 (abaixo de 1MB, necessario para real mode) */
    uint8_t *tramp_dest = (uint8_t *)0x8000;
    for (uint32_t i = 0; i < sizeof(ap_trampoline); i++)
        tramp_dest[i] = ap_trampoline[i];

    /* Envia INIT+SIPI para cada AP */
    for (int i = 1; i < cpu_count; i++) {
        shell_write("SMP: iniciando AP APIC_ID=");
        shell_write_num(cpus[i].apic_id);
        shell_writeln("...");

        apic_send_init(cpus[i].apic_id);
        apic_send_sipi(cpus[i].apic_id, 0x08);  /* vector 0x08 → 0x8000 */
    }

    /* Aguarda APs (timeout simples) */
    apic_delay(100);

    int online = 0;
    for (int i = 0; i < cpu_count; i++)
        if (cpus[i].active) online++;

    shell_write("SMP: ");
    shell_write_num((uint32_t)online);
    shell_write("/");
    shell_write_num((uint32_t)cpu_count);
    shell_writeln(" nucleos online");
}

uint8_t smp_current_cpu(void) {
    uint8_t my_id = apic_id();
    for (int i = 0; i < cpu_count; i++) {
        if (cpus[i].apic_id == my_id) return (uint8_t)i;
    }
    return 0;  /* fallback: BSP */
}
