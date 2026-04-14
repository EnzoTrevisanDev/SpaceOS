#include "rtl8139.h"
#include "../drivers/pci.h"
#include "../cpu/io.h"
#include "../disk/kstring.h"
#include "../shell/shell.h"

/* ---- Registradores RTL8139 (offsets relativos ao I/O base) ------- */
#define RTL_MAC         0x00  /* MAC address (6 bytes) */
#define RTL_MAR0        0x08  /* Multicast filter */
#define RTL_TSAD0       0x20  /* TX Buffer Address 0 */
#define RTL_TSD0        0x10  /* TX Status 0 */
#define RTL_RBSTART     0x30  /* RX Buffer Start */
#define RTL_CR          0x37  /* Command Register */
#define RTL_CAPR        0x38  /* Current Address of Packet Read */
#define RTL_CBR         0x3A  /* Current Buffer Address */
#define RTL_IMR         0x3C  /* Interrupt Mask Register */
#define RTL_ISR         0x3E  /* Interrupt Status Register */
#define RTL_TCR         0x40  /* Transmit Configuration Register */
#define RTL_RCR         0x44  /* Receive Configuration Register */
#define RTL_BMCR        0x62  /* Basic Mode Control Register */
#define RTL_BMSR        0x64  /* Basic Mode Status Register */
#define RTL_CONFIG1     0x52

/* Bits do CR */
#define CR_RST          0x10  /* Reset */
#define CR_RE           0x08  /* Receiver Enable */
#define CR_TE           0x04  /* Transmitter Enable */
#define CR_BUFE         0x01  /* Buffer Empty (read-only) */

/* Bits do ISR */
#define ISR_ROK         0x0001  /* Receive OK */
#define ISR_RER         0x0002  /* Receive Error */
#define ISR_TOK         0x0004  /* Transmit OK */
#define ISR_TER         0x0008  /* Transmit Error */
#define ISR_RXOVW       0x0010  /* RX Buffer Overflow */
#define ISR_PUN         0x0020  /* Packet Underrun */
#define ISR_FOVW        0x0040  /* RX FIFO Overflow */
#define ISR_SERR        0x8000  /* System Error */

/* Bits do TSD (TX status) */
#define TSD_OWN         0x2000  /* OWN: 0 = DMA complete */
#define TSD_TABT        0x4000  /* Transmit Abort */
#define TSD_TOK         0x8000  /* Transmit OK */

/* RCR: accept all packets, no wrap, 8KB buffer */
#define RCR_AAP         0x01   /* accept all packets */
#define RCR_APM         0x02   /* accept physical match */
#define RCR_AM          0x04   /* accept multicast */
#define RCR_AB          0x08   /* accept broadcast */
#define RCR_WRAP        0x80   /* wrap mode (simplifica o ring buffer) */
#define RCR_RBLEN_8K    0x00   /* 8KB receive buffer */
#define RCR_MXDMA_UNLIM 0x700  /* DMA burst ilimitado */

/* ---- Receive ring buffer ----------------------------------------- */
/* 8KB + 16 bytes de overflow + 1500 para ultimo pacote nao truncar */
#define RX_BUF_SIZE  (8192 + 16 + 1500)

static uint8_t rx_buf[RX_BUF_SIZE] __attribute__((aligned(4)));
static uint16_t rx_read_ptr = 0;  /* CAPR espelhado em software */

/* ---- TX descriptors --------------------------------------------- */
#define TX_DESC_COUNT 4
static uint8_t tx_bufs[TX_DESC_COUNT][1536] __attribute__((aligned(4)));
static int tx_next = 0;  /* proximo descritor a usar */

/* ---- Estado -------------------------------------------------------- */
static uint16_t io_base = 0;
static uint8_t  mac_addr[6];
static net_recv_cb_t recv_cb = 0;
static int rtl_initialized = 0;

/* ---- Helpers de I/O ----------------------------------------------- */

static inline void rtl_w8 (uint8_t  reg, uint8_t  v) { outb((uint16_t)(io_base + reg), v); }
static inline void rtl_w16(uint8_t  reg, uint16_t v) { outw((uint16_t)(io_base + reg), v); }
static inline void rtl_w32(uint8_t  reg, uint32_t v) { outl((uint16_t)(io_base + reg), v); }
static inline uint8_t  rtl_r8 (uint8_t reg) { return inb((uint16_t)(io_base + reg)); }
static inline uint16_t rtl_r16(uint8_t reg) { return inw((uint16_t)(io_base + reg)); }
static inline uint32_t rtl_r32(uint8_t reg) { return inl((uint16_t)(io_base + reg)); }

/* ---- Inicializacao ----------------------------------------------- */

int rtl8139_init(void) {
    pci_device_t *pci = pci_find(RTL8139_VENDOR, RTL8139_DEVICE);
    if (!pci) return -1;

    /* BAR0 contem I/O base (bit 0 = 1 indica I/O space) */
    uint32_t bar0 = pci->bar[0];
    if (!(bar0 & 0x01)) {
        /* Alguns QEMU usam MMIO — tenta BAR1 */
        bar0 = pci->bar[1];
    }
    io_base = (uint16_t)(bar0 & ~0x3);

    /* Habilita Bus Master + I/O Space no comando PCI */
    uint32_t cmd = pci_read(pci->bus, pci->device, pci->function, 0x04);
    pci_write(pci->bus, pci->device, pci->function, 0x04, cmd | 0x05);

    /* Power on (CONFIG1 bit 0 = 0 => on) */
    rtl_w8(RTL_CONFIG1, 0x00);

    /* Soft reset */
    rtl_w8(RTL_CR, CR_RST);
    uint32_t timeout = 100000;
    while ((rtl_r8(RTL_CR) & CR_RST) && timeout--) io_wait();
    if (!timeout) return -1;

    /* Le MAC address */
    for (int i = 0; i < 6; i++)
        mac_addr[i] = rtl_r8((uint8_t)(RTL_MAC + i));

    /* Configura RX buffer */
    rtl_w32(RTL_RBSTART, (uint32_t)rx_buf);
    rx_read_ptr = 0;

    /* Aceita todos os pacotes; WRAP mode; 8KB buffer */
    rtl_w32(RTL_RCR, RCR_AAP | RCR_APM | RCR_AM | RCR_AB |
                     RCR_WRAP | RCR_RBLEN_8K | RCR_MXDMA_UNLIM);

    /* Transmit config: max DMA burst */
    rtl_w32(RTL_TCR, 0x03000700);

    /* Mascara de interrupcao: ROK + TOK */
    rtl_w16(RTL_IMR, ISR_ROK | ISR_TOK | ISR_RER | ISR_TER);

    /* Habilita TX e RX */
    rtl_w8(RTL_CR, CR_RE | CR_TE);

    rtl_initialized = 1;

    shell_write("RTL8139: MAC ");
    for (int i = 0; i < 6; i++) {
        shell_write_char("0123456789ABCDEF"[(mac_addr[i] >> 4) & 0xF]);
        shell_write_char("0123456789ABCDEF"[mac_addr[i] & 0xF]);
        if (i < 5) shell_write_char(':');
    }
    shell_writeln("");

    return 0;
}

/* ---- Transmissao ------------------------------------------------- */

int rtl8139_send(const uint8_t *data, uint16_t len) {
    if (!rtl_initialized) return -1;
    if (len > 1514 || len == 0) return -1;

    /* Copia para buffer TX alinhado */
    int desc = tx_next;
    k_memcpy(tx_bufs[desc], data, len);

    /* Padding minimo do Ethernet (64 bytes) */
    if (len < 60) {
        k_memset(tx_bufs[desc] + len, 0, 60 - len);
        len = 60;
    }

    /* Registra endereco e tamanho no descritor */
    rtl_w32((uint8_t)(RTL_TSAD0 + desc * 4), (uint32_t)tx_bufs[desc]);
    /* Escrita no TSD com OWN=0 dispara TX */
    rtl_w32((uint8_t)(RTL_TSD0  + desc * 4), (uint32_t)len);

    /* Aguarda TOK ou TABT (nao OWN, que pode ser 0 antes do hardware iniciar) */
    uint32_t t = 200000;
    uint32_t tsd;
    do {
        tsd = rtl_r32((uint8_t)(RTL_TSD0 + desc * 4));
        if (tsd & (TSD_TOK | TSD_TABT)) break;
        io_wait();
    } while (t--);

    tx_next = (tx_next + 1) % TX_DESC_COUNT;
    return (tsd & TSD_TOK) ? 0 : -1;
}

/* ---- Recepcao (polling) ----------------------------------------- */

int rtl8139_poll(void) {
    if (!rtl_initialized) return 0;

    uint16_t isr = rtl_r16(RTL_ISR);
    if (!(isr & (ISR_ROK | ISR_RER | ISR_RXOVW | ISR_FOVW))) return 0;

    /* Limpa ISR */
    rtl_w16(RTL_ISR, isr);

    int frames = 0;

    while (!(rtl_r8(RTL_CR) & CR_BUFE)) {
        /* Header do pacote no ring buffer: [status 16b][length 16b] */
        uint8_t *header = rx_buf + (rx_read_ptr % (RX_BUF_SIZE - 1500));
        uint16_t rx_status = (uint16_t)(header[0] | (header[1] << 8));
        uint16_t rx_len    = (uint16_t)(header[2] | (header[3] << 8));

        if (!(rx_status & 0x0001) || rx_len == 0 || rx_len > 1518) {
            /* Reset do ponteiro em caso de erro */
            rtl_w16(RTL_CAPR, (uint16_t)(rtl_r16(RTL_CBR) - 16));
            rx_read_ptr = rtl_r16(RTL_CBR);
            break;
        }

        /* Tamanho sem CRC (4 bytes) */
        uint16_t pkt_len = (uint16_t)(rx_len - 4);

        if (recv_cb && pkt_len <= NETBUF_SIZE) {
            netbuf_t *buf = netbuf_alloc();
            if (buf) {
                /* Lida com wrap: copia em duas partes se necessario */
                uint32_t off = (uint32_t)(rx_read_ptr + 4) % (RX_BUF_SIZE - 1500);
                if (off + pkt_len <= RX_BUF_SIZE - 1500) {
                    k_memcpy(buf->data, rx_buf + off, pkt_len);
                } else {
                    uint16_t part1 = (uint16_t)(RX_BUF_SIZE - 1500 - off);
                    k_memcpy(buf->data, rx_buf + off, part1);
                    k_memcpy(buf->data + part1, rx_buf, pkt_len - part1);
                }
                buf->len    = pkt_len;
                buf->offset = 0;
                recv_cb(buf);
                frames++;
            }
        }

        /* Avanca o ponteiro: +4 (header) + pkt_len, alinhado a 4 bytes */
        rx_read_ptr = (uint16_t)(rx_read_ptr + 4 + rx_len);
        rx_read_ptr = (uint16_t)((rx_read_ptr + 3) & ~3);  /* align 4 */

        /* Atualiza CAPR (menos 16 bytes, spec do RTL8139) */
        rtl_w16(RTL_CAPR, (uint16_t)(rx_read_ptr - 16));
    }

    return frames;
}

void rtl8139_set_recv_cb(net_recv_cb_t cb) { recv_cb = cb; }
const uint8_t *rtl8139_get_mac(void) { return mac_addr; }
int rtl8139_ready(void) { return rtl_initialized; }
