#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

void sched_init(void);

void sched_tick(void);

uint32_t sched_ticks(void);

#endif