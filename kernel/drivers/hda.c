#include "hda.h"
#include "pci.h"
#include "../mem/paging.h"
#include "../shell/shell.h"

/* PCI IDs do HDA (QEMU usa ICH6) */
#define HDA_PCI_VENDOR  0x8086
#define HDA_PCI_DEVICE  0x2668

/* CORB/RIRB — 256 entradas de 4 bytes (CORB) e 8 bytes (RIRB) */
#define CORB_ENTRIES  256
#define RIRB_ENTRIES  256

/* Buffers estaticos — enderecos fisicos fixos para DMA */
static uint32_t corb[CORB_ENTRIES] __attribute__((aligned(128)));
static uint64_t rirb[RIRB_ENTRIES] __attribute__((aligned(128)));

static volatile uint8_t *hda_base = 0;
static int hda_initialized = 0;
static uint16_t corb_wp = 0;  /* write pointer do CORB */

/* ---- Acesso MMIO -------------------------------------------------- */

static inline uint8_t  hda_r8 (uint32_t off) { return *(volatile uint8_t  *)(hda_base + off); }
static inline uint16_t hda_r16(uint32_t off) { return *(volatile uint16_t *)(hda_base + off); }
static inline uint32_t hda_r32(uint32_t off) { return *(volatile uint32_t *)(hda_base + off); }

static inline void hda_w8 (uint32_t off, uint8_t  v) { *(volatile uint8_t  *)(hda_base + off) = v; }
static inline void hda_w16(uint32_t off, uint16_t v) { *(volatile uint16_t *)(hda_base + off) = v; }
static inline void hda_w32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(hda_base + off) = v; }

/* Delay simples por polling */
static void hda_delay(uint32_t n) {
    for (volatile uint32_t i = 0; i < n * 10000; i++)
        __asm__ volatile ("nop");
}

/* ---- Inicializacao ----------------------------------------------- */

int hda_init(void) {
    /* Encontra HDA via PCI */
    pci_device_t *pci = pci_find(HDA_PCI_VENDOR, HDA_PCI_DEVICE);
    if (!pci) {
        /* Tenta busca por classe (class=0x04, subclass=0x03) */
        int n = pci_get_count();
        pci_device_t *list = pci_get_list();
        for (int i = 0; i < n; i++) {
            if (list[i].class_code == 0x04 && list[i].subclass == 0x03) {
                pci = &list[i];
                break;
            }
        }
        if (!pci) return -1;
    }

    /* BAR0 contem a base MMIO */
    uint32_t bar0 = pci->bar[0] & ~0xFUL;
    if (!bar0) return -1;

    /* Mapeia BAR0 no espaco do kernel */
    paging_mapear(bar0, bar0, 0x001 | 0x002);
    __asm__ volatile ("invlpg (%0)" : : "r"(bar0) : "memory");

    hda_base = (volatile uint8_t *)bar0;

    /* Habilita Bus Master e Memory Space no PCI command register */
    uint32_t cmd = pci_read(pci->bus, pci->device, pci->function, 0x04);
    pci_write(pci->bus, pci->device, pci->function, 0x04, cmd | 0x06);

    /* Reset do controller: zera GCTL bit 0, aguarda, seta bit 0 */
    hda_w32(HDA_GCTL, 0);
    hda_delay(1);  /* espera reset propagar */

    hda_w32(HDA_GCTL, 1);  /* sai do reset */

    /* Aguarda controller sair do reset (GCTL bit 0 = 1) */
    uint32_t timeout = 50;
    while (!(hda_r32(HDA_GCTL) & 1) && timeout--)
        hda_delay(1);

    if (!timeout) {
        shell_writeln("HDA: timeout no reset");
        return -1;
    }

    /* Aguarda codecs se anunciarem via STATESTS (25.4.3 da spec) */
    hda_delay(2);
    uint16_t statests = hda_r16(HDA_STATESTS);

    /* Configura CORB ------------------------------------------------ */
    /* Desativa CORB antes de configurar */
    hda_w8(HDA_CORBCTL, 0);

    /* Tamanho: 256 entradas (bits 1:0 = 10) */
    hda_w8(HDA_CORBSIZE, 0x02);

    /* Base do CORB (endereco fisico dos nossos buffers estaticos) */
    uint32_t corb_phys = (uint32_t)corb;
    hda_w32(HDA_CORBLBASE, corb_phys);
    hda_w32(HDA_CORBUBASE, 0);  /* 64-bit: upper 32 bits = 0 */

    /* Reset read pointer */
    hda_w16(HDA_CORBRP, 0x8000);
    hda_delay(1);
    hda_w16(HDA_CORBRP, 0x0000);

    hda_w16(HDA_CORBWP, 0);
    corb_wp = 0;

    /* Ativa CORB: bit 1 = DMA enable */
    hda_w8(HDA_CORBCTL, 0x02);

    /* Configura RIRB ------------------------------------------------ */
    hda_w8(HDA_RIRBCTL, 0);

    hda_w8(HDA_RIRBSIZE, 0x02);  /* 256 entradas */

    uint32_t rirb_phys = (uint32_t)rirb;
    hda_w32(HDA_RIRBLBASE, rirb_phys);
    hda_w32(HDA_RIRBUBASE, 0);

    /* Reset write pointer */
    hda_w16(HDA_RIRBWP, 0x8000);

    /* Interrupcao apos cada resposta */
    hda_w16(HDA_RINTCNT, 1);

    /* Ativa RIRB: bit 1 = DMA enable */
    hda_w8(HDA_RIRBCTL, 0x02);

    hda_initialized = 1;

    shell_write("HDA: inicializado, codecs detectados: 0x");
    shell_write_hex(statests);
    shell_writeln("");

    return 0;
}

int hda_ready(void) {
    return hda_initialized;
}

uint32_t hda_send_verb(uint8_t codec, uint8_t node, uint16_t verb, uint8_t payload) {
    if (!hda_initialized) return 0xFFFFFFFF;

    /* Constroi o verb de 32 bits:
       bits 31-28 = codec address
       bits 27-20 = node id
       bits 19-8  = verb (12-bit)
       bits 7-0   = payload */
    uint32_t cmd = ((uint32_t)codec  << 28)
                 | ((uint32_t)node   << 20)
                 | ((uint32_t)verb   <<  8)
                 | payload;

    /* Incrementa write pointer do CORB */
    corb_wp = (uint16_t)((corb_wp + 1) & 0xFF);
    corb[corb_wp] = cmd;
    hda_w16(HDA_CORBWP, corb_wp);

    /* Aguarda resposta no RIRB (polling no write pointer) */
    uint16_t rirb_wp_prev = hda_r16(HDA_RIRBWP);
    uint32_t timeout = 10000;
    while (timeout--) {
        uint16_t wp = hda_r16(HDA_RIRBWP);
        if (wp != rirb_wp_prev) {
            /* Nova resposta disponivel */
            return (uint32_t)(rirb[wp & 0xFF] & 0xFFFFFFFF);
        }
        __asm__ volatile ("pause");
    }
    return 0xFFFFFFFF;  /* timeout */
}
