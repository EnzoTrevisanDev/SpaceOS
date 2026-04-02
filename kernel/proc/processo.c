#include "processo.h"
#include "../mem/kheap.h"
#include "../shell/shell.h"

//estado global do scheduler
static processo_t *proc_atual_ptr = 0; //processo rodando agora
static processo_t *fila_pronto = 0; // fila de prontos
static uint32_t prox_pid = 1;

// func auxiliar - copia str sem stdlib
static void str_copy(char *dst, const char *src, int max){
    int i = 0;
    while(src[i] && i < max - 1){ dst[i] = src[i]; i++;}
    dst[i] = '\0';
}

processo_t *proc_atual(void) {return proc_atual_ptr;}
processo_t *proc_fila(void) {return fila_pronto;}

//adiciona processo no final da fila
static void fila_adicionar(processo_t *p){
    p->proximo = 0;
    if(!fila_pronto){ 
        fila_pronto = p; 
        return;
    }

    processo_t *ultimo = fila_pronto;
    while (ultimo->proximo) ultimo = ultimo->proximo;
    ultimo->proximo = p;   
}

void proc_init(void){
    //cira o PCB do kernel - Processo 0
    //o kernel ja esta rodando so falo que ele existe...
    processo_t *kproc = kmalloc(sizeof(processo_t));
    if(!kproc) return;
    kproc->pid = 0;
    kproc->estado = PROC_RUNNING;
    kproc->prioridade = 1;
    kproc->page_dir = 0;
    kproc->stack = 0;
    kproc->stack_size = 0;
    kproc->proximo = 0;
    str_copy(kproc->nome, "kernel", 32);

    proc_atual_ptr = kproc;
    fila_pronto = kproc;
}

processo_t *proc_criar(const char *nome, void (*entrada)(void), uint8_t prioridade) {
    processo_t *proc = kmalloc(sizeof(processo_t));
    if (!proc) return 0;

    uint8_t *stack = kmalloc(STACK_SIZE);
    if (!stack) { kfree(proc); return 0; }

    proc->pid        = prox_pid++;
    proc->estado     = PROC_READY;
    proc->prioridade = prioridade;
    proc->stack      = stack;
    proc->stack_size = STACK_SIZE;
    proc->page_dir   = 0;
    proc->proximo    = 0;
    str_copy(proc->nome, nome, 32);

    uint32_t *topo = (uint32_t *)(stack + STACK_SIZE);

    *--topo = 0x00000202;
    *--topo = 0x08;
    *--topo = (uint32_t)entrada;
    *--topo = 0;  /* EAX */
    *--topo = 0;  /* ECX */
    *--topo = 0;  /* EDX */
    *--topo = 0;  /* EBX */
    *--topo = 0;  /* ESP dummy */
    *--topo = 0;  /* EBP */
    *--topo = 0;  /* ESI */
    *--topo = 0;  /* EDI */

    proc->ctx.esp = (uint32_t)topo;
    proc->ctx.eip = (uint32_t)entrada;

    fila_adicionar(proc);
    return proc;
}


void proc_encerrar(void){
    if(proc_atual_ptr){
        proc_atual_ptr->estado = PROC_MORTO;
        //loop infinito - scheduler vai remover no proximo tick
        while (1) {}
    }
}

void proc_set_atual(processo_t *p) {
    proc_atual_ptr = p;
}