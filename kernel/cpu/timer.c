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

/* Chamado pelo Assembly com o ESP atual do processo pausado
   Retorna o ESP do proximo processo a rodar */
uint32_t timer_handler_c(uint32_t esp_atual) {
    ticks++;
    pic_ack(0);
    return sched_tick(esp_atual);   /* scheduler decide quem roda */
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