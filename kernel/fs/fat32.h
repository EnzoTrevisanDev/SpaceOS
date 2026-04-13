#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

/* Atributos de entrada de diretorio */
#define FAT32_ATTR_READONLY  0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME    0x08
#define FAT32_ATTR_DIR       0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F   /* Long Filename entry — ignorado */

/* Fim de cadeia FAT32 */
#define FAT32_EOF            0x0FFFFFF8

/* Entrada "amigavel" usada pelas camadas acima */
typedef struct {
    char     name[13];       /* "ARQUIVO.TXT\0" — nome reconstruido */
    uint8_t  is_dir;
    uint32_t first_cluster;
    uint32_t size;           /* 0 para diretorios */
} fat32_entry_t;

/*
 * fat32_init — le o BPB do setor 0 e valida o filesystem.
 * Retorna 0 se FAT32 valido, -1 se erro (sem disco, FS invalido).
 */
int fat32_init(void);

/*
 * fat32_readdir — lista entradas de um diretorio.
 * dir_cluster: cluster inicial do diretorio (2 = raiz).
 * entries: array de saida alocado pelo chamador.
 * max: capacidade maxima de entries[].
 * Retorna numero de entradas encontradas, ou -1 em erro.
 */
int fat32_readdir(uint32_t dir_cluster, fat32_entry_t *entries, int max);

/*
 * fat32_find — busca uma entrada pelo nome dentro de um diretorio.
 * name: nome no formato "ARQUIVO TXT" (11 chars, maiusculo, espaco-padded).
 * Retorna 0 se encontrado (out preenchido), -1 se nao encontrado.
 */
int fat32_find(uint32_t dir_cluster, const char *name11, fat32_entry_t *out);

/*
 * fat32_read — le o conteudo completo de um arquivo seguindo a cadeia FAT.
 * buf deve ter espaco para pelo menos `size` bytes.
 * Retorna bytes lidos, ou -1 em erro.
 */
int fat32_read(uint32_t first_cluster, uint32_t size, uint8_t *buf);

/* --- Escrita --- */

/*
 * fat32_alloc_cluster — aloca um cluster livre, marca como EOF na FAT.
 * Retorna numero do cluster alocado, ou 0 em erro.
 */
uint32_t fat32_alloc_cluster(void);

/*
 * fat32_write_file — escreve dados num arquivo, alocando clusters conforme necessario.
 * Retorna bytes escritos, ou -1 em erro.
 */
int fat32_write_file(uint32_t first_cluster, const uint8_t *buf, uint32_t size);

/*
 * fat32_create — cria uma entrada no diretorio pai.
 * attr: FAT32_ATTR_DIR ou FAT32_ATTR_ARCHIVE.
 * Retorna cluster inicial da nova entrada, ou 0 em erro.
 */
uint32_t fat32_create(uint32_t parent_cluster, const char *name11, uint8_t attr);

/*
 * fat32_delete — apaga uma entrada de diretorio e libera sua cadeia FAT.
 * Retorna 0 se ok, -1 em erro.
 */
int fat32_delete(uint32_t parent_cluster, const char *name11);

/*
 * fat32_rename — renomeia uma entrada dentro do mesmo diretorio.
 * Retorna 0 se ok, -1 em erro.
 */
int fat32_rename(uint32_t dir_cluster, const char *old_name11, const char *new_name11);

/*
 * fat32_normalize — converte "arquivo.txt" para "ARQUIVO TXT" (11 chars).
 * out deve ter pelo menos 12 bytes (11 + \0).
 */
void fat32_normalize(const char *filename, char *out);

/* Retorna o cluster raiz do volume (normalmente 2) */
uint32_t fat32_root_cluster(void);

#endif
