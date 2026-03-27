#ifndef TECLADO_H
#define TECLADO_H

#include <stdint.h>

void teclado_init(void);
char teclado_ultimo_char(void);
/* Wrapper Assembly — precisa ser visivel para o teclado_init */

void teclado_handler_asm(void);
#endif