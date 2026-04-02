#include "sched.h"
#include "processo.h"
#include "../shell/shell.h"

static uint32_t ticks = 0;

uint32_t sched_ticks(void) { return ticks; }

uint32_t sched_tick(uint32_t esp_atual) {
    ticks++;

    processo_t *proc = proc_atual();
    if (!proc) return esp_atual;

    /* Salva ESP do processo atual */
    proc->ctx.esp = esp_atual;

    /* Conta quantos processos READY existem */
    processo_t *candidato = proc->proximo;
    if (!candidato) candidato = proc_fila();

    /* Pula mortos e o proprio processo atual */
    int tentativas = 0;
    while (candidato) {
        if (candidato != proc && candidato->estado == PROC_READY)
            break;
        candidato = candidato->proximo;
        if (!candidato) candidato = proc_fila();
        if (++tentativas > MAX_PROCESSOS) return esp_atual;
    }

    /* Nenhum outro processo pronto — continua no atual */
    if (!candidato || candidato == proc)
        return esp_atual;

    /* Troca */
    if (proc->estado == PROC_RUNNING)
        proc->estado = PROC_READY;

    candidato->estado = PROC_RUNNING;
    proc_set_atual(candidato);

    return candidato->ctx.esp;
}

void sched_init(void) {
    //proc_init();
}
