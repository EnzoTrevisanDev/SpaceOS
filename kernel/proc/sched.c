#include "sched.h"
#include "processo.h"
#include "../shell/shell.h"

static uint32_t ticks = 0;

uint32_t sched_ticks(void) { return ticks; }

void sched_tick(void){
    ticks++;
    //troca de contexto vem no futuro!
}
void sched_init(void){
    proc_init();
}