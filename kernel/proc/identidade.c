#include "identidade.h"
#include "../shell/shell.h"

/* Hash simples mas suficientemente unico para identificacao de processos
   Baseado em FNV-1a adaptado para tres valores de entrada */
static uint32_t hash_fnv(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = 2166136261u;  /* FNV offset basis */

    /* Processa cada byte dos tres valores */
    for (int i = 0; i < 4; i++) {
        h ^= (a >> (i * 8)) & 0xFF;
        h *= 16777619u;         /* FNV prime */
    }
    for (int i = 0; i < 4; i++) {
        h ^= (b >> (i * 8)) & 0xFF;
        h *= 16777619u;
    }
    for (int i = 0; i < 4; i++) {
        h ^= (c >> (i * 8)) & 0xFF;
        h *= 16777619u;
    }

    return h;
}

identidade_t identidade_gerar(uint32_t pid, uint32_t tick, uint32_t stack_addr) {
    identidade_t id;
    id.pid             = pid;
    id.tick_nascimento = tick;
    id.hash            = hash_fnv(pid, tick, stack_addr);
    return id;
}

int identidade_verificar(identidade_t *id, uint32_t pid) {
    if (!id) return 0;
    return id->pid == pid && id->hash != 0;
}