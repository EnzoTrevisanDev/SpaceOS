#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Numeros de syscall — o processo coloca em EAX antes do int 0x80 */
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_GETPID      2
#define SYS_SLEEP       3
/* v0.6 Urano — Package Manager */
#define SYS_PKG_INSTALL 10  /* EBX = ponteiro para path do .spk */
#define SYS_PKG_REMOVE  11  /* EBX = ponteiro para nome do pacote */
#define SYS_PKG_LIST    12  /* sem argumentos */

void syscall_init(void);
void syscall_handler(void);
void syscall_handler_asm(void);

#endif