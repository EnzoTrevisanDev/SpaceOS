#ifndef AHCI_H
#define AHCI_H

/*
 * ahci — Advanced Host Controller Interface (SATA via PCI)
 *
 * PCI: class=0x01, subclass=0x06
 * QEMU: -device ahci,id=ahci0 -device ide-hd,drive=d1,bus=ahci0.0
 *
 * Fornece ahci_ler() / ahci_gravar() como alternativa de alta
 * performance ao ATA PIO. Usa DMA interno (HBA gerencia transferencia).
 */

#include <stdint.h>

int  ahci_init(void);
int  ahci_disponivel(void);
int  ahci_ler(uint32_t lba, uint8_t count, uint8_t *buffer);
int  ahci_gravar(uint32_t lba, uint8_t count, uint8_t *buffer);
void ahci_info(void);   /* imprime portas detectadas */

#endif
