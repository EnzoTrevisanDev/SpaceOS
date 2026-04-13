#ifndef IDENTIDADE_H
#define IDENTIDADE_H

#include <stdint.h>

/* Identidade unica de cada processo — gerada no nascimento */
typedef struct {
    uint32_t hash;          /* hash unico do processo */
    uint32_t pid;           /* PID quando foi gerado */
    uint32_t tick_nascimento; /* tick do timer no nascimento */
} identidade_t;

/* Gera a identidade de um processo */
identidade_t identidade_gerar(uint32_t pid, uint32_t tick, uint32_t stack_addr);

/* Verifica se uma identidade e valida */
int identidade_verificar(identidade_t *id, uint32_t pid);

#endif