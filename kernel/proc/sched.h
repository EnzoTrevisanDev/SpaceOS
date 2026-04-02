#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

void     sched_init(void);
uint32_t sched_tick(uint32_t esp_atual);
uint32_t sched_ticks(void);

#endif