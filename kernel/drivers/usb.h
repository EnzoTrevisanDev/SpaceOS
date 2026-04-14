#ifndef USB_H
#define USB_H

/*
 * usb — EHCI USB 2.0 Host Controller
 *
 * PCI: class=0x0C, subclass=0x03, prog_if=0x20 (EHCI)
 * QEMU: -device usb-ehci,id=ehci -device usb-kbd,bus=ehci.0
 *
 * Suporte: controle de transfers (setup/data/status)
 * Dispositivos: HID boot protocol keyboard (via usbhid.c)
 */

#include <stdint.h>

#define USB_CLASS_HID    0x03
#define USB_SUBCLASS_BOOT 0x01
#define USB_PROTO_KBD    0x01

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) usb_iface_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;  /* bit7=direction (1=IN) */
    uint8_t  bmAttributes;      /* bits1:0 = transfer type (3=interrupt) */
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_ep_desc_t;

/* Setup packet */
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_t;

/* Requests */
#define USB_REQ_GET_DESCRIPTOR  0x06
#define USB_REQ_SET_ADDRESS     0x05
#define USB_REQ_SET_CONFIG      0x09
#define USB_REQ_SET_PROTOCOL    0x0B
#define USB_REQ_GET_REPORT      0x01

#define USB_DESC_DEVICE     0x01
#define USB_DESC_CONFIG     0x02
#define USB_DESC_STRING     0x03
#define USB_DESC_INTERFACE  0x04
#define USB_DESC_ENDPOINT   0x05
#define USB_DESC_HID        0x21

int  usb_init(void);
int  usb_ready(void);

/* Executa control transfer no dispositivo */
int  usb_control(uint8_t dev_addr, usb_setup_t *setup,
                 uint8_t *data, uint16_t len, int is_in);

/* Obtem relatorio HID via interrupt transfer */
int  usb_hid_poll(uint8_t dev_addr, uint8_t ep, uint8_t *buf, uint8_t len);

/* Numero de dispositivos USB detectados */
int  usb_device_count(void);

#endif
