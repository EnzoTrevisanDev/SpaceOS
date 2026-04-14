#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Portas do controlador ATA primario */
#define ATA_DATA        0x1F0   /* leitura/escrita de dados */
#define ATA_ERROR       0x1F1   /* registro de erro */
#define ATA_SETOR_COUNT 0x1F2   /* quantos setores ler */
#define ATA_LBA_LOW     0x1F3   /* bits 0-7 do LBA */
#define ATA_LBA_MID     0x1F4   /* bits 8-15 do LBA */
#define ATA_LBA_HIGH    0x1F5   /* bits 16-23 do LBA */
#define ATA_DRIVE       0x1F6   /* selecao de drive + bits 24-27 do LBA */
#define ATA_STATUS      0x1F7   /* status do controlador */
#define ATA_CMD         0x1F7   /* comando para o controlador */

/* Bits do registro de status */
#define ATA_SR_BSY  0x80   /* busy — disco ocupado */
#define ATA_SR_DRQ  0x08   /* data request — dados prontos para ler */
#define ATA_SR_ERR  0x01   /* erro */

/* Comandos ATA */
#define ATA_CMD_READ  0x20   /* ler setor(es) */
#define ATA_CMD_WRITE 0x30   /* gravar setor(es) */
#define ATA_CMD_ID    0xEC   /* identificar disco */

/* Tamanho de um setor em bytes */
#define SETOR_SIZE 512

/* Inicializa e detecta o disco 0 (master) */
int  ata_init(void);

/* Le count setores a partir do LBA para o buffer (disco 0) */
int  ata_ler(uint32_t lba, uint8_t count, uint8_t *buffer);

/* Grava count setores a partir do LBA do buffer (disco 0) */
int  ata_gravar(uint32_t lba, uint8_t count, uint8_t *buffer);

/* Disco 1 (slave no canal primario) */
int  ata_init1(void);
int  ata_presente1(void);
int  ata_ler1(uint32_t lba, uint8_t count, uint8_t *buffer);
int  ata_gravar1(uint32_t lba, uint8_t count, uint8_t *buffer);

#endif