#ifndef TECLADO_H
#define TECLADO_H

#include <stdint.h>

/* Codigos especiais para teclas nao-ASCII */
#define TECLA_SETA_CIMA   0x81
#define TECLA_SETA_BAIXO  0x82
#define TECLA_SETA_ESQ    0x83
#define TECLA_SETA_DIR    0x84


void teclado_init(void);
char teclado_ultimo_char(void);
/* Wrapper Assembly — precisa ser visivel para o teclado_init */

void teclado_handler_asm(void);
#endif