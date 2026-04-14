#include "usb.h"
#include "pci.h"
#include "../mem/paging.h"
#include "../cpu/io.h"
#include "../disk/kstring.h"
#include "../shell/shell.h"

/* ---- EHCI Capability Registers (relativo ao BAR0) ----------------- */
#define EHCI_CAPLENGTH  0x00   /* offset para operational registers */
#define EHCI_HCIVERSION 0x02
#define EHCI_HCSPARAMS  0x04   /* N_PORTS em bits3:0 */
#define EHCI_HCCPARAMS  0x08

/* ---- EHCI Operational Registers (base = BAR0 + CAPLENGTH) --------- */
#define EHCI_USBCMD    0x00
#define EHCI_USBSTS    0x04
#define EHCI_USBINTR   0x08
#define EHCI_FRINDEX   0x0C
#define EHCI_CTRLDSEG  0x10
#define EHCI_PLISTBASE 0x14
#define EHCI_ALISTADDR 0x18
#define EHCI_CONFIGFLAG 0x40
#define EHCI_PORTSC(n) (0x44 + (n) * 4)

/* USBCMD bits */
#define USBCMD_RS     (1u << 0)
#define USBCMD_HCRESET (1u << 1)
#define USBCMD_FLS512 (1u << 3)   /* frame list size 512 */
#define USBCMD_ASE    (1u << 5)   /* async schedule enable */
#define USBCMD_PSE    (1u << 4)   /* periodic schedule enable */

/* USBSTS bits */
#define USBSTS_HALTED (1u << 12)

/* PORTSC bits */
#define PORTSC_CCS    (1u << 0)   /* current connect status */
#define PORTSC_CSC    (1u << 1)   /* connect status change */
#define PORTSC_PED    (1u << 2)   /* port enable */
#define PORTSC_PEDC   (1u << 3)   /* port enable change */
#define PORTSC_PRST   (1u << 8)   /* port reset */
#define PORTSC_PP     (1u << 12)  /* port power */
#define PORTSC_OWNER  (1u << 13)  /* 0=EHCI, 1=companion HC */

/* ---- Queue Head (QH) — 48 bytes aligned 32 ------------------------ */
typedef struct {
    uint32_t next_qh;         /* bit0=T (terminate), bits4:1=type */
    uint32_t ep_char;         /* device addr, EP num, speed, max pkt */
    uint32_t ep_cap;          /* mult, port, hub, C-mask, S-mask */
    /* Overlay Transfer Descriptor */
    uint32_t cur_qtd;
    uint32_t next_qtd;
    uint32_t alt_qtd;
    uint32_t token;
    uint32_t buf[5];
    uint32_t _pad[4];
} __attribute__((packed, aligned(32))) ehci_qh_t;

/* ---- Queue Transfer Descriptor (qTD) — 32 bytes aligned 32 -------- */
typedef struct {
    uint32_t next;         /* bit0=T */
    uint32_t alt_next;
    uint32_t token;        /* PID, CERR, C_Page, IOC, Total bytes, Status */
    uint32_t buf[5];
} __attribute__((packed, aligned(32))) ehci_qtd_t;

/* QTD token bits */
#define QTD_STS_ACTIVE (1u << 7)
#define QTD_STS_HALTED (1u << 6)
#define QTD_STS_MASK   0xFF
#define QTD_PID_OUT    (0u << 8)
#define QTD_PID_IN     (1u << 8)
#define QTD_PID_SETUP  (2u << 8)
#define QTD_IOC        (1u << 15)
#define QTD_BYTES(n)   ((uint32_t)(n) << 16)
#define QTD_DT         (1u << 31)

/* QH char bits */
#define QH_EPS_FS  (0u << 12)
#define QH_EPS_LS  (1u << 12)
#define QH_EPS_HS  (2u << 12)
#define QH_DTC     (1u << 14)
#define QH_H       (1u << 15)  /* head of reclamation list */

/* ---- Estado interno ----------------------------------------------- */
#define USB_MAX_DEVS 8

static uint32_t ehci_mmio = 0;    /* BAR0 */
static uint32_t op_base   = 0;    /* mmio + CAPLENGTH */
static int      n_ports   = 0;
static int      ehci_ok   = 0;
static int      n_devs    = 0;

/* Alocacao estatica das estruturas DMA */
static ehci_qh_t   async_head __attribute__((aligned(32)));
static ehci_qh_t   qh         __attribute__((aligned(32)));
static ehci_qtd_t  qtds[4]    __attribute__((aligned(32)));
static uint8_t     xfer_buf[512] __attribute__((aligned(4)));

static inline uint32_t op_r(uint32_t reg) {
    return *(volatile uint32_t *)(op_base + reg);
}
static inline void op_w(uint32_t reg, uint32_t v) {
    *(volatile uint32_t *)(op_base + reg) = v;
}
static inline uint32_t cap_r16(uint32_t reg) {
    return *(volatile uint16_t *)(ehci_mmio + reg);
}
static inline uint32_t cap_r(uint32_t reg) {
    return *(volatile uint32_t *)(ehci_mmio + reg);
}

static void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 10000; i++) __asm__ volatile("nop");
}

/* ---- Envio de qTD via async queue --------------------------------- */

static int ehci_run_qh(uint8_t dev_addr, uint8_t ep, uint8_t speed) {
    /* Head QH da async list (circular, aponta para si mesmo) */
    k_memset(&async_head, 0, sizeof(async_head));
    async_head.next_qh = (uint32_t)&async_head | 0x2;  /* type=QH, T=0 */
    async_head.ep_char = QH_H | QH_DTC | QH_EPS_HS;
    async_head.ep_cap  = 1u << 30;  /* mult=1 */
    async_head.next_qtd = 1;  /* T=1 (terminate) */
    async_head.alt_qtd  = 1;
    async_head.token    = 0;

    /* QH do dispositivo aponta de volta para o head */
    qh.next_qh  = (uint32_t)&async_head | 0x2;
    qh.ep_char  = (uint32_t)dev_addr
                | ((uint32_t)ep << 8)
                | ((speed == 2) ? QH_EPS_HS : QH_EPS_FS)
                | QH_DTC
                | (64u << 16);   /* MaxPacket=64 */
    qh.ep_cap   = 1u << 30;     /* mult=1 */
    qh.cur_qtd  = 0;
    qh.next_qtd = (uint32_t)&qtds[0];
    qh.alt_qtd  = 1;
    qh.token    = 0;
    k_memset(qh.buf, 0, sizeof(qh.buf));

    /* Insere QH no anel (head → qh → head) */
    async_head.next_qh = (uint32_t)&qh | 0x2;

    /* Configura async list */
    op_w(EHCI_ALISTADDR, (uint32_t)&async_head);

    /* Habilita async schedule */
    uint32_t cmd = op_r(EHCI_USBCMD);
    op_w(EHCI_USBCMD, cmd | USBCMD_ASE | USBCMD_RS);

    /* Aguarda conclusao (qualquer qTD ativo) */
    uint32_t t = 500000;
    while (t--) {
        if (!(qh.token & QTD_STS_ACTIVE)) break;
        __asm__ volatile("pause");
    }

    /* Para async */
    op_w(EHCI_USBCMD, op_r(EHCI_USBCMD) & ~USBCMD_ASE);
    uint32_t tw = 10000;
    while ((op_r(EHCI_USBSTS) & (1u << 15)) && tw--);

    return (qh.token & (QTD_STS_HALTED | QTD_STS_ACTIVE)) ? -1 : 0;
}

/* ---- Control Transfer -------------------------------------------- */

int usb_control(uint8_t dev_addr, usb_setup_t *setup,
                uint8_t *data, uint16_t len, int is_in) {
    if (!ehci_ok) return -1;

    /* qTD 0: SETUP */
    k_memcpy(xfer_buf, setup, 8);
    k_memset(&qtds[0], 0, sizeof(qtds[0]));
    qtds[0].next     = (uint32_t)&qtds[1];
    qtds[0].alt_next = 1;
    qtds[0].token    = QTD_STS_ACTIVE | QTD_PID_SETUP | QTD_BYTES(8) | (3u << 10);
    qtds[0].buf[0]   = (uint32_t)xfer_buf;

    uint32_t n_qtds = 1;

    /* qTD 1: DATA (opcional) */
    if (len > 0 && data) {
        if (is_in) k_memset(xfer_buf + 8, 0, len);
        else k_memcpy(xfer_buf + 8, data, len);
        k_memset(&qtds[1], 0, sizeof(qtds[1]));
        qtds[1].next     = (uint32_t)&qtds[2];
        qtds[1].alt_next = 1;
        qtds[1].token    = QTD_STS_ACTIVE | QTD_DT
                         | (is_in ? QTD_PID_IN : QTD_PID_OUT)
                         | QTD_BYTES(len) | (3u << 10);
        qtds[1].buf[0]   = (uint32_t)(xfer_buf + 8);
        n_qtds = 2;
    }

    /* qTD STATUS: handshake inverso */
    k_memset(&qtds[n_qtds], 0, sizeof(qtds[0]));
    qtds[n_qtds].next     = 1;  /* terminate */
    qtds[n_qtds].alt_next = 1;
    qtds[n_qtds].token    = QTD_STS_ACTIVE | QTD_IOC
                          | (is_in ? QTD_PID_OUT : QTD_PID_IN)
                          | QTD_DT | (3u << 10);

    int ret = ehci_run_qh(dev_addr, 0, 2 /* HS */);
    if (ret == 0 && is_in && len > 0 && data)
        k_memcpy(data, xfer_buf + 8, len);
    return ret;
}

int usb_hid_poll(uint8_t dev_addr, uint8_t ep, uint8_t *buf, uint8_t len) {
    if (!ehci_ok) return -1;
    k_memset(&qtds[0], 0, sizeof(qtds[0]));
    k_memset(xfer_buf, 0, len);
    qtds[0].next     = 1;
    qtds[0].alt_next = 1;
    qtds[0].token    = QTD_STS_ACTIVE | QTD_PID_IN | QTD_IOC
                     | QTD_DT | QTD_BYTES(len) | (3u << 10);
    qtds[0].buf[0]   = (uint32_t)xfer_buf;

    int ret = ehci_run_qh(dev_addr, ep & 0x0F, 2);
    if (ret == 0) k_memcpy(buf, xfer_buf, len);
    return ret;
}

/* ---- Port reset + enumerate --------------------------------------- */

static int port_reset(int p) {
    uint32_t ps = op_r(EHCI_PORTSC(p));
    if (!(ps & PORTSC_CCS)) return -1;  /* nada conectado */

    /* Garante energia na porta */
    op_w(EHCI_PORTSC(p), ps | PORTSC_PP);
    delay_ms(20);

    /* Reset (250ms conforme USB spec) */
    ps = op_r(EHCI_PORTSC(p));
    op_w(EHCI_PORTSC(p), (ps & ~PORTSC_PED) | PORTSC_PRST);
    delay_ms(50);
    op_w(EHCI_PORTSC(p), op_r(EHCI_PORTSC(p)) & ~PORTSC_PRST);
    delay_ms(10);

    ps = op_r(EHCI_PORTSC(p));
    if (!(ps & PORTSC_PED)) {
        /* Porta nao habilitou — dispositivo FS/LS, redireciona para companion */
        op_w(EHCI_PORTSC(p), ps | PORTSC_OWNER);
        return -1;
    }
    return 0;
}

static void enumerate_port(int p) {
    if (port_reset(p) != 0) return;

    /* Dispositivo esta em addr 0 apos reset */
    usb_device_desc_t desc;
    usb_setup_t setup;
    k_memset(&setup, 0, sizeof(setup));

    /* GET_DESCRIPTOR (Device, 8 bytes primeiro para saber MaxPacketSize) */
    setup.bmRequestType = 0x80;
    setup.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup.wValue   = (USB_DESC_DEVICE << 8);
    setup.wLength  = 8;
    if (usb_control(0, &setup, (uint8_t *)&desc, 8, 1) != 0) return;

    /* SET_ADDRESS */
    uint8_t addr = (uint8_t)(n_devs + 1);
    k_memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x00;
    setup.bRequest = USB_REQ_SET_ADDRESS;
    setup.wValue   = addr;
    if (usb_control(0, &setup, 0, 0, 0) != 0) return;
    delay_ms(2);

    /* GET_DESCRIPTOR completo */
    k_memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x80;
    setup.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup.wValue   = (USB_DESC_DEVICE << 8);
    setup.wLength  = sizeof(usb_device_desc_t);
    usb_control(addr, &setup, (uint8_t *)&desc, sizeof(desc), 1);

    /* SET_CONFIGURATION 1 */
    k_memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x00;
    setup.bRequest = USB_REQ_SET_CONFIG;
    setup.wValue   = 1;
    usb_control(addr, &setup, 0, 0, 0);

    n_devs++;
    shell_write("USB: dispositivo "); shell_write_num(addr);
    shell_write(" VID="); shell_write_hex(desc.idVendor);
    shell_write(" PID="); shell_write_hex(desc.idProduct);
    shell_writeln("");
}

/* ---- Init --------------------------------------------------------- */

int usb_init(void) {
    pci_device_t *dev = pci_find_class(0x0C, 0x03);
    if (!dev) return -1;

    uint8_t prog_if = (uint8_t)(pci_read(dev->bus, dev->device, dev->function, 0x08) >> 8);
    if (prog_if != 0x20) return -1;  /* precisa ser EHCI */

    uint32_t bar0 = dev->bar[0] & ~0xF;
    if (!bar0) return -1;

    /* Mapeia MMIO */
    for (uint32_t off = 0; off < 0x2000; off += 0x1000)
        paging_mapear(bar0 + off, bar0 + off, 0x001 | 0x002);

    ehci_mmio = bar0;

    uint8_t caplength = (uint8_t)cap_r16(EHCI_CAPLENGTH);
    op_base = ehci_mmio + caplength;

    n_ports = (int)(cap_r(EHCI_HCSPARAMS) & 0xF);

    /* Habilita Bus Master */
    uint32_t cmd_pci = pci_read(dev->bus, dev->device, dev->function, 0x04);
    pci_write(dev->bus, dev->device, dev->function, 0x04, cmd_pci | 0x06);

    /* Reset HC */
    op_w(EHCI_USBCMD, USBCMD_HCRESET);
    uint32_t t = 100000;
    while ((op_r(EHCI_USBCMD) & USBCMD_HCRESET) && t--);

    /* Configura async list vazia */
    k_memset(&async_head, 0, sizeof(async_head));
    async_head.next_qh = (uint32_t)&async_head | 0x2;
    async_head.ep_char = QH_H | QH_EPS_HS;
    async_head.ep_cap  = 1u << 30;
    async_head.next_qtd = 1;
    async_head.alt_qtd  = 1;

    op_w(EHCI_CTRLDSEG, 0);
    op_w(EHCI_ALISTADDR, (uint32_t)&async_head);
    op_w(EHCI_USBINTR, 0);
    op_w(EHCI_USBSTS, 0x3F);  /* limpa status */

    /* Configura para rotear portas para EHCI */
    op_w(EHCI_CONFIGFLAG, 1);
    delay_ms(5);

    /* Liga o HC */
    op_w(EHCI_USBCMD, USBCMD_RS | (3u << 2));  /* RS + FLS=1024 frames */
    t = 50000;
    while ((op_r(EHCI_USBSTS) & USBSTS_HALTED) && t--);

    ehci_ok = 1;

    /* Enumera portas */
    for (int p = 0; p < n_ports && p < 4; p++) {
        uint32_t ps = op_r(EHCI_PORTSC(p));
        /* Liga energia */
        op_w(EHCI_PORTSC(p), ps | PORTSC_PP);
        delay_ms(20);
        /* Limpa mudancas de status */
        op_w(EHCI_PORTSC(p), op_r(EHCI_PORTSC(p)) | PORTSC_CSC | PORTSC_PEDC);
        delay_ms(100);
        if (op_r(EHCI_PORTSC(p)) & PORTSC_CCS)
            enumerate_port(p);
    }

    return 0;
}

int usb_ready(void)        { return ehci_ok; }
int usb_device_count(void) { return n_devs; }
