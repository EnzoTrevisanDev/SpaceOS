#ifndef SPACEFS_H
#define SPACEFS_H

/*
 * SpaceFS — filesystem proprio do SpaceOS
 *
 * Layout no disco 1 (slave IDE):
 *   Setor 0:     Superbloco
 *   Setor 1:     Bitmap de inodes (256 bits)
 *   Setor 2:     Bitmap de blocos
 *   Setores 3-34:  Tabela de inodes (256 × 64B = 32 setores)
 *   Setores 35-98: Journal (64 setores = 32 entradas de 2 setores)
 *   Setor 99+:   Blocos de dados
 *
 * Inode: 64 bytes
 *   10 blocos diretos (5KB) + 1 indireto (64KB) = max 69KB por arquivo
 *
 * Diretorio: arquivo especial com entradas de 32 bytes (inode + nome)
 *
 * Comandos shell: spfs fmt | spfs ls | spfs cat | spfs write | spfs info
 */

#include <stdint.h>

#define SFS_MAGIC     0x53465321u  /* "SFS!" */
#define SFS_VERSION   1
#define SFS_INODE_MAX 256
#define SFS_NAME_MAX  27

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t inode_count;
    uint32_t free_inodes;
    uint32_t data_start;   /* primeiro setor de dados */
    uint32_t root_inode;   /* sempre 0 */
    uint8_t  _pad[480];    /* preenche 512 bytes */
} __attribute__((packed)) sfs_super_t;

typedef struct {
    uint8_t  type;         /* 0=livre, 1=arquivo, 2=diretorio */
    uint8_t  flags;
    uint16_t nlinks;
    uint32_t size;         /* bytes */
    uint32_t mtime;
    uint32_t blocks[10];   /* blocos diretos */
    uint32_t indirect;     /* bloco de ponteiros indireto */
    uint8_t  _pad[8];
} __attribute__((packed)) sfs_inode_t;  /* 64 bytes */

typedef struct {
    uint32_t inode;        /* 0 = entrada livre */
    char     name[SFS_NAME_MAX + 1]; /* nome null-terminated (28 bytes) */
} __attribute__((packed)) sfs_dirent_t; /* 32 bytes */

/* ---- API publica -------------------------------------------------- */

int  spfs_format(void);        /* formata disco 1 com SpaceFS */
int  spfs_mount(void);         /* monta SpaceFS do disco 1 */
int  spfs_mounted(void);

/* Operacoes de arquivo */
int  spfs_read(const char *path, uint8_t *buf, uint32_t max);
int  spfs_write(const char *path, const uint8_t *buf, uint32_t size);
int  spfs_mkdir(const char *path);
int  spfs_unlink(const char *path);
int  spfs_ls(const char *path); /* imprime diretorio na shell */
void spfs_info(void);           /* imprime info do superbloco */

#endif
