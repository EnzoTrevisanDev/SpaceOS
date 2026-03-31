#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_HZ 100 // 100 ticks por segundo
#define PIT_FREQ 1193182 // frequencia base do PIT em Hz — fixo no hardware

void timer_init(void);
void timer_handler(void);
void timer_handler_asm(void);
uint32_t timer_ticks(void); //quantos ticks desde o boot

#endif