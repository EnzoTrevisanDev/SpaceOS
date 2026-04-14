#include "wifi.h"
#include "../drivers/pci.h"
#include "../shell/shell.h"

/* Tabela de chipsets conhecidos */
static const wifi_chip_t wifi_chips[] = {
    { 0x10EC, 0x8180, "RTL8180",  "Realtek 802.11b" },
    { 0x10EC, 0x8187, "RTL8187",  "Realtek 802.11bg" },
    { 0x10EC, 0x8185, "RTL8185",  "Realtek 802.11bg" },
    { 0x10EC, 0xB723, "RTL8723", "Realtek 802.11n"  },
    { 0x8086, 0x1043, "IPW2100", "Intel PRO/Wireless 2100" },
    { 0x8086, 0x1044, "IPW2100", "Intel PRO/Wireless 2100" },
    { 0x8086, 0x4220, "IPW2200", "Intel PRO/Wireless 2200BG" },
    { 0x8086, 0x4223, "IPW2915", "Intel PRO/Wireless 2915ABG" },
    { 0x8086, 0x4229, "IPW3945", "Intel PRO/Wireless 3945ABG" },
    { 0x168C, 0x001C, "AR5416",  "Atheros 802.11abgn" },
    { 0x168C, 0x002A, "AR9280",  "Atheros 802.11abgn" },
    { 0x168C, 0x0030, "AR9300",  "Atheros 802.11abgn" },
    { 0x14E4, 0x4324, "BCM4309", "Broadcom 802.11abg" },
    { 0x14E4, 0x4328, "BCM4321", "Broadcom 802.11abgn" },
    { 0, 0, 0, 0 }
};

static const wifi_chip_t *found_chip = 0;
static int wifi_ok = 0;

int wifi_init(void) {
    /* Busca PCI class 0x02 (network) subclass 0x80 (wireless) */
    pci_device_t *dev = pci_find_class(0x02, 0x80);

    if (!dev) {
        /* Busca por vendor/device ID da tabela */
        for (int i = 0; wifi_chips[i].vendor_id; i++) {
            dev = pci_find(wifi_chips[i].vendor_id, wifi_chips[i].device_id);
            if (dev) {
                found_chip = &wifi_chips[i];
                break;
            }
        }
    } else {
        /* Encontrou por classe — busca na tabela */
        for (int i = 0; wifi_chips[i].vendor_id; i++) {
            if (wifi_chips[i].vendor_id == dev->vendor_id &&
                wifi_chips[i].device_id == dev->device_id) {
                found_chip = &wifi_chips[i];
                break;
            }
        }
    }

    if (!dev) return -1;  /* nenhum adaptador WiFi */

    wifi_ok = 1;
    return 0;
    /*
     * TODO v2.x: inicializar chipset, associar com AP, WPA2/3 handshake.
     * Cada chipset tem firmware e protocolo proprietario.
     * Referencia: Linux kernel drivers/net/wireless/
     */
}

int wifi_disponivel(void) { return wifi_ok; }

void wifi_info(void) {
    if (!wifi_ok) {
        shell_writeln("WiFi: nenhum adaptador detectado");
        shell_writeln("  (driver 802.11 completo previsto para v2.x)");
        return;
    }
    shell_write("WiFi: ");
    if (found_chip) {
        shell_write(found_chip->name);
        shell_write(" — ");
        shell_write(found_chip->chipset);
        shell_writeln(" [driver pendente v2.x]");
    } else {
        shell_writeln("chipset desconhecido detectado [driver pendente]");
    }
}
