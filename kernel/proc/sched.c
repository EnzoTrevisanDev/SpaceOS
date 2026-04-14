#include "sched.h"
#include "processo.h"
#include "../shell/shell.h"
#include "../cpu/spinlock.h"

static uint32_t ticks = 0;
static uint32_t creditos_atuais = 0;

/* Lock para proteger a fila de processos em sistemas SMP */
spinlock_t sched_lock = SPINLOCK_INIT;  /* creditos restantes do processo atual */

uint32_t sched_ticks(void) { return ticks; }

/* Recalcula creditos de todos os processos
   Alta prioridade = 3 creditos, media = 2, baixa = 1 */
static void recalcular_creditos(void) {
    processo_t *p = proc_fila();
    while (p) {
        if (p->estado == PROC_READY || p->estado == PROC_RUNNING) {
            switch (p->prioridade) {
                case 2:  p->creditos = 3; break;
                case 1:  p->creditos = 2; break;
                default: p->creditos = 1; break;
            }
        }
        p = p->proximo;
    }
}

/* Escolhe o proximo processo com creditos disponíveis */
static processo_t *escolher_proximo(processo_t *atual) {
    /* Tenta achar alguem com creditos na fila apos o atual */
    processo_t *candidato = atual->proximo;
    if (!candidato) candidato = proc_fila();

    int tentativas = 0;
    while (tentativas < MAX_PROCESSOS) {
        if (candidato && candidato != atual &&
            candidato->estado == PROC_READY &&
            candidato->creditos > 0) {
            return candidato;
        }
        if (candidato) candidato = candidato->proximo;
        if (!candidato) candidato = proc_fila();
        tentativas++;
    }

    /* Nenhum tem creditos — recalcula e tenta de novo */
    recalcular_creditos();
    candidato = proc_fila();
    while (candidato) {
        if (candidato->estado == PROC_READY && candidato->creditos > 0)
            return candidato;
        candidato = candidato->proximo;
    }

    return 0;  /* so o atual disponivel */
}

uint32_t sched_tick(uint32_t esp_atual) {
    ticks++;

    spin_lock(&sched_lock);
    processo_t *atual = proc_atual();
    if (!atual) { spin_unlock(&sched_lock); return esp_atual; }

    /* Salva ESP do processo atual */
    atual->ctx.esp = esp_atual;

    /* Consome um credito do processo atual */
    if (creditos_atuais > 0) creditos_atuais--;

    /* Se ainda tem creditos, continua rodando */
    if (creditos_atuais > 0) { spin_unlock(&sched_lock); return esp_atual; }

    /* Sem creditos — escolhe o proximo */
    processo_t *proximo = escolher_proximo(atual);
    if (!proximo) { spin_unlock(&sched_lock); return esp_atual; }

    /* Troca */
    if (atual->estado == PROC_RUNNING)
        atual->estado = PROC_READY;

    proximo->estado   = PROC_RUNNING;
    creditos_atuais   = proximo->creditos;
    proc_set_atual(proximo);

    spin_unlock(&sched_lock);
    return proximo->ctx.esp;
}


void sched_init(void) {
    recalcular_creditos();
    processo_t *p = proc_atual();
    if (p) creditos_atuais = (p->prioridade == 2) ? 3 :
                             (p->prioridade == 1) ? 2 : 1;
}

