/* kernel/cpu/pic.c
   O PIC precisa ser remapeado porque por padrao os IRQs 0-15
   conflitam com as excecoes do x86 (0-31).
   Remapeamos: IRQ0-7 -> interrupcoes 32-39, IRQ8-15 -> 40-47 */

#include "pic.h"

/* out_byte: escreve um byte em uma porta de I/O.
   No x86 perifericos sao acessados por portas, nao por endereco de memoria */
static void out_byte(uint16_t porta, uint8_t valor) {
    __asm__ volatile ("outb %0, %1" : : "a"(valor), "Nd"(porta));
}

/* in_byte: le um byte de uma porta de I/O */
static uint8_t in_byte(uint16_t porta) {
    uint8_t valor;
    __asm__ volatile ("inb %1, %0" : "=a"(valor) : "Nd"(porta));
    return valor;
}

/* Pequeno delay — necessario entre comandos ao PIC */
static void io_wait(void) {
    out_byte(0x80, 0);
}

void pic_init(void) {
    /* ICW1: inicia sequencia de configuracao */
    out_byte(PIC1_CMD, 0x11); io_wait();
    out_byte(PIC2_CMD, 0x11); io_wait();

    /* ICW2: remapeia os vetores
       PIC1 agora comeca na interrupcao 32
       PIC2 agora comeca na interrupcao 40 */
    out_byte(PIC1_DAD, 0x20); io_wait();
    out_byte(PIC2_DAD, 0x28); io_wait();

    /* ICW3: configura cascata entre PIC1 e PIC2 */
    out_byte(PIC1_DAD, 0x04); io_wait();
    out_byte(PIC2_DAD, 0x02); io_wait();

    /* ICW4: modo 8086 */
    out_byte(PIC1_DAD, 0x01); io_wait();
    out_byte(PIC2_DAD, 0x01); io_wait();

    /* Mascara todos os IRQs por enquanto — habilita so o que precisar */
    out_byte(PIC1_DAD, 0xFC); /* IRQ0 (timer) + IRQ1 (teclado) */
    out_byte(PIC2_DAD, 0xFF); /* tudo desabilitado no PIC2 por agora */
}

/* Depois de tratar uma interrupcao, avisa o PIC que pode mandar mais */
void pic_ack(uint8_t irq) {
    if (irq >= 8)
        out_byte(PIC2_CMD, 0x20);
    out_byte(PIC1_CMD, 0x20);
}