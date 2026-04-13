#include "ata.h"
#include "../shell/shell.h"

/* Lê um byte de uma porta de I/O */
static uint8_t in_byte(uint16_t porta) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(porta));
    return val;
}

/* Lê uma word (2 bytes) de uma porta de I/O */
static uint16_t in_word(uint16_t porta) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(porta));
    return val;
}

/* Escreve um byte em uma porta de I/O */
static void out_byte(uint16_t porta, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(porta));
}

/* Escreve uma word em uma porta de I/O */
static void out_word(uint16_t porta, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(porta));
}

/* Espera o disco sair do estado busy */
static int ata_esperar_pronto(void) {
    uint32_t tentativas = 0;
    while (in_byte(ATA_STATUS) & ATA_SR_BSY) {
        if (++tentativas > 100000) return -1;  /* timeout */
    }
    return 0;
}

/* Espera os dados ficarem prontos para leitura */
static int ata_esperar_drq(void) {
    uint32_t tentativas = 0;
    uint8_t status;
    while (1) {
        status = in_byte(ATA_STATUS);
        if (status & ATA_SR_ERR) return -1;   /* erro */
        if (status & ATA_SR_DRQ) return 0;    /* pronto */
        if (++tentativas > 100000) return -1;  /* timeout */
    }
}

static int disco_presente = 0;

int ata_init(void) {
    /* Envia comando de identificacao para ver se existe disco */
    out_byte(ATA_DRIVE, 0xA0);    /* seleciona master drive */
    out_byte(ATA_SETOR_COUNT, 0);
    out_byte(ATA_LBA_LOW,  0);
    out_byte(ATA_LBA_MID,  0);
    out_byte(ATA_LBA_HIGH, 0);
    out_byte(ATA_CMD, ATA_CMD_ID);

    uint8_t status = in_byte(ATA_STATUS);
    if (status == 0) {
        /* Nenhum disco encontrado */
        disco_presente = 0;
        return -1;
    }

    /* Espera o disco processar */
    if (ata_esperar_pronto() < 0) {
        disco_presente = 0;
        return -1;
    }

    /* Descarta os 256 words de identificacao */
    for (int i = 0; i < 256; i++)
        in_word(ATA_DATA);

    disco_presente = 1;
    return 0;
}

int ata_ler(uint32_t lba, uint8_t count, uint8_t *buffer) {
    if (!disco_presente) return -1;
    if (!buffer || count == 0) return -1;

    /* Espera o disco estar livre */
    if (ata_esperar_pronto() < 0) return -1;

    /* Configura o LBA e o drive
       0xE0 = master drive + modo LBA habilitado */
    out_byte(ATA_DRIVE,       0xE0 | ((lba >> 24) & 0x0F));
    out_byte(ATA_SETOR_COUNT, count);
    out_byte(ATA_LBA_LOW,     (uint8_t)(lba & 0xFF));
    out_byte(ATA_LBA_MID,     (uint8_t)((lba >> 8) & 0xFF));
    out_byte(ATA_LBA_HIGH,    (uint8_t)((lba >> 16) & 0xFF));

    /* Envia comando de leitura */
    out_byte(ATA_CMD, ATA_CMD_READ);

    /* Le cada setor */
    for (int s = 0; s < count; s++) {
        if (ata_esperar_drq() < 0) return -1;

        /* Le 256 words = 512 bytes = 1 setor */
        uint16_t *buf16 = (uint16_t *)(buffer + s * SETOR_SIZE);
        for (int i = 0; i < 256; i++)
            buf16[i] = in_word(ATA_DATA);
    }

    return 0;
}

int ata_gravar(uint32_t lba, uint8_t count, uint8_t *buffer) {
    if (!disco_presente) return -1;
    if (!buffer || count == 0) return -1;

    if (ata_esperar_pronto() < 0) return -1;

    out_byte(ATA_DRIVE,       0xE0 | ((lba >> 24) & 0x0F));
    out_byte(ATA_SETOR_COUNT, count);
    out_byte(ATA_LBA_LOW,     (uint8_t)(lba & 0xFF));
    out_byte(ATA_LBA_MID,     (uint8_t)((lba >> 8) & 0xFF));
    out_byte(ATA_LBA_HIGH,    (uint8_t)((lba >> 16) & 0xFF));

    out_byte(ATA_CMD, ATA_CMD_WRITE);

    for (int s = 0; s < count; s++) {
        if (ata_esperar_drq() < 0) return -1;

        uint16_t *buf16 = (uint16_t *)(buffer + s * SETOR_SIZE);
        for (int i = 0; i < 256; i++)
            out_word(ATA_DATA, buf16[i]);
    }

    return 0;
}