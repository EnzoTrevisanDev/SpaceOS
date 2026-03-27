#ifndef GDT_H
#define GDT_H

#include <stdint.h>

typedef struct {
    uint16_t limite_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  acesso;
    uint8_t  granularidade;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;


typedef struct {
    uint16_t limite;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;


void gdt_init(void);
#endif