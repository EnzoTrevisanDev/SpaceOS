#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Numeros de syscall — o processo coloca em EAX antes do int 0x80 */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_SLEEP   3

void syscall_init(void);
void syscall_handler(void);
void syscall_handler_asm(void);

#endif