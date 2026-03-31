#include "timer.h"
#include "pic.h"
#include "../proc/sched.h"

#define PIT_CMD 0x43
#define PIT_CH0 0x40

static uint32_t ticks = 0;

static void out_byte(uint16_t porta, uint8_t valor){
    __asm__ volatile ("outb %0, %1" : : "a"(valor), "Nd"(porta));
}

uint32_t timer_ticks(void) { return ticks; }

void timer_handler(void){
    ticks++;
    sched_tick(); //avisa o scheduler que um tick passou
    pic_ack(0); // obrigatorio - sem isso o PIC para de mandar IRQ0
}


void timer_init(void){
    // calcula o divisor para atingir TIMER_HZ
    // PIT roda a 1193182 Hz - div para chegar em 100 Hz
    uint32_t divisor = PIT_FREQ / TIMER_HZ;

    //config PIT
    //0x36 = canal 0, acesso low/high, modo 3 (square wave), binario
    out_byte(PIT_CMD, 0x36);

    //envia o divisor em dois bytes - low byte, primeiro, high byte depois
    out_byte(PIT_CH0, (uint8_t)(divisor & 0xFF));
    out_byte(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
}