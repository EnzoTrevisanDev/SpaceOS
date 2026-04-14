#ifndef USBHID_H
#define USBHID_H

/*
 * usbhid — USB HID teclado via boot protocol
 *
 * Boot protocol: relatorio fixo de 8 bytes
 *   [0] modifier  [1] reserved  [2-7] keycodes (HID Usage IDs)
 *
 * SET_PROTOCOL(BOOT) evita necessidade de parsear report descriptor.
 * Polling por GET_REPORT via interrupt transfer.
 */

#include <stdint.h>

int  usbhid_init(uint8_t dev_addr);  /* configura dispositivo HID */
int  usbhid_poll(char *key_out);     /* retorna caracter ou 0 */
int  usbhid_ready(void);

#endif
