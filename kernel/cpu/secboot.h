#ifndef SECBOOT_H
#define SECBOOT_H

/*
 * secboot — verificacao de integridade do kernel em BIOS legado.
 *
 * BIOS antigo nao tem Secure Boot (e feature UEFI). O que podemos fazer:
 *   1. Medir o kernel (hash djb2 do segmento de codigo)
 *   2. Comparar com hash armazenado em /boot/kernel.hash no disco
 *   3. Alertar se divergir (boot continua — modo aviso)
 *
 * Para modo strict: descomentar SECBOOT_HALT_ON_FAIL em secboot.c
 *
 * Gerar o hash:
 *   python3 -c "
 *   data=open('SpaceOS.bin','rb').read()
 *   h=5381
 *   for b in data: h=((h<<5)+h+b)&0xFFFFFFFF
 *   print(hex(h))
 *   " > kernel.hash
 *   mcopy -i disco.img kernel.hash ::/boot/kernel.hash
 */

#include <stdint.h>

void     secboot_init(void);        /* mede e verifica integridade */
void     secboot_status(void);      /* imprime status na shell */
uint32_t secboot_measure(void);     /* retorna hash do kernel carregado */
int      secboot_verified(void);    /* 1 = ok ou sem hash armazenado */

#endif
