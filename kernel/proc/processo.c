#include "processo.h"
#include "../mem/kheap.h"
#include "../shell/shell.h"
#include "identidade.h"
#include "../proc/sched.h"

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


    kproc->id = identidade_gerar(0, sched_ticks(), (uint32_t)kproc);

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


    proc->id = identidade_gerar(proc->pid, sched_ticks(), (uint32_t)stack);

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


int32_t proc_fork(void) {
    processo_t *pai = proc_atual_ptr;
    if (!pai) return -1;

    /* Kernel (PID 0) nao pode fazer fork — nao tem stack propria */
    if (pai->stack == 0) {
        shell_writeln("fork: processo kernel nao pode fazer fork");
        return -1;
    }

    processo_t *filho = kmalloc(sizeof(processo_t));
    if (!filho) return -1;

    uint8_t *stack_filho = kmalloc(STACK_SIZE);
    if (!stack_filho) { kfree(filho); return -1; }

    *filho = *pai;

    filho->pid    = prox_pid++;
    filho->estado = PROC_READY;
    filho->stack  = stack_filho;
    filho->proximo = 0;

    for (uint32_t i = 0; i < STACK_SIZE; i++)
        stack_filho[i] = pai->stack[i];

    uint32_t offset = pai->ctx.esp - (uint32_t)pai->stack;
    filho->ctx.esp  = (uint32_t)stack_filho + offset;

    uint32_t *stack_ptr = (uint32_t *)filho->ctx.esp;
    stack_ptr[7] = 0;

    str_copy(filho->nome, pai->nome, 28);
    filho->id = identidade_gerar(filho->pid, sched_ticks(), (uint32_t)stack_filho);
    filho->nome[27] = '\0';

    fila_adicionar(filho);
    return (int32_t)filho->pid;
}
int32_t proc_exec(void (*entrada)(void)) {
    processo_t *proc = proc_atual_ptr;
    if (!proc) return -1;

    /* Reconfigura a stack do processo para comecar na nova funcao */
    uint32_t *topo = (uint32_t *)(proc->stack + STACK_SIZE);

    *--topo = 0x00000202;          /* EFLAGS */
    *--topo = 0x08;                /* CS */
    *--topo = (uint32_t)entrada;   /* EIP — nova funcao */
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

    return 0;
}