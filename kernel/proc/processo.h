#ifndef PROCESSO_H
#define PROCESSO_H

#include <stdint.h>

//estados dos processos
#define PROC_RUNNING 0
#define PROC_READY 1
#define PROC_BLOCKED 2
#define PROC_MORTO 3


//contexto da CPU
typedef struct{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy; //esp no momento do pushed - nao usamos...
    uint32_t ebs;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip; //proxima instrucao a executar
    uint32_t esp; //topo real da stack do processo
}__attribute__((packed)) contexto_t;


//PCB - Process control brock = tudo o que define um processo
typedef struct processo {
    uint32_t pid;
    uint8_t estado;
    uint8_t prioridade; //0=baixa 1=media 2=alta
    uint8_t creditos;
    contexto_t ctx; //estado da CPU salvo
    uint32_t *page_dir; //page directory proprio
    uint8_t *stack; //base da stack alocada
    uint32_t stack_size; //base da stack alocada
    char nome[32];
    struct processo *proximo; //proximo na fila do scheduler    
} processo_t;

//constantes
#define MAX_PROCESSOS 16
#define STACK_SIZE (16 * 1024) //16KB por processo


// API
void proc_init(void);
processo_t *proc_criar(const char *nome, void (*entrada)(void), uint8_t prioridade);
void proc_encerrar(void);
processo_t *proc_atual(void);
processo_t *proc_fila(void);
void proc_set_atual(processo_t *p);
#endif