#ifndef SHELL_H
#define SHELL_H

#define MAX_INPUT 128 // tamanho maximo do input do usuario
#define HISTORICO_MAX 16 // numero maximo de comandos no historico
#define ARGS_MAX 16 // numero maximo de argumentos por comando


#include <stdint.h>
//inicializa o shell
void shell_init(void);

//comandos para escrever na tela
void shell_write(const char *s);
void shell_writeln(const char *s);
void shell_write_num(uint32_t n);
void shell_write_char(const char c);
void shell_set_cor(unsigned char cor);
void shell_clear(void);
#endif