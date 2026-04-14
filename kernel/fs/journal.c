#include "journal.h"
#include "../disk/ata.h"
#include "../disk/kstring.h"

/* ---- Layout ------------------------------------------------------- */
#define J_META_SECTOR  35u   /* setor do meta do journal */
#define J_DATA_START   36u   /* primeiro setor de entrada */
#define J_MAX_ENTRIES  31u   /* (97 - 36 + 1) / 2 */
#define J_ENTRY_SECS   2u    /* setores por entrada: 1 meta + 1 dados */

/* Setor meta do journal */
typedef struct {
    uint32_t magic;      /* 0x4A524E4C */
    uint32_t head;       /* primeira entrada nao commitada */
    uint32_t tail;       /* proxima posicao livre */
    uint32_t next_txid;
    uint8_t  _pad[496];
} __attribute__((packed)) j_meta_t;

/* Cabecalho de cada entrada (1 setor) */
typedef struct {
    uint32_t txid;
    uint32_t block_num;
    uint32_t committed;  /* 0 = pendente, 1 = commitado */
    uint32_t checksum;   /* djb2 dos dados */
    uint8_t  _pad[496];
} __attribute__((packed)) j_entry_hdr_t;

#define J_MAGIC 0x4A524E4Cu

static j_meta_t    jmeta;
static int         j_ok = 0;

static uint32_t djb2(const uint8_t *d, uint32_t len) {
    uint32_t h = 5381;
    for (uint32_t i = 0; i < len; i++) h = ((h << 5) + h) + d[i];
    return h;
}

static void j_flush_meta(void) {
    ata_gravar1(J_META_SECTOR, 1, (uint8_t *)&jmeta);
}

static uint32_t j_entry_sector(uint32_t idx) {
    return J_DATA_START + (idx % J_MAX_ENTRIES) * J_ENTRY_SECS;
}

void journal_init(void) {
    if (!ata_presente1()) return;
    if (ata_ler1(J_META_SECTOR, 1, (uint8_t *)&jmeta) != 0) return;
    if (jmeta.magic != J_MAGIC) {
        /* Inicializa journal limpo */
        k_memset(&jmeta, 0, sizeof(jmeta));
        jmeta.magic = J_MAGIC;
        j_flush_meta();
    }
    j_ok = 1;
}

void journal_recover(void) {
    if (!j_ok) return;
    static uint8_t data[512];
    j_entry_hdr_t hdr;

    uint32_t idx = jmeta.head;
    while (idx != jmeta.tail) {
        uint32_t sec = j_entry_sector(idx);
        if (ata_ler1(sec, 1, (uint8_t *)&hdr) != 0) break;
        if (!hdr.committed) {
            /* Entrada nao commitada — replay: aplica dados ao bloco */
            if (ata_ler1(sec + 1, 1, data) == 0) {
                ata_gravar1(hdr.block_num, 1, data);
            }
        }
        idx = (idx + 1) % J_MAX_ENTRIES;
    }
    /* Limpa journal apos recovery */
    jmeta.head = jmeta.tail;
    j_flush_meta();
}

uint32_t journal_begin(uint32_t block_num, const uint8_t *new_data) {
    if (!j_ok) return 0;

    uint32_t idx = jmeta.tail;
    uint32_t sec = j_entry_sector(idx);

    j_entry_hdr_t hdr;
    hdr.txid      = jmeta.next_txid;
    hdr.block_num = block_num;
    hdr.committed = 0;
    hdr.checksum  = djb2(new_data, 512);
    k_memset(hdr._pad, 0, sizeof(hdr._pad));

    ata_gravar1(sec,     1, (uint8_t *)&hdr);
    ata_gravar1(sec + 1, 1, (uint8_t *)new_data);

    jmeta.tail = (jmeta.tail + 1) % J_MAX_ENTRIES;
    jmeta.next_txid++;
    j_flush_meta();

    return hdr.txid;
}

void journal_commit(uint32_t tx_id) {
    if (!j_ok) return;

    /* Encontra entrada pelo txid */
    j_entry_hdr_t hdr;
    for (uint32_t i = 0; i < J_MAX_ENTRIES; i++) {
        uint32_t sec = j_entry_sector(i);
        if (ata_ler1(sec, 1, (uint8_t *)&hdr) != 0) continue;
        if (hdr.txid == tx_id && !hdr.committed) {
            hdr.committed = 1;
            ata_gravar1(sec, 1, (uint8_t *)&hdr);
            break;
        }
    }
    /* Avanca head ate a primeira entrada nao-commitada */
    while (jmeta.head != jmeta.tail) {
        uint32_t sec = j_entry_sector(jmeta.head);
        if (ata_ler1(sec, 1, (uint8_t *)&hdr) != 0) break;
        if (!hdr.committed) break;
        jmeta.head = (jmeta.head + 1) % J_MAX_ENTRIES;
    }
    j_flush_meta();
}
