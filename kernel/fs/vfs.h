#ifndef VFS_H
#define VFS_H

#include <stdint.h>

/* Entrada de diretorio vista pelo shell e pelos comandos */
typedef struct {
    char     name[13];   /* "ARQUIVO.TXT\0" */
    uint8_t  is_dir;
    uint32_t cluster;
    uint32_t size;
} vfs_entry_t;

/*
 * vfs_init — detecta filesystem no disco ATA e monta na raiz "/".
 * Retorna 0 se ok, -1 se nao houver disco ou filesystem valido.
 */
void vfs_init(void);

/*
 * vfs_readdir — lista entradas de um diretorio.
 * path: caminho absoluto ou relativo ao cwd. "" = cwd atual.
 * Retorna numero de entradas, ou -1 em erro.
 */
int vfs_readdir(const char *path, vfs_entry_t *entries, int max);

/*
 * vfs_open — resolve um caminho para uma entrada (arquivo ou dir).
 * Retorna 0 se encontrado (out preenchido), -1 se nao existe.
 */
int vfs_open(const char *path, vfs_entry_t *out);

/*
 * vfs_read — le conteudo de um arquivo para buf.
 * max_size: tamanho maximo do buf.
 * Retorna bytes lidos, ou -1 em erro.
 */
int vfs_read(const char *path, uint8_t *buf, uint32_t max_size);

/*
 * vfs_write — cria ou sobrescreve um arquivo.
 * Retorna bytes escritos, ou -1 em erro.
 */
int vfs_write(const char *path, const uint8_t *buf, uint32_t size);

/* vfs_mkdir — cria diretorio. Retorna 0 se ok, -1 em erro. */
int vfs_mkdir(const char *path);

/* vfs_unlink — apaga arquivo. Retorna 0 se ok, -1 em erro. */
int vfs_unlink(const char *path);

/* vfs_rename — renomeia arquivo/dir. Retorna 0 se ok, -1 em erro. */
int vfs_rename(const char *old_path, const char *new_path);

/* vfs_getcwd — retorna string do diretorio atual (ex: "/docs"). */
const char *vfs_getcwd(void);

/* vfs_chdir — muda diretorio atual. Retorna 0 se ok, -1 em erro. */
int vfs_chdir(const char *path);

/* vfs_mounted — retorna 1 se filesystem esta montado, 0 se nao. */
int vfs_mounted(void);

/* vfs_append — adiciona dados ao final de um arquivo (cria se nao existe).
   Limite interno de 16KB por arquivo de log. Retorna 0 ok, -1 erro. */
int vfs_append(const char *path, const uint8_t *data, uint32_t size);

#endif
