#ifndef DMA_H
#define DMA_H

/*
 * dma — Bus Master IDE DMA para transferencias ATA sem CPU.
 *
 * O controlador IDE (PCI class=0x01, subclass=0x01, prog_if bit 7)
 * suporta DMA via registradores Bus Master em BAR4.
 *
 * PRDT: Physical Region Descriptor Table — lista de buffers DMA.
 * Cada entrada: [phys_addr 32b][byte_count 16b | EOT bit 16b]
 *
 * Uso:
 *   dma_init()                        — detecta via PCI
 *   dma_ler(lba, count, buf)          — DMA read (disco 0)
 *   dma_gravar(lba, count, buf)       — DMA write (disco 0)
 *   dma_disponivel()                  — 1 se DMA ativo
 */

#include <stdint.h>

int  dma_init(void);
int  dma_disponivel(void);
int  dma_ler(uint32_t lba, uint8_t count, uint8_t *buffer);
int  dma_gravar(uint32_t lba, uint8_t count, uint8_t *buffer);

#endif
