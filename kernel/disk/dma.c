#include "dma.h"
#include "ata.h"
#include "../drivers/pci.h"
#include "../cpu/io.h"
#include "../disk/kstring.h"

/* Bus Master IDE register offsets (relativo ao BAR4 I/O base) */
#define BM_CMD     0x00   /* Command: bit0=Start/Stop, bit3=Direction(0=read,1=write) */
#define BM_STATUS  0x02   /* Status: bit0=Active, bit1=Fail, bit2=IRQ */
#define BM_PRDT    0x04   /* PRDT physical address */

#define BM_CMD_START  0x01
#define BM_CMD_WRITE  0x08   /* 0=host read (ATA write), 1=host write (ATA read) */
#define BM_ST_ACTIVE  0x01
#define BM_ST_ERR     0x02
#define BM_ST_IRQ     0x04

/* ATA DMA commands */
#define ATA_CMD_READ_DMA  0xC8
#define ATA_CMD_WRITE_DMA 0xCA

typedef struct {
    uint32_t phys_addr;
    uint16_t byte_count;  /* 0 = 64KB */
    uint16_t flags;       /* bit15 = EOT (end of table) */
} __attribute__((packed)) prdt_entry_t;

/* Alinhamento 4-byte; max 1 entrada por operacao (max 64KB) */
static prdt_entry_t prdt[2] __attribute__((aligned(4)));
static uint8_t      dma_buf[512 * 16] __attribute__((aligned(4)));  /* buffer intermediario 8KB */

static uint16_t bm_base = 0;   /* I/O base do Bus Master (BAR4) */
static int dma_ok = 0;

/* ---- Helpers I/O Bus Master --------------------------------------- */

static inline void bm_w8 (uint8_t reg, uint8_t  v) { outb((uint16_t)(bm_base + reg), v); }
static inline void bm_w32(uint8_t reg, uint32_t v) { outl((uint16_t)(bm_base + reg), v); }
static inline uint8_t  bm_r8 (uint8_t reg) { return inb((uint16_t)(bm_base + reg)); }

/* ---- Init --------------------------------------------------------- */

int dma_init(void) {
    /* Busca controlador IDE com suporte a Bus Master (prog_if bit 7) */
    pci_device_t *ide = pci_find_class(0x01, 0x01);
    if (!ide) return -1;

    /* Verifica se Bus Master e suportado (prog_if bit 7) */
    uint32_t pi = pci_read(ide->bus, ide->device, ide->function, 0x08);
    uint8_t prog_if = (uint8_t)(pi >> 8);
    if (!(prog_if & 0x80)) return -1;  /* sem Bus Master */

    /* BAR4 = Bus Master I/O base */
    uint32_t bar4 = ide->bar[4];
    if (!(bar4 & 0x01)) return -1;  /* deve ser I/O space */
    bm_base = (uint16_t)(bar4 & ~0x3);

    /* Habilita Bus Master no comando PCI */
    uint32_t cmd = pci_read(ide->bus, ide->device, ide->function, 0x04);
    pci_write(ide->bus, ide->device, ide->function, 0x04, cmd | 0x05);

    /* Limpa status */
    bm_w8(BM_STATUS, BM_ST_ERR | BM_ST_IRQ);

    dma_ok = 1;
    return 0;
}

int dma_disponivel(void) { return dma_ok; }

/* ---- Operacao DMA generica --------------------------------------- */

static int dma_op(uint32_t lba, uint8_t count, uint8_t *buffer, int write) {
    if (!dma_ok || count == 0 || count > 16) return -1;
    uint32_t bytes = (uint32_t)count * 512;

    /* Monta PRDT */
    if (write) k_memcpy(dma_buf, buffer, bytes);
    prdt[0].phys_addr  = (uint32_t)dma_buf;
    prdt[0].byte_count = (uint16_t)(bytes == 65536 ? 0 : bytes);
    prdt[0].flags      = 0x8000;  /* EOT */

    /* Para Bus Master */
    bm_w8(BM_CMD, 0);
    bm_w8(BM_STATUS, BM_ST_ERR | BM_ST_IRQ);

    /* Configura PRDT */
    bm_w32(BM_PRDT, (uint32_t)prdt);

    /* Configura direcao: 0=write-from-mem(ATA read), 8=read-to-mem(ATA write) */
    bm_w8(BM_CMD, write ? 0 : BM_CMD_WRITE);

    /* Aguarda ATA pronto */
    uint32_t t = 100000;
    while ((inb(ATA_STATUS) & 0x80) && t--);  /* BSY */

    /* Envia comando ATA DMA */
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0xF)));
    outb(ATA_SETOR_COUNT, count);
    outb(ATA_LBA_LOW,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD, write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA);

    /* Inicia Bus Master */
    bm_w8(BM_CMD, (uint8_t)((write ? 0 : BM_CMD_WRITE) | BM_CMD_START));

    /* Aguarda conclusao (polling status) */
    t = 500000;
    uint8_t st;
    do {
        st = bm_r8(BM_STATUS);
        if (!(st & BM_ST_ACTIVE)) break;
        io_wait();
    } while (t--);

    /* Para Bus Master */
    bm_w8(BM_CMD, 0);
    bm_w8(BM_STATUS, BM_ST_ERR | BM_ST_IRQ);

    if (st & BM_ST_ERR) return -1;
    if (!write) k_memcpy(buffer, dma_buf, bytes);
    return 0;
}

int dma_ler(uint32_t lba, uint8_t count, uint8_t *buffer) {
    return dma_op(lba, count, buffer, 0);
}

int dma_gravar(uint32_t lba, uint8_t count, uint8_t *buffer) {
    return dma_op(lba, count, buffer, 1);
}
