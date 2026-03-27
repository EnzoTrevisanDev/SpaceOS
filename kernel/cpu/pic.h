#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Portas de I/O do PIC
   O x86 tem dois PICs em cascata: mestre e escravo
   Cada um tem uma porta de comando e uma de dados */
#define PIC1_CMD  0x20
#define PIC1_DAD  0x21
#define PIC2_CMD  0xA0
#define PIC2_DAD  0xA1

void pic_init(void);
void pic_ack(uint8_t irq);  /* avisa o PIC que a interrupcao foi tratada */

#endif