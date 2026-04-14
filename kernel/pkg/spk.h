#ifndef SPK_H
#define SPK_H

#include <stdint.h>

/* Assinatura magica do formato .spk: "SPK!" em little-endian */
#define SPK_MAGIC      0x53504B21
#define SPK_VERSION    1
#define SPK_MAX_DEPS   8
#define SPK_MAX_FILES  8
#define SPK_MAX_SIZE   (64 * 1024)  /* 64KB limite por pacote */

/*
 * Cabecalho do arquivo .spk — sempre no inicio do arquivo.
 * Layout binario: [spk_header_t][spk_file_record_t * num_files][payload]
 */
typedef struct {
    uint32_t magic;              /* SPK_MAGIC */
    uint32_t version;            /* SPK_VERSION */
    char     name[32];           /* nome do pacote (max 8 chars p/ FAT32) */
    char     pkg_version[16];    /* "1.0.0" */
    char     author[32];
    uint32_t num_deps;           /* numero de dependencias */
    char     deps[8][32];        /* nomes das deps */
    uint32_t num_files;          /* numero de arquivos no pacote */
    uint32_t total_payload_size; /* soma dos tamanhos de todos os arquivos */
    uint32_t checksum;           /* djb2 sobre todos os bytes do payload */
    uint8_t  _pad[32];           /* reservado para crypto futura (HMAC-SHA256) */
} __attribute__((packed)) spk_header_t;

/*
 * Registro de arquivo dentro do .spk.
 * Segue imediatamente apos o spk_header_t.
 */
typedef struct {
    char     install_path[64];   /* ex: "/bin/hello" */
    uint32_t size;               /* tamanho em bytes */
    uint32_t offset;             /* offset desde inicio do payload */
    uint8_t  executable;         /* 1 = executavel */
} __attribute__((packed)) spk_file_record_t;

#endif
