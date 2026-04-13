#include "fat32.h"
#include "../disk/ata.h"
#include "../disk/kstring.h"
#include "../mem/kheap.h"

/* -----------------------------------------------------------------------
 * BPB — Boot Parameter Block (setor 0, offsets conforme spec FAT32)
 * ----------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  jump[3];           /* 0x00 — instrucao de boot */
    char     oem[8];            /* 0x03 — nome do OEM */
    uint16_t bytes_per_sector;  /* 0x0B — sempre 512 para discos fisicos */
    uint8_t  spc;               /* 0x0D — setores por cluster (potencia de 2) */
    uint16_t reserved_sectors;  /* 0x0E — setores antes da FAT (inclui setor 0) */
    uint8_t  num_fats;          /* 0x10 — numero de copias da FAT (normalmente 2) */
    uint16_t root_entry_count;  /* 0x11 — 0 para FAT32 */
    uint16_t total_sectors16;   /* 0x13 — 0 para FAT32 (usa campo 32-bit) */
    uint8_t  media;             /* 0x15 — tipo de midia */
    uint16_t fat_size16;        /* 0x16 — 0 para FAT32 */
    uint16_t sectors_per_track; /* 0x18 */
    uint16_t num_heads;         /* 0x1A */
    uint32_t hidden_sectors;    /* 0x1C */
    uint32_t total_sectors32;   /* 0x20 */
    /* FAT32-especifico a partir daqui */
    uint32_t fat_size32;        /* 0x24 — setores por copia da FAT */
    uint16_t ext_flags;         /* 0x28 */
    uint16_t fs_version;        /* 0x2A */
    uint32_t root_cluster;      /* 0x2C — cluster do diretorio raiz (normalmente 2) */
    uint16_t fs_info;           /* 0x30 */
    uint16_t backup_boot;       /* 0x32 */
    uint8_t  _reserved[12];     /* 0x34 */
    uint8_t  drive_number;      /* 0x40 */
    uint8_t  _reserved2;        /* 0x41 */
    uint8_t  boot_sig;          /* 0x42 */
    uint32_t volume_id;         /* 0x43 */
    char     volume_label[11];  /* 0x47 */
    char     fs_type[8];        /* 0x52 — "FAT32   " */
} fat32_bpb_raw_t;

/* Entrada de diretorio de 32 bytes (layout exato em disco) */
typedef struct __attribute__((packed)) {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  _nt_reserved;
    uint8_t  ctime_tenth;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t cluster_lo;
    uint32_t size;
} fat32_dirent_raw_t;

/* -----------------------------------------------------------------------
 * Estado interno do driver (preenchido em fat32_init)
 * ----------------------------------------------------------------------- */
static struct {
    int      inicializado;
    uint16_t bps;               /* bytes por setor (512) */
    uint8_t  spc;               /* setores por cluster */
    uint32_t fat_lba;           /* LBA do inicio da FAT */
    uint32_t data_lba;          /* LBA do inicio da regiao de dados */
    uint32_t root_cluster;      /* cluster do diretorio raiz */
    uint32_t total_clusters;    /* total de clusters de dados */
} bpb;

/* Buffer estatico para leituras de setor (evita kmalloc em hot paths) */
static uint8_t fat32_buf[512];

/* -----------------------------------------------------------------------
 * Primitivas internas
 * ----------------------------------------------------------------------- */

/* Converte numero de cluster para LBA do primeiro setor */
static uint32_t cluster_to_lba(uint32_t cluster) {
    return bpb.data_lba + (cluster - 2) * bpb.spc;
}

/* Le o valor da FAT para o cluster N (segue a cadeia) */
static uint32_t fat32_read_fat(uint32_t cluster) {
    uint32_t byte_offset = cluster * 4;
    uint32_t fat_sector  = bpb.fat_lba + byte_offset / 512;
    uint32_t fat_offset  = byte_offset % 512;

    if (ata_ler(fat_sector, 1, fat32_buf) != 0) return 0x0FFFFFFF;
    uint32_t val = *(uint32_t *)(fat32_buf + fat_offset);
    return val & 0x0FFFFFFF;
}

/* Escreve um valor na FAT para o cluster N (read-modify-write) */
static int fat32_write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t byte_offset = cluster * 4;
    uint32_t fat_sector  = bpb.fat_lba + byte_offset / 512;
    uint32_t fat_offset  = byte_offset % 512;

    /* Lemos o setor, modificamos 4 bytes, gravamos de volta */
    if (ata_ler(fat_sector, 1, fat32_buf) != 0) return -1;
    *(uint32_t *)(fat32_buf + fat_offset) = value & 0x0FFFFFFF;
    if (ata_gravar(fat_sector, 1, fat32_buf) != 0) return -1;

    /* Atualiza a copia de backup da FAT (num_fats normalmente = 2) */
    uint32_t backup_sector = fat_sector + (bpb.fat_lba +
        (bpb.data_lba - bpb.fat_lba) / 2 - bpb.fat_lba);
    /* Simplificado: a segunda FAT fica fat_size setores depois da primeira */
    /* Calculado no init: fat2_lba = fat_lba + fat_size */
    /* Por ora, apenas a FAT primaria — suficiente para QEMU */
    (void)backup_sector;

    return 0;
}

/* Reconstroi nome legivel a partir dos campos name[8] + ext[3] da entrada raw */
static void reconstruct_name(const fat32_dirent_raw_t *e, char *out) {
    int i = 0, j = 0;

    /* Nome: ate 8 chars, sem espacos finais */
    for (i = 0; i < 8 && e->name[i] != ' '; i++)
        out[j++] = e->name[i];

    /* Extensao: ate 3 chars, sem espacos finais */
    int has_ext = 0;
    for (i = 0; i < 3; i++) {
        if (e->ext[i] != ' ') { has_ext = 1; break; }
    }
    if (has_ext) {
        out[j++] = '.';
        for (i = 0; i < 3 && e->ext[i] != ' '; i++)
            out[j++] = e->ext[i];
    }

    out[j] = '\0';
}

/* -----------------------------------------------------------------------
 * fat32_normalize — converte "arquivo.txt" → "ARQUIVO TXT" (11 chars, sem ponto)
 * ----------------------------------------------------------------------- */
void fat32_normalize(const char *filename, char *out) {
    /* Preenche com espacos */
    k_memset(out, ' ', 11);
    out[11] = '\0';

    int i = 0, j = 0;
    /* Copia nome (antes do ponto), ate 8 chars, maiusculo */
    while (filename[i] && filename[i] != '.' && j < 8) {
        out[j++] = k_toupper(filename[i++]);
    }
    /* Pula o ponto */
    if (filename[i] == '.') i++;
    /* Copia extensao, ate 3 chars, maiusculo */
    j = 8;
    while (filename[i] && j < 11) {
        out[j++] = k_toupper(filename[i++]);
    }
}

/* -----------------------------------------------------------------------
 * fat32_init
 * ----------------------------------------------------------------------- */
int fat32_init(void) {
    uint8_t setor0[512];
    if (ata_ler(0, 1, setor0) != 0) return -1;

    fat32_bpb_raw_t *raw = (fat32_bpb_raw_t *)setor0;

    /* Valida assinatura FAT32: campo fs_type deve comecar com "FAT" */
    if (k_strncmp(raw->fs_type, "FAT32   ", 5) != 0) {
        /* Tenta verificar a assinatura de boot 0x55AA como fallback */
        if (setor0[510] != 0x55 || setor0[511] != 0xAA) return -1;
        /* Se nao tem fs_type correto, pode ser FAT16 ou outro — rejeita */
        if (raw->fat_size32 == 0) return -1;
    }

    bpb.bps          = raw->bytes_per_sector;
    bpb.spc          = raw->spc;
    bpb.fat_lba      = raw->reserved_sectors;
    bpb.root_cluster = raw->root_cluster;

    /* data_lba = fat_lba + (num_fats * fat_size) */
    bpb.data_lba = bpb.fat_lba + (raw->num_fats * raw->fat_size32);

    uint32_t total_sectors = raw->total_sectors32;
    bpb.total_clusters = (total_sectors - bpb.data_lba) / bpb.spc;

    bpb.inicializado = 1;
    return 0;
}

uint32_t fat32_root_cluster(void) {
    return bpb.root_cluster;
}

/* -----------------------------------------------------------------------
 * fat32_readdir
 * ----------------------------------------------------------------------- */
int fat32_readdir(uint32_t dir_cluster, fat32_entry_t *entries, int max) {
    if (!bpb.inicializado) return -1;

    int count = 0;
    uint32_t cluster = dir_cluster;

    while (cluster < FAT32_EOF && count < max) {
        uint32_t lba = cluster_to_lba(cluster);

        /* Itera sobre os setores do cluster */
        for (uint8_t s = 0; s < bpb.spc && count < max; s++) {
            if (ata_ler(lba + s, 1, fat32_buf) != 0) return -1;

            /* Cada setor tem 512/32 = 16 entradas de diretorio */
            fat32_dirent_raw_t *dir = (fat32_dirent_raw_t *)fat32_buf;
            for (int i = 0; i < 16 && count < max; i++) {
                uint8_t first = (uint8_t)dir[i].name[0];

                if (first == 0x00) goto done;  /* sem mais entradas */
                if (first == 0xE5) continue;   /* entrada deletada */
                if (dir[i].attr == FAT32_ATTR_LFN) continue;  /* LFN, pula */
                if (dir[i].attr & FAT32_ATTR_VOLUME) continue; /* label, pula */

                fat32_entry_t *e = &entries[count++];
                reconstruct_name(&dir[i], e->name);
                e->is_dir        = (dir[i].attr & FAT32_ATTR_DIR) ? 1 : 0;
                e->first_cluster = ((uint32_t)dir[i].cluster_hi << 16) | dir[i].cluster_lo;
                e->size          = dir[i].size;
            }
        }

        cluster = fat32_read_fat(cluster);
    }

done:
    return count;
}

/* -----------------------------------------------------------------------
 * fat32_find
 * ----------------------------------------------------------------------- */
int fat32_find(uint32_t dir_cluster, const char *name11, fat32_entry_t *out) {
    if (!bpb.inicializado) return -1;

    uint32_t cluster = dir_cluster;

    while (cluster < FAT32_EOF) {
        uint32_t lba = cluster_to_lba(cluster);

        for (uint8_t s = 0; s < bpb.spc; s++) {
            if (ata_ler(lba + s, 1, fat32_buf) != 0) return -1;

            fat32_dirent_raw_t *dir = (fat32_dirent_raw_t *)fat32_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = (uint8_t)dir[i].name[0];
                if (first == 0x00) return -1;
                if (first == 0xE5) continue;
                if (dir[i].attr == FAT32_ATTR_LFN) continue;
                if (dir[i].attr & FAT32_ATTR_VOLUME) continue;

                /* Compara os 11 bytes do nome diretamente */
                char raw11[12];
                k_memcpy(raw11, dir[i].name, 8);
                k_memcpy(raw11 + 8, dir[i].ext, 3);
                raw11[11] = '\0';

                if (k_strncmp(raw11, name11, 11) == 0) {
                    reconstruct_name(&dir[i], out->name);
                    out->is_dir        = (dir[i].attr & FAT32_ATTR_DIR) ? 1 : 0;
                    out->first_cluster = ((uint32_t)dir[i].cluster_hi << 16) | dir[i].cluster_lo;
                    out->size          = dir[i].size;
                    return 0;
                }
            }
        }

        cluster = fat32_read_fat(cluster);
    }

    return -1;
}

/* -----------------------------------------------------------------------
 * fat32_read
 * ----------------------------------------------------------------------- */
int fat32_read(uint32_t first_cluster, uint32_t size, uint8_t *buf) {
    if (!bpb.inicializado) return -1;

    uint32_t cluster = first_cluster;
    uint32_t bytes_read = 0;
    uint32_t cluster_bytes = bpb.spc * 512;

    while (cluster < FAT32_EOF && bytes_read < size) {
        uint32_t lba = cluster_to_lba(cluster);

        for (uint8_t s = 0; s < bpb.spc; s++) {
            uint32_t to_read = size - bytes_read;
            if (to_read == 0) break;

            if (ata_ler(lba + s, 1, fat32_buf) != 0) return -1;

            uint32_t chunk = (to_read < 512) ? to_read : 512;
            k_memcpy(buf + bytes_read, fat32_buf, chunk);
            bytes_read += chunk;
        }

        (void)cluster_bytes;
        cluster = fat32_read_fat(cluster);
    }

    return (int)bytes_read;
}

/* -----------------------------------------------------------------------
 * fat32_alloc_cluster
 * ----------------------------------------------------------------------- */
uint32_t fat32_alloc_cluster(void) {
    if (!bpb.inicializado) return 0;

    /* Scan da FAT procurando entrada livre (valor 0x00000000) */
    for (uint32_t c = 2; c < bpb.total_clusters + 2; c++) {
        if (fat32_read_fat(c) == 0) {
            /* Marca como EOF (arquivo de um unico cluster por ora) */
            fat32_write_fat_entry(c, 0x0FFFFFFF);
            return c;
        }
    }
    return 0;  /* disco cheio */
}

/* -----------------------------------------------------------------------
 * fat32_write_file
 * ----------------------------------------------------------------------- */
int fat32_write_file(uint32_t first_cluster, const uint8_t *buf, uint32_t size) {
    if (!bpb.inicializado) return -1;

    uint32_t cluster = first_cluster;
    uint32_t bytes_written = 0;
    uint32_t prev_cluster = 0;

    while (bytes_written < size) {
        /* Se nao temos cluster, aloca um novo */
        if (cluster == 0 || cluster >= FAT32_EOF) {
            uint32_t new_c = fat32_alloc_cluster();
            if (new_c == 0) return -1;

            /* Encadeia no cluster anterior */
            if (prev_cluster != 0)
                fat32_write_fat_entry(prev_cluster, new_c);

            cluster = new_c;
        }

        uint32_t lba = cluster_to_lba(cluster);

        for (uint8_t s = 0; s < bpb.spc && bytes_written < size; s++) {
            uint32_t chunk = size - bytes_written;
            if (chunk > 512) chunk = 512;

            /* Copia dados para buffer, zera restante do setor */
            k_memset(fat32_buf, 0, 512);
            k_memcpy(fat32_buf, buf + bytes_written, chunk);

            if (ata_gravar(lba + s, 1, fat32_buf) != 0) return -1;
            bytes_written += chunk;
        }

        prev_cluster = cluster;
        cluster = fat32_read_fat(cluster);
    }

    return (int)bytes_written;
}

/* -----------------------------------------------------------------------
 * fat32_create — cria entrada de diretorio no pai
 * ----------------------------------------------------------------------- */
uint32_t fat32_create(uint32_t parent_cluster, const char *name11, uint8_t attr) {
    if (!bpb.inicializado) return 0;

    /* Aloca cluster para o novo arquivo/diretorio */
    uint32_t new_cluster = fat32_alloc_cluster();
    if (new_cluster == 0) return 0;

    /* Zera o cluster (importante para diretorios: indica "sem mais entradas") */
    uint32_t lba = cluster_to_lba(new_cluster);
    k_memset(fat32_buf, 0, 512);
    for (uint8_t s = 0; s < bpb.spc; s++)
        ata_gravar(lba + s, 1, fat32_buf);

    /* Para diretorios, cria entradas "." e ".." */
    if (attr & FAT32_ATTR_DIR) {
        if (ata_ler(lba, 1, fat32_buf) != 0) return 0;
        fat32_dirent_raw_t *dot = (fat32_dirent_raw_t *)fat32_buf;

        /* Entrada "." */
        k_memset(dot, 0, sizeof(fat32_dirent_raw_t));
        k_memset(dot->name, ' ', 8);
        k_memset(dot->ext, ' ', 3);
        dot->name[0]    = '.';
        dot->attr       = FAT32_ATTR_DIR;
        dot->cluster_hi = (uint16_t)(new_cluster >> 16);
        dot->cluster_lo = (uint16_t)(new_cluster & 0xFFFF);

        /* Entrada ".." */
        fat32_dirent_raw_t *dotdot = dot + 1;
        k_memset(dotdot, 0, sizeof(fat32_dirent_raw_t));
        k_memset(dotdot->name, ' ', 8);
        k_memset(dotdot->ext, ' ', 3);
        dotdot->name[0]    = '.';
        dotdot->name[1]    = '.';
        dotdot->attr       = FAT32_ATTR_DIR;
        dotdot->cluster_hi = (uint16_t)(parent_cluster >> 16);
        dotdot->cluster_lo = (uint16_t)(parent_cluster & 0xFFFF);

        ata_gravar(lba, 1, fat32_buf);
    }

    /* Procura slot livre no diretorio pai para escrever a nova entrada */
    uint32_t cluster = parent_cluster;
    while (cluster < FAT32_EOF) {
        uint32_t plba = cluster_to_lba(cluster);

        for (uint8_t s = 0; s < bpb.spc; s++) {
            if (ata_ler(plba + s, 1, fat32_buf) != 0) return 0;

            fat32_dirent_raw_t *dir = (fat32_dirent_raw_t *)fat32_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = (uint8_t)dir[i].name[0];
                /* Slot livre: nunca usado (0x00) ou deletado (0xE5) */
                if (first == 0x00 || first == 0xE5) {
                    k_memset(&dir[i], 0, sizeof(fat32_dirent_raw_t));
                    k_memcpy(dir[i].name, name11, 8);
                    k_memcpy(dir[i].ext, name11 + 8, 3);
                    dir[i].attr       = attr;
                    dir[i].cluster_hi = (uint16_t)(new_cluster >> 16);
                    dir[i].cluster_lo = (uint16_t)(new_cluster & 0xFFFF);
                    dir[i].size       = 0;

                    ata_gravar(plba + s, 1, fat32_buf);
                    return new_cluster;
                }
            }
        }

        cluster = fat32_read_fat(cluster);
    }

    /* Sem espaco no diretorio pai (precisaria expandi-lo — nao implementado) */
    return 0;
}

/* -----------------------------------------------------------------------
 * fat32_delete
 * ----------------------------------------------------------------------- */
int fat32_delete(uint32_t parent_cluster, const char *name11) {
    if (!bpb.inicializado) return -1;

    uint32_t cluster = parent_cluster;
    while (cluster < FAT32_EOF) {
        uint32_t lba = cluster_to_lba(cluster);

        for (uint8_t s = 0; s < bpb.spc; s++) {
            if (ata_ler(lba + s, 1, fat32_buf) != 0) return -1;

            fat32_dirent_raw_t *dir = (fat32_dirent_raw_t *)fat32_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = (uint8_t)dir[i].name[0];
                if (first == 0x00) return -1;
                if (first == 0xE5) continue;
                if (dir[i].attr == FAT32_ATTR_LFN) continue;

                char raw11[12];
                k_memcpy(raw11, dir[i].name, 8);
                k_memcpy(raw11 + 8, dir[i].ext, 3);
                raw11[11] = '\0';

                if (k_strncmp(raw11, name11, 11) == 0) {
                    uint32_t fc = ((uint32_t)dir[i].cluster_hi << 16) | dir[i].cluster_lo;

                    /* Marca entrada como deletada */
                    dir[i].name[0] = (char)0xE5;
                    ata_gravar(lba + s, 1, fat32_buf);

                    /* Libera cadeia FAT */
                    uint32_t c = fc;
                    while (c >= 2 && c < FAT32_EOF) {
                        uint32_t next = fat32_read_fat(c);
                        fat32_write_fat_entry(c, 0x00000000);
                        c = next;
                    }

                    return 0;
                }
            }
        }

        cluster = fat32_read_fat(cluster);
    }

    return -1;
}

/* -----------------------------------------------------------------------
 * fat32_rename — renomeia entrada no mesmo diretorio
 * ----------------------------------------------------------------------- */
int fat32_rename(uint32_t dir_cluster, const char *old_name11, const char *new_name11) {
    if (!bpb.inicializado) return -1;

    uint32_t cluster = dir_cluster;
    while (cluster < FAT32_EOF) {
        uint32_t lba = cluster_to_lba(cluster);

        for (uint8_t s = 0; s < bpb.spc; s++) {
            if (ata_ler(lba + s, 1, fat32_buf) != 0) return -1;

            fat32_dirent_raw_t *dir = (fat32_dirent_raw_t *)fat32_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = (uint8_t)dir[i].name[0];
                if (first == 0x00) return -1;
                if (first == 0xE5) continue;
                if (dir[i].attr == FAT32_ATTR_LFN) continue;

                char raw11[12];
                k_memcpy(raw11, dir[i].name, 8);
                k_memcpy(raw11 + 8, dir[i].ext, 3);
                raw11[11] = '\0';

                if (k_strncmp(raw11, old_name11, 11) == 0) {
                    k_memcpy(dir[i].name, new_name11, 8);
                    k_memcpy(dir[i].ext, new_name11 + 8, 3);
                    ata_gravar(lba + s, 1, fat32_buf);
                    return 0;
                }
            }
        }

        cluster = fat32_read_fat(cluster);
    }

    return -1;
}
