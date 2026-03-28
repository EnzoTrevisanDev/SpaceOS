#ifndef COMMANDOS_H
#define COMMANDOS_H


typedef struct {
    const char *nome;
    const char *descricao;
    void (*func)(int arggc, char **argv);
} Comando;

// Registra os comandos disponiveis no shell
void comandos_init(void);

// retornar array de comandos e o tamanho
Comando *comandos_lista(void);
int comandos_total(void);

//executa m comando pelo nome. Retorna 0 se encontrou, -1 se nao encontrou
int comandos_executar(int argc, char **argv);
#endif