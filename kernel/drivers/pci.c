#include "pci.h"
#include "../cpu/io.h"
#include "../shell/shell.h"

static pci_device_t pci_devices[PCI_MAX_DEVICES];
static int pci_count = 0;

/* ---- Acesso ao espaco de configuracao ----------------------------- */

uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = (1U << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, addr);
    outl(PCI_CONFIG_DATA, val);
}

/* ---- Varredura do barramento -------------------------------------- */

static void pci_scan_function(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t id = pci_read(bus, dev, func, PCI_VENDOR_ID);
    uint16_t vendor = (uint16_t)(id & 0xFFFF);
    if (vendor == 0xFFFF) return;  /* slot vazio */

    if (pci_count >= PCI_MAX_DEVICES) return;

    pci_device_t *d = &pci_devices[pci_count++];
    d->bus      = bus;
    d->device   = dev;
    d->function = func;
    d->vendor_id  = vendor;
    d->device_id  = (uint16_t)(id >> 16);

    uint32_t class_info = pci_read(bus, dev, func, PCI_CLASS_CODE);
    d->revision   = (uint8_t)(class_info & 0xFF);
    d->prog_if    = (uint8_t)((class_info >>  8) & 0xFF);
    d->subclass   = (uint8_t)((class_info >> 16) & 0xFF);
    d->class_code = (uint8_t)((class_info >> 24) & 0xFF);

    uint32_t hdr = pci_read(bus, dev, func, PCI_HEADER_TYPE);
    d->header_type = (uint8_t)((hdr >> 16) & 0xFF);

    /* Le BARs (apenas header type 0) */
    if ((d->header_type & 0x7F) == 0) {
        for (int i = 0; i < 6; i++)
            d->bar[i] = pci_read(bus, dev, func, (uint8_t)(PCI_BAR0 + i * 4));

        uint32_t irq = pci_read(bus, dev, func, PCI_INTERRUPT_LINE);
        d->interrupt_line = (uint8_t)(irq & 0xFF);
        d->interrupt_pin  = (uint8_t)((irq >> 8) & 0xFF);
    }
}

static void pci_scan_device(uint8_t bus, uint8_t dev) {
    uint32_t id = pci_read(bus, dev, 0, PCI_VENDOR_ID);
    if ((id & 0xFFFF) == 0xFFFF) return;

    pci_scan_function(bus, dev, 0);

    /* Dispositivo multifuncao? */
    uint32_t hdr = pci_read(bus, dev, 0, PCI_HEADER_TYPE);
    uint8_t header_type = (uint8_t)((hdr >> 16) & 0xFF);
    if (header_type & 0x80) {
        for (uint8_t func = 1; func < 8; func++) {
            uint32_t fid = pci_read(bus, dev, func, PCI_VENDOR_ID);
            if ((fid & 0xFFFF) != 0xFFFF)
                pci_scan_function(bus, dev, func);
        }
    }
}

int pci_init(void) {
    pci_count = 0;
    /* Varre bus 0 a 7 (suficiente para QEMU e maioria dos PCs) */
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            pci_scan_device(bus, dev);
        }
    }
    return pci_count;
}

/* ---- Busca de dispositivos --------------------------------------- */

pci_device_t *pci_find(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id)
            return &pci_devices[i];
    }
    return 0;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_count; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].device_id  == subclass)
            return &pci_devices[i];
    }
    return 0;
}

pci_device_t *pci_get_list(void)  { return pci_devices; }
int           pci_get_count(void) { return pci_count; }

/* ---- Dump para debug --------------------------------------------- */

static void write_hex8(uint8_t v) {
    shell_write_char("0123456789ABCDEF"[(v >> 4) & 0xF]);
    shell_write_char("0123456789ABCDEF"[v & 0xF]);
}
static void write_hex16(uint16_t v) {
    write_hex8((uint8_t)(v >> 8));
    write_hex8((uint8_t)(v & 0xFF));
}

void pci_dump(void) {
    shell_writeln("=== Dispositivos PCI ===");
    shell_writeln("");
    shell_writeln("Bus:Dev:Fn  Vendor:Device  Class:Sub  IRQ");

    for (int i = 0; i < pci_count; i++) {
        pci_device_t *d = &pci_devices[i];

        shell_write("  ");
        write_hex8(d->bus); shell_write(":");
        write_hex8(d->device); shell_write(":");
        write_hex8(d->function); shell_write("  ");
        write_hex16(d->vendor_id); shell_write(":");
        write_hex16(d->device_id); shell_write("  ");
        write_hex8(d->class_code); shell_write(":");
        write_hex8(d->subclass); shell_write("  IRQ");
        shell_write_num(d->interrupt_line);
        shell_writeln("");
    }

    shell_writeln("");
    shell_write("Total: ");
    shell_write_num((uint32_t)pci_count);
    shell_writeln(" dispositivo(s)");
}
