#include "syscall.h"
#include "../cpu/idt.h"
#include "../proc/processo.h"
#include "../shell/shell.h"

/* O handler recebe os registradores empurrados pelo Assembly
   EAX = numero da syscall
   EBX = argumento 1
   ECX = argumento 2
   EDX = argumento 3 */
void syscall_handler(void) {
    /* Lemos EAX diretamente do registrador */
    uint32_t num, ebx, ecx;
    __asm__ volatile (
        "mov %%eax, %0\n"
        "mov %%ebx, %1\n"
        "mov %%ecx, %2\n"
        : "=r"(num), "=r"(ebx), "=r"(ecx)
    );

    switch (num) {

        case SYS_EXIT:
            /* Encerra o processo atual */
            proc_encerrar();
            break;

        case SYS_WRITE:
            /* Escreve uma string na tela
               EBX = ponteiro para a string
               ECX = tamanho */
            if (ebx) {
                const char *s = (const char *)ebx;
                uint32_t tam = ecx;
                for (uint32_t i = 0; i < tam && s[i]; i++)
                    shell_write_char(s[i]);
            }
            break;

        case SYS_GETPID: {
            /* Retorna o PID do processo atual em EAX */
            processo_t *p = proc_atual();
            uint32_t pid = p ? p->pid : 0;
            __asm__ volatile ("mov %0, %%eax" : : "r"(pid));
            break;
        }

        default:
            shell_write("[syscall] numero desconhecido: ");
            shell_write_num(num);
            shell_writeln("");
            break;
    }
}

void syscall_init(void) {
    /* Registra o handler na interrupcao 0x80 (128)
       0xEE = presente + DPL 3 — permite chamada do Ring 3 */
    idt_set(0x80, (uint32_t)syscall_handler_asm, 0x08, 0xEE);
}