#ifndef HDA_H
#define HDA_H

#include <stdint.h>

/*
 * Intel High Definition Audio (HDA) — driver minimo
 *
 * Detectado via PCI: class=0x04, subclass=0x03
 * QEMU: vendor=0x8086, device=0x2668 (ICH6 HDA)
 *
 * Implementado:
 *   - Reset do controller
 *   - Deteccao de codecs via STATESTS
 *   - Setup do CORB/RIRB (buffers de comando/resposta)
 *   - Envio de verbos via CORB
 *
 * Para usar com QEMU:
 *   qemu-system-x86_64 ... -device intel-hda -device hda-duplex
 */

/* Offsets dos registradores HDA (relativos a BAR0 MMIO) */
#define HDA_GCAP      0x00   /* Global Capabilities */
#define HDA_VMIN      0x02   /* Minor Version */
#define HDA_VMAJ      0x03   /* Major Version */
#define HDA_GCTL      0x08   /* Global Control (bit 0 = reset) */
#define HDA_STATESTS  0x0E   /* State Change Status (codec detection) */
#define HDA_CORBLBASE 0x40   /* CORB Lower Base Address */
#define HDA_CORBUBASE 0x44   /* CORB Upper Base Address */
#define HDA_CORBWP    0x48   /* CORB Write Pointer */
#define HDA_CORBRP    0x4A   /* CORB Read Pointer */
#define HDA_CORBCTL   0x4C   /* CORB Control */
#define HDA_CORBSIZE  0x4E   /* CORB Size */
#define HDA_RIRBLBASE 0x50   /* RIRB Lower Base Address */
#define HDA_RIRBUBASE 0x54   /* RIRB Upper Base Address */
#define HDA_RIRBWP    0x58   /* RIRB Write Pointer */
#define HDA_RINTCNT   0x5A   /* Response Interrupt Count */
#define HDA_RIRBCTL   0x5C   /* RIRB Control */
#define HDA_RIRBSIZE  0x5E   /* RIRB Size */

/*
 * hda_init — detecta e inicializa o controller HDA.
 * Retorna 0 se ok, -1 se nao encontrado ou falha.
 */
int hda_init(void);

/*
 * hda_send_verb — envia comando de codec via CORB.
 * codec: 0-15, node: 0-127, verb: 12-bit, payload: 8-bit
 * Retorna resposta do codec ou 0xFFFFFFFF em timeout.
 */
uint32_t hda_send_verb(uint8_t codec, uint8_t node,
                        uint16_t verb, uint8_t payload);

/* Retorna 1 se HDA foi inicializado */
int hda_ready(void);

#endif
