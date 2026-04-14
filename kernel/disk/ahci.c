#include "ahci.h"
#include "../drivers/pci.h"
#include "../mem/paging.h"
#include "../disk/kstring.h"
#include "../shell/shell.h"

/* ---- Offsets HBA Generic Host Control ----------------------------- */
#define HBA_CAP    0x00
#define HBA_GHC    0x04   /* Global Host Control: bit0=HR, bit1=IE, bit31=AE */
#define HBA_IS     0x08
#define HBA_PI     0x0C   /* Ports Implemented (bitmap) */
#define HBA_VS     0x10
#define HBA_CAP2   0x24

#define GHC_AE     (1u << 31)
#define GHC_HR     (1u << 0)

/* ---- Offsets Port Registers (base = ABAR + 0x100 + port*0x80) ---- */
#define PORT_CLB   0x00   /* Command List Base Address */
#define PORT_CLBU  0x04
#define PORT_FB    0x08   /* FIS Base Address */
#define PORT_FBU   0x0C
#define PORT_IS    0x10
#define PORT_IE    0x14
#define PORT_CMD   0x18
#define PORT_TFD   0x20   /* Task File Data: bit0=ERR, bit3=DRQ, bit7=BSY */
#define PORT_SIG   0x24
#define PORT_SSTS  0x28   /* SATA Status: bits3:0=DET, bits11:8=IPM */
#define PORT_SERR  0x30
#define PORT_SACT  0x34
#define PORT_CI    0x38   /* Command Issue */

#define PORT_CMD_ST   (1u << 0)   /* Start */
#define PORT_CMD_FRE  (1u << 4)   /* FIS Receive Enable */
#define PORT_CMD_FR   (1u << 14)
#define PORT_CMD_CR   (1u << 15)

#define SSTS_DET_PRESENT  0x3   /* device present + comms established */
#define SSTS_IPM_ACTIVE   0x1

/* ---- FIS Type ----------------------------------------------------- */
#define FIS_TYPE_REG_H2D  0x27

typedef struct {
    uint8_t  type;      /* FIS_TYPE_REG_H2D */
    uint8_t  pmport_c;  /* bit7 = C (command register) */
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0, lba1, lba2;
    uint8_t  device;    /* bit6=LBA, bit4=DRV */
    uint8_t  lba3, lba4, lba5;
    uint8_t  featureh;
    uint8_t  countl, counth;
    uint8_t  icc, control;
    uint8_t  _pad[4];
} __attribute__((packed)) fis_reg_h2d_t;  /* 20 bytes */

/* ---- Command Header (32 bytes) ------------------------------------ */
typedef struct {
    uint16_t flags;    /* bits4:0=CFL, bit6=ATAPI, bit7=W(write), bit9=P */
    uint16_t prdtl;   /* PRDT length (entries) */
    uint32_t prdbc;   /* bytes transferred (set by HBA) */
    uint32_t ctba;    /* Command Table Base Address */
    uint32_t ctbau;
    uint32_t _rsv[4];
} __attribute__((packed)) hba_cmd_hdr_t;

/* ---- PRDT entry (16 bytes) ---------------------------------------- */
typedef struct {
    uint32_t dba;    /* Data Base Address */
    uint32_t dbau;
    uint32_t _rsv;
    uint32_t dbc;    /* bits21:0=byte_count-1, bit31=IRQ on completion */
} __attribute__((packed)) hba_prdt_t;

/* ---- Command Table ------------------------------------------------ */
typedef struct {
    uint8_t    cfis[64];  /* Command FIS */
    uint8_t    acmd[16];  /* ATAPI Command */
    uint8_t    _rsv[48];
    hba_prdt_t prdt[1];   /* PRDT entries (1 per operacao) */
} __attribute__((packed)) hba_cmd_tbl_t;

/* ---- Estado interno ----------------------------------------------- */
#define AHCI_MAX_PORTS  4
#define AHCI_BUF_SIZE   (512 * 16)  /* 8KB por operacao maxima */

/* Estruturas alinhadas em 1KB (CLB) e 256B (FB) — requisito AHCI */
static hba_cmd_hdr_t cmd_list[AHCI_MAX_PORTS][32] __attribute__((aligned(1024)));
static uint8_t       fis_buf [AHCI_MAX_PORTS][256] __attribute__((aligned(256)));
static hba_cmd_tbl_t cmd_tbl [AHCI_MAX_PORTS]      __attribute__((aligned(128)));
static uint8_t       xfer_buf[AHCI_BUF_SIZE]        __attribute__((aligned(4)));

static uint32_t abar_base  = 0;   /* MMIO base */
static int      ahci_port  = -1;  /* porta SATA com disco */
static int      ahci_ready = 0;

/* ---- MMIO helpers ------------------------------------------------- */

static inline uint32_t hba_r(uint32_t off) {
    return *(volatile uint32_t *)(abar_base + off);
}
static inline void hba_w(uint32_t off, uint32_t v) {
    *(volatile uint32_t *)(abar_base + off) = v;
}
static inline uint32_t port_r(int p, uint32_t off) {
    return hba_r(0x100u + (uint32_t)p * 0x80u + off);
}
static inline void port_w(int p, uint32_t off, uint32_t v) {
    hba_w(0x100u + (uint32_t)p * 0x80u + off, v);
}

/* ---- Port start/stop ---------------------------------------------- */

static void port_stop(int p) {
    uint32_t cmd = port_r(p, PORT_CMD);
    cmd &= ~(PORT_CMD_ST | PORT_CMD_FRE);
    port_w(p, PORT_CMD, cmd);
    /* Aguarda CR e FR = 0 */
    uint32_t t = 50000;
    while ((port_r(p, PORT_CMD) & (PORT_CMD_CR | PORT_CMD_FR)) && t--);
}

static void port_start(int p) {
    uint32_t t = 10000;
    while ((port_r(p, PORT_CMD) & PORT_CMD_CR) && t--);

    uint32_t cmd = port_r(p, PORT_CMD);
    cmd |= PORT_CMD_FRE | PORT_CMD_ST;
    port_w(p, PORT_CMD, cmd);
}

/* ---- Init --------------------------------------------------------- */

int ahci_init(void) {
    pci_device_t *dev = pci_find_class(0x01, 0x06);
    if (!dev) return -1;

    /* BAR5 = ABAR (AHCI Base Memory Register) */
    uint32_t bar5 = dev->bar[5];
    if (!bar5 || (bar5 & 0x01)) return -1;  /* deve ser MMIO */
    abar_base = bar5 & ~0xFFF;

    /* Mapeia MMIO no espaco de enderecos do kernel */
    for (uint32_t off = 0; off < 0x2000; off += 0x1000)
        paging_mapear(abar_base + off, abar_base + off,
                      0x001 | 0x002);  /* PRESENTE | ESCRITA */

    /* Enable AHCI mode + Bus Master */
    uint32_t cmd_pci = pci_read(dev->bus, dev->device, dev->function, 0x04);
    pci_write(dev->bus, dev->device, dev->function, 0x04, cmd_pci | 0x06);

    /* Habilita AHCI (GHC.AE = 1) */
    hba_w(HBA_GHC, hba_r(HBA_GHC) | GHC_AE);

    /* Reset HBA */
    hba_w(HBA_GHC, hba_r(HBA_GHC) | GHC_HR);
    uint32_t t = 100000;
    while ((hba_r(HBA_GHC) & GHC_HR) && t--);
    hba_w(HBA_GHC, hba_r(HBA_GHC) | GHC_AE);  /* re-enable apos reset */

    /* Enumera portas implementadas */
    uint32_t pi = hba_r(HBA_PI);
    for (int p = 0; p < AHCI_MAX_PORTS; p++) {
        if (!(pi & (1u << p))) continue;

        /* Verifica presenca de dispositivo */
        uint32_t ssts = port_r(p, PORT_SSTS);
        uint8_t det = (uint8_t)(ssts & 0xF);
        uint8_t ipm = (uint8_t)((ssts >> 8) & 0xF);
        if (det != SSTS_DET_PRESENT || ipm != SSTS_IPM_ACTIVE) continue;

        /* Para porta antes de configurar */
        port_stop(p);

        /* Configura CLB e FB */
        port_w(p, PORT_CLB,  (uint32_t)cmd_list[p]);
        port_w(p, PORT_CLBU, 0);
        port_w(p, PORT_FB,   (uint32_t)fis_buf[p]);
        port_w(p, PORT_FBU,  0);

        /* Zera command list */
        k_memset(cmd_list[p], 0, sizeof(cmd_list[0]));
        k_memset(fis_buf[p],  0, sizeof(fis_buf[0]));

        /* Limpa erros */
        port_w(p, PORT_SERR, 0xFFFFFFFF);
        port_w(p, PORT_IS,   0xFFFFFFFF);

        /* Inicia porta */
        port_start(p);

        ahci_port = p;
        ahci_ready = 1;
        break;
    }

    return ahci_ready ? 0 : -1;
}

int ahci_disponivel(void) { return ahci_ready; }

/* ---- Envio de comando --------------------------------------------- */

static int ahci_issue(int p, uint8_t ata_cmd, uint32_t lba,
                       uint8_t count, uint8_t *buf, int write) {
    /* Usa slot 0 */
    hba_cmd_hdr_t *hdr = &cmd_list[p][0];
    hba_cmd_tbl_t *tbl = &cmd_tbl[p];

    k_memset(hdr, 0, sizeof(*hdr));
    k_memset(tbl, 0, sizeof(*tbl));

    /* Header */
    uint32_t bytes = (uint32_t)count * 512;
    hdr->flags = (uint16_t)(sizeof(fis_reg_h2d_t) / 4);  /* CFL em dwords */
    if (write) hdr->flags |= (1u << 6);                  /* W bit */
    hdr->prdtl = 1;
    hdr->ctba  = (uint32_t)tbl;
    hdr->ctbau = 0;

    /* PRDT */
    if (write) k_memcpy(xfer_buf, buf, bytes);
    tbl->prdt[0].dba  = (uint32_t)xfer_buf;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].dbc  = bytes - 1;  /* byte_count - 1 */

    /* FIS H2D */
    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)tbl->cfis;
    fis->type     = FIS_TYPE_REG_H2D;
    fis->pmport_c = 0x80;  /* C bit = 1 (command register) */
    fis->command  = ata_cmd;
    fis->device   = 0x40;  /* LBA mode */
    fis->lba0 = (uint8_t)(lba);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = 0;
    fis->lba5 = 0;
    fis->countl = count;
    fis->counth = 0;

    /* Limpa IS e emite comando (slot 0) */
    port_w(p, PORT_IS, 0xFFFFFFFF);
    port_w(p, PORT_CI, 1);

    /* Aguarda conclusao */
    uint32_t t = 500000;
    while (t--) {
        if (!(port_r(p, PORT_CI) & 1)) break;
        if (port_r(p, PORT_IS) & (1u << 30)) return -1;  /* fatal error */
    }
    if (t == 0) return -1;  /* timeout */

    if (!write) k_memcpy(buf, xfer_buf, bytes);
    return 0;
}

int ahci_ler(uint32_t lba, uint8_t count, uint8_t *buffer) {
    if (!ahci_ready || count > 16) return -1;
    return ahci_issue(ahci_port, 0x25 /*READ DMA EXT*/, lba, count, buffer, 0);
}

int ahci_gravar(uint32_t lba, uint8_t count, uint8_t *buffer) {
    if (!ahci_ready || count > 16) return -1;
    return ahci_issue(ahci_port, 0x35 /*WRITE DMA EXT*/, lba, count, buffer, 1);
}

void ahci_info(void) {
    if (!ahci_ready) { shell_writeln("AHCI: nenhum dispositivo"); return; }
    shell_write("AHCI: porta ");
    shell_write_num(ahci_port);
    shell_writeln(" ativa");
    uint32_t sig = port_r(ahci_port, PORT_SIG);
    shell_write("  SIG=0x"); shell_write_hex(sig); shell_writeln("");
}
