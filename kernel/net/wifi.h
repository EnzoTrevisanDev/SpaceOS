#ifndef WIFI_H
#define WIFI_H

/*
 * wifi — deteccao de chipset WiFi 802.11 via PCI.
 *
 * Chipsets suportados (deteccao):
 *   Realtek RTL8180/RTL8187 (PCI)
 *   Intel PRO/Wireless (PCI)
 *   Atheros AR9xxx (PCI)
 *
 * Status: deteccao implementada. Driver full 802.11 + WPA2/3
 * requer ~10.000 linhas de codigo (v2.x).
 *
 * QEMU: WiFi nao emulado em modo padrao.
 * Para testar: qemu com mac80211_hwsim (Linux guest) ou USB WiFi dongle.
 */

#include <stdint.h>

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    const char *name;
    const char *chipset;
} wifi_chip_t;

int  wifi_init(void);
int  wifi_disponivel(void);
void wifi_info(void);

#endif
