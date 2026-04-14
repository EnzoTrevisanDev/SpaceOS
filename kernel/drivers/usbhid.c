#include "usbhid.h"
#include "usb.h"
#include "../disk/kstring.h"

/* ---- HID keycodes → ASCII (boot protocol, sem shift) ------------- */
static const char hid_to_ascii[128] = {
    0,0,0,0,
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '1','2','3','4','5','6','7','8','9','0',
    '\n',27,'\b','\t',' ','-','=','[',']','\\',0,';','\'','`',',','.','/',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char hid_to_ascii_shift[128] = {
    0,0,0,0,
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '!','@','#','$','%','^','&','*','(',')',
    '\n',27,'\b','\t',' ','_','+','{','}','|',0,':','"','~','<','>','?',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static uint8_t hid_addr   = 0;
static uint8_t hid_ep     = 0x81;   /* EP1 IN (tipico para teclado) */
static int     hid_ok     = 0;
static uint8_t prev_keys[6];        /* teclas pressionadas no ultimo poll */

int usbhid_init(uint8_t dev_addr) {
    hid_addr = dev_addr;
    k_memset(prev_keys, 0, 6);

    /* SET_PROTOCOL(0 = boot protocol) */
    usb_setup_t setup;
    k_memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x21;  /* class, interface, host->device */
    setup.bRequest = USB_REQ_SET_PROTOCOL;
    setup.wValue   = 0;          /* boot protocol */
    setup.wIndex   = 0;          /* interface 0 */
    usb_control(dev_addr, &setup, 0, 0, 0);

    hid_ok = 1;
    return 0;
}

int usbhid_poll(char *key_out) {
    if (!hid_ok) return 0;

    uint8_t report[8];
    if (usb_hid_poll(hid_addr, hid_ep, report, 8) != 0) return 0;

    uint8_t mod = report[0];
    int shifted = (mod & 0x22) != 0;  /* SHIFT L ou SHIFT R */

    /* Verifica novas teclas pressionadas */
    for (int i = 2; i < 8; i++) {
        uint8_t kc = report[i];
        if (!kc) continue;

        /* Verifica se e tecla nova (nao estava pressionada antes) */
        int is_new = 1;
        for (int j = 0; j < 6; j++) {
            if (prev_keys[j] == kc) { is_new = 0; break; }
        }

        if (is_new && kc < 128) {
            char c = shifted ? hid_to_ascii_shift[kc] : hid_to_ascii[kc];
            if (c) {
                k_memcpy(prev_keys, report + 2, 6);
                if (key_out) *key_out = c;
                return 1;
            }
        }
    }

    k_memcpy(prev_keys, report + 2, 6);
    return 0;
}

int usbhid_ready(void) { return hid_ok; }
