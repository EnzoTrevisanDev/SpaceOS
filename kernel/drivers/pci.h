#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* Portas de acesso ao espaco de configuracao PCI */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define PCI_MAX_DEVICES     64

/* Offsets no espaco de configuracao */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_CLASS_CODE      0x08   /* class (bits 31-24), subclass (23-16), progif (15-8), rev (7-0) */
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

/* Classes PCI relevantes */
#define PCI_CLASS_NETWORK   0x02   /* subclass 0x00 = Ethernet */
#define PCI_CLASS_MULTIMEDIA 0x04  /* subclass 0x03 = HDA audio */
#define PCI_CLASS_STORAGE   0x01   /* subclass 0x01 = IDE */
#define PCI_CLASS_BRIDGE    0x06
#define PCI_CLASS_SERIAL    0x0C   /* subclass 0x03 = USB */

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    uint8_t  header_type;
    uint32_t bar[6];
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
} pci_device_t;

/*
 * pci_init — varre todos os barramentos e detecta dispositivos.
 * Retorna numero de dispositivos encontrados.
 */
int pci_init(void);

/* Leitura/escrita no espaco de configuracao PCI (32 bits) */
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void     pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);

/*
 * pci_find — retorna ponteiro para o primeiro dispositivo com vendor:device.
 * Retorna NULL se nao encontrado.
 */
pci_device_t *pci_find(uint16_t vendor_id, uint16_t device_id);

/*
 * pci_find_class — retorna primeiro dispositivo com class:subclass.
 * Retorna NULL se nao encontrado.
 */
pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass);

/* pci_dump — imprime todos os dispositivos detectados na shell */
void pci_dump(void);

/* Acesso direto a lista interna (para drivers) */
pci_device_t *pci_get_list(void);
int           pci_get_count(void);

#endif
