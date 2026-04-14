#include "spacefs.h"
#include "journal.h"
#include "../disk/ata.h"
#include "../disk/kstring.h"
#include "../shell/shell.h"
#include "../proc/sched.h"

/* ---- Setores fixos ------------------------------------------------ */
#define SFS_SUPER_SEC     0u
#define SFS_IMAP_SEC      1u
#define SFS_BMAP_SEC      2u
#define SFS_INODE_SEC     3u    /* 3..34 = 32 setores = 256 inodes × 64B */
#define SFS_JOURNAL_SEC   35u
#define SFS_DATA_START    99u

#define SFS_INODES_PER_SEC  (512u / sizeof(sfs_inode_t))  /* = 8 */
#define SFS_DIRENTS_PER_BLK (512u / sizeof(sfs_dirent_t)) /* = 16 */

/* ---- Estado ------------------------------------------------------- */
static sfs_super_t  sb;
static uint8_t      imap[512];  /* bitmap de inodes (256 bits = 32 bytes) */
static uint8_t      bmap[512];  /* bitmap de blocos */
static int          sfs_ok = 0;

/* ---- Leitura/escrita de setor (disco 1) com journal -------------- */

static int sfs_read_sec(uint32_t sec, uint8_t *buf) {
    return ata_ler1(sec, 1, buf);
}

static int sfs_write_sec(uint32_t sec, const uint8_t *buf) {
    uint32_t txid = journal_begin(sec, buf);
    int ret = ata_gravar1(sec, 1, (uint8_t *)buf);
    journal_commit(txid);
    return ret;
}

/* ---- Bitmap helpers ---------------------------------------------- */

static int bm_test(const uint8_t *bm, uint32_t bit) {
    return (bm[bit / 8] >> (bit % 8)) & 1;
}
static void bm_set(uint8_t *bm, uint32_t bit) {
    bm[bit / 8] |= (uint8_t)(1u << (bit % 8));
}
static void bm_clr(uint8_t *bm, uint32_t bit) {
    bm[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

/* ---- Inode I/O ---------------------------------------------------- */

static int inode_read(uint32_t inum, sfs_inode_t *out) {
    static uint8_t sec_buf[512];
    uint32_t sec = SFS_INODE_SEC + inum / SFS_INODES_PER_SEC;
    if (sfs_read_sec(sec, sec_buf) != 0) return -1;
    uint32_t off = (inum % SFS_INODES_PER_SEC) * sizeof(sfs_inode_t);
    k_memcpy(out, sec_buf + off, sizeof(sfs_inode_t));
    return 0;
}

static int inode_write(uint32_t inum, const sfs_inode_t *in) {
    static uint8_t sec_buf[512];
    uint32_t sec = SFS_INODE_SEC + inum / SFS_INODES_PER_SEC;
    if (sfs_read_sec(sec, sec_buf) != 0) return -1;
    uint32_t off = (inum % SFS_INODES_PER_SEC) * sizeof(sfs_inode_t);
    k_memcpy(sec_buf + off, in, sizeof(sfs_inode_t));
    return sfs_write_sec(sec, sec_buf);
}

/* ---- Alocacao ----------------------------------------------------- */

static uint32_t alloc_inode(void) {
    for (uint32_t i = 1; i < SFS_INODE_MAX; i++) {
        if (!bm_test(imap, i)) {
            bm_set(imap, i);
            sfs_write_sec(SFS_IMAP_SEC, imap);
            sb.free_inodes--;
            sfs_write_sec(SFS_SUPER_SEC, (uint8_t *)&sb);
            return i;
        }
    }
    return (uint32_t)-1;
}

static uint32_t alloc_block(void) {
    uint32_t max = sb.total_blocks < 512 * 8 ? sb.total_blocks : 512 * 8;
    for (uint32_t i = 0; i < max; i++) {
        if (!bm_test(bmap, i)) {
            bm_set(bmap, i);
            sfs_write_sec(SFS_BMAP_SEC, bmap);
            sb.free_blocks--;
            sfs_write_sec(SFS_SUPER_SEC, (uint8_t *)&sb);
            return SFS_DATA_START + i;
        }
    }
    return (uint32_t)-1;
}

static void free_block(uint32_t sec) {
    if (sec < SFS_DATA_START) return;
    uint32_t idx = sec - SFS_DATA_START;
    bm_clr(bmap, idx);
    sfs_write_sec(SFS_BMAP_SEC, bmap);
    sb.free_blocks++;
    sfs_write_sec(SFS_SUPER_SEC, (uint8_t *)&sb);
}

/* ---- Resolucao de caminho ---------------------------------------- */

static int path_find(const char *path, uint32_t *inum_out, uint32_t *parent_out) {
    uint32_t cur = sb.root_inode;
    uint32_t parent = cur;

    if (path[0] == '/') path++;

    char buf[128];
    k_strncpy(buf, path, 127); buf[127] = '\0';

    char *tok = buf;
    while (*tok) {
        char *sep = tok;
        while (*sep && *sep != '/') sep++;
        int more = (*sep == '/');
        *sep = '\0';

        if (k_strlen(tok) == 0) { tok = sep + (more ? 1 : 0); continue; }

        /* Busca 'tok' no diretorio 'cur' */
        sfs_inode_t dir;
        if (inode_read(cur, &dir) != 0) return -1;
        if (dir.type != 2) return -1;  /* nao e diretorio */

        int found = 0;
        static uint8_t dblk[512];
        uint32_t entries = dir.size / sizeof(sfs_dirent_t);
        uint32_t blk_idx = 0;

        while (entries > 0) {
            uint32_t sec = (blk_idx < 10) ? dir.blocks[blk_idx] : 0;
            if (!sec) break;
            if (sfs_read_sec(sec, dblk) != 0) break;

            uint32_t per = SFS_DIRENTS_PER_BLK;
            sfs_dirent_t *de = (sfs_dirent_t *)dblk;
            for (uint32_t i = 0; i < per && entries > 0; i++, entries--) {
                if (!de[i].inode) continue;
                if (k_strcmp(de[i].name, tok) == 0) {
                    parent = cur;
                    cur = de[i].inode;
                    found = 1;
                    break;
                }
            }
            if (found) break;
            blk_idx++;
        }

        if (!found) return -1;
        tok = sep + (more ? 1 : 0);
    }

    if (inum_out)   *inum_out   = cur;
    if (parent_out) *parent_out = parent;
    return 0;
}

/* Retorna nome do ultimo componente e resolve parent */
static const char *path_split(const char *path, char *parent_buf) {
    int len = (int)k_strlen(path);
    k_strncpy(parent_buf, path, 127); parent_buf[127] = '\0';

    for (int i = len - 1; i >= 0; i--) {
        if (parent_buf[i] == '/') {
            parent_buf[i] = '\0';
            return path + i + 1;
        }
    }
    parent_buf[0] = '\0';
    return path;
}

/* ---- Criar entrada de diretorio ---------------------------------- */

static int dir_add_entry(uint32_t dir_inum, uint32_t child_inum, const char *name) {
    sfs_inode_t dir;
    if (inode_read(dir_inum, &dir) != 0) return -1;

    static uint8_t dblk[512];
    uint32_t entries = dir.size / sizeof(sfs_dirent_t);
    uint32_t blk_idx = 0;

    while (1) {
        uint32_t sec;
        if (blk_idx < 10) {
            if (!dir.blocks[blk_idx]) {
                /* Aloca novo bloco */
                sec = alloc_block();
                if (sec == (uint32_t)-1) return -1;
                dir.blocks[blk_idx] = sec;
                k_memset(dblk, 0, 512);
            } else {
                sec = dir.blocks[blk_idx];
                if (sfs_read_sec(sec, dblk) != 0) return -1;
            }
        } else return -1;  /* sem espaco */

        /* Procura slot livre */
        sfs_dirent_t *de = (sfs_dirent_t *)dblk;
        for (uint32_t i = 0; i < SFS_DIRENTS_PER_BLK; i++) {
            if (!de[i].inode) {
                de[i].inode = child_inum;
                k_strncpy(de[i].name, name, SFS_NAME_MAX);
                de[i].name[SFS_NAME_MAX] = '\0';
                sfs_write_sec(sec, dblk);
                dir.size += sizeof(sfs_dirent_t);
                inode_write(dir_inum, &dir);
                return 0;
            }
        }
        blk_idx++;
    }
}

/* ---- API publica -------------------------------------------------- */

int spfs_format(void) {
    if (!ata_presente1()) return -1;

    /* Superbloco */
    k_memset(&sb, 0, sizeof(sb));
    sb.magic       = SFS_MAGIC;
    sb.version     = SFS_VERSION;
    sb.total_blocks = 65536 - SFS_DATA_START;  /* disco de 32MB = 65536 setores */
    sb.free_blocks = sb.total_blocks;
    sb.inode_count = SFS_INODE_MAX;
    sb.free_inodes = SFS_INODE_MAX - 1;  /* inode 0 = root */
    sb.data_start  = SFS_DATA_START;
    sb.root_inode  = 0;
    ata_gravar1(SFS_SUPER_SEC, 1, (uint8_t *)&sb);

    /* Bitmaps */
    k_memset(imap, 0, 512);
    bm_set(imap, 0);  /* inode 0 = root reservado */
    ata_gravar1(SFS_IMAP_SEC, 1, imap);

    k_memset(bmap, 0, 512);
    ata_gravar1(SFS_BMAP_SEC, 1, bmap);

    /* Inodes — zera tabela */
    static uint8_t zero[512];
    k_memset(zero, 0, 512);
    for (uint32_t s = SFS_INODE_SEC; s < SFS_INODE_SEC + 32; s++)
        ata_gravar1(s, 1, zero);

    /* Inode 0 = root directory */
    sfs_inode_t root;
    k_memset(&root, 0, sizeof(root));
    root.type = 2;  /* diretorio */
    root.nlinks = 1;
    root.mtime = 0;
    ata_gravar1(SFS_INODE_SEC, 1, (uint8_t *)&root);  /* inode 0 esta no setor 3, offset 0 */

    journal_init();  /* inicializa journal limpo */

    shell_writeln("SpaceFS: formatado com sucesso");
    return 0;
}

int spfs_mount(void) {
    if (!ata_presente1()) return -1;
    if (ata_ler1(SFS_SUPER_SEC, 1, (uint8_t *)&sb) != 0) return -1;
    if (sb.magic != SFS_MAGIC) return -1;

    ata_ler1(SFS_IMAP_SEC, 1, imap);
    ata_ler1(SFS_BMAP_SEC, 1, bmap);

    journal_init();
    journal_recover();

    sfs_ok = 1;
    return 0;
}

int spfs_mounted(void) { return sfs_ok; }

int spfs_read(const char *path, uint8_t *buf, uint32_t max) {
    if (!sfs_ok) return -1;
    uint32_t inum;
    if (path_find(path, &inum, 0) != 0) return -1;

    sfs_inode_t in;
    if (inode_read(inum, &in) != 0) return -1;
    if (in.type != 1) return -1;

    uint32_t to_read = (in.size < max) ? in.size : max;
    uint32_t done = 0;
    static uint8_t blk[512];

    for (uint32_t b = 0; b < 10 && done < to_read; b++) {
        if (!in.blocks[b]) break;
        if (sfs_read_sec(in.blocks[b], blk) != 0) break;
        uint32_t chunk = to_read - done;
        if (chunk > 512) chunk = 512;
        k_memcpy(buf + done, blk, chunk);
        done += chunk;
    }
    return (int)done;
}

int spfs_write(const char *path, const uint8_t *buf, uint32_t size) {
    if (!sfs_ok || size > 10 * 512) return -1;  /* max 5KB (direto) */

    char parent_path[128];
    const char *name = path_split(path, parent_path);

    uint32_t parent_inum = 0;
    if (k_strlen(parent_path) > 0) {
        if (path_find(parent_path, &parent_inum, 0) != 0) return -1;
    }

    /* Verifica se ja existe */
    uint32_t existing;
    if (path_find(path, &existing, 0) == 0) {
        sfs_inode_t ex;
        inode_read(existing, &ex);
        /* Libera blocos antigos */
        for (int i = 0; i < 10; i++) {
            if (ex.blocks[i]) free_block(ex.blocks[i]);
        }
        ex.size = 0;
        k_memset(ex.blocks, 0, sizeof(ex.blocks));
        inode_write(existing, &ex);
    } else {
        /* Cria novo inode */
        existing = alloc_inode();
        if (existing == (uint32_t)-1) return -1;
        sfs_inode_t nin;
        k_memset(&nin, 0, sizeof(nin));
        nin.type = 1;
        nin.nlinks = 1;
        nin.mtime = sched_ticks();
        inode_write(existing, &nin);
        dir_add_entry(parent_inum, existing, name);
    }

    /* Escreve dados */
    sfs_inode_t in;
    inode_read(existing, &in);
    in.size = size;

    uint32_t done = 0;
    static uint8_t blk[512];
    for (uint32_t b = 0; b < 10 && done < size; b++) {
        uint32_t sec = alloc_block();
        if (sec == (uint32_t)-1) break;
        in.blocks[b] = sec;
        uint32_t chunk = size - done;
        if (chunk > 512) chunk = 512;
        k_memset(blk, 0, 512);
        k_memcpy(blk, buf + done, chunk);
        sfs_write_sec(sec, blk);
        done += chunk;
    }
    inode_write(existing, &in);
    return (int)done;
}

int spfs_mkdir(const char *path) {
    if (!sfs_ok) return -1;
    char parent_path[128];
    const char *name = path_split(path, parent_path);

    uint32_t parent_inum = 0;
    if (k_strlen(parent_path) > 0) {
        if (path_find(parent_path, &parent_inum, 0) != 0) return -1;
    }

    uint32_t inum = alloc_inode();
    if (inum == (uint32_t)-1) return -1;

    sfs_inode_t nin;
    k_memset(&nin, 0, sizeof(nin));
    nin.type = 2;
    nin.nlinks = 1;
    nin.mtime = sched_ticks();
    inode_write(inum, &nin);
    dir_add_entry(parent_inum, inum, name);
    return 0;
}

int spfs_unlink(const char *path) {
    if (!sfs_ok) return -1;
    uint32_t inum, parent;
    if (path_find(path, &inum, &parent) != 0) return -1;

    sfs_inode_t in;
    if (inode_read(inum, &in) != 0) return -1;
    for (int i = 0; i < 10; i++) if (in.blocks[i]) free_block(in.blocks[i]);
    in.type = 0;
    inode_write(inum, &in);
    bm_clr(imap, inum);
    sfs_write_sec(SFS_IMAP_SEC, imap);
    sb.free_inodes++;
    sfs_write_sec(SFS_SUPER_SEC, (uint8_t *)&sb);

    /* Remove entrada do diretorio pai */
    sfs_inode_t dir;
    if (inode_read(parent, &dir) != 0) return 0;
    static uint8_t dblk[512];
    for (uint32_t b = 0; b < 10; b++) {
        if (!dir.blocks[b]) break;
        if (sfs_read_sec(dir.blocks[b], dblk) != 0) break;
        sfs_dirent_t *de = (sfs_dirent_t *)dblk;
        int changed = 0;
        for (uint32_t i = 0; i < SFS_DIRENTS_PER_BLK; i++) {
            if (de[i].inode == inum) { de[i].inode = 0; changed = 1; break; }
        }
        if (changed) { sfs_write_sec(dir.blocks[b], dblk); break; }
    }
    return 0;
}

int spfs_ls(const char *path) {
    if (!sfs_ok) { shell_writeln("SpaceFS: nao montado"); return -1; }
    uint32_t inum = 0;
    if (path && k_strlen(path) > 0 && !(path[0] == '/' && !path[1])) {
        if (path_find(path, &inum, 0) != 0) { shell_writeln("SpaceFS: caminho nao encontrado"); return -1; }
    }
    sfs_inode_t dir;
    if (inode_read(inum, &dir) != 0) return -1;
    if (dir.type != 2) { shell_writeln("SpaceFS: nao e diretorio"); return -1; }

    static uint8_t dblk[512];
    int count = 0;
    for (uint32_t b = 0; b < 10; b++) {
        if (!dir.blocks[b]) break;
        if (sfs_read_sec(dir.blocks[b], dblk) != 0) break;
        sfs_dirent_t *de = (sfs_dirent_t *)dblk;
        for (uint32_t i = 0; i < SFS_DIRENTS_PER_BLK; i++) {
            if (!de[i].inode) continue;
            sfs_inode_t child;
            inode_read(de[i].inode, &child);
            shell_write(child.type == 2 ? "[D] " : "    ");
            shell_write(de[i].name);
            shell_writeln("");
            count++;
        }
    }
    if (!count) shell_writeln("(vazio)");
    return count;
}

void spfs_info(void) {
    if (!sfs_ok) {
        shell_writeln("SpaceFS: nao montado");
        shell_writeln("  use 'spfs fmt' para formatar o disco2");
        return;
    }
    shell_writeln("=== SpaceFS ===");
    shell_write("Magic:        0x");
    /* print hex */
    uint32_t m = sb.magic;
    char hx[9]; hx[8] = '\0';
    for (int i = 7; i >= 0; i--) { hx[i] = "0123456789ABCDEF"[m & 0xF]; m >>= 4; }
    shell_writeln(hx);
    shell_write("Versao:       "); shell_write_num(sb.version);       shell_writeln("");
    shell_write("Blocos total: "); shell_write_num(sb.total_blocks);  shell_writeln("");
    shell_write("Blocos livres:"); shell_write_num(sb.free_blocks);   shell_writeln("");
    shell_write("Inodes total: "); shell_write_num(sb.inode_count);   shell_writeln("");
    shell_write("Inodes livres:"); shell_write_num(sb.free_inodes);   shell_writeln("");
    shell_write("Data start:   setor "); shell_write_num(sb.data_start); shell_writeln("");
}
