/* kernel/teclado.c
   O teclado PS/2 envia um "scancode" quando uma tecla e pressionada.
   Cada tecla tem um numero unico. Precisamos traduzir para ASCII. */

#include "teclado.h"
#include "cpu/idt.h"
#include "cpu/pic.h"

#define PORTA_TECLADO 0x60  /* porta de I/O onde o teclado envia scancodes */

/* Tabela de traducao: scancode -> caractere ASCII
   Posicao = scancode, valor = caractere correspondente */
static char scancode_map[128] = {
    0, 0,
    '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};

static char ultimo_char = 0;

/* Funcao auxiliar para ler porta de I/O */
static uint8_t in_byte(uint16_t porta) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(porta));
    return v;
}


/* Handler da interrupcao do teclado — chamado automaticamente pela CPU
   quando uma tecla e pressionada */
void teclado_handler(void) {
    uint8_t scancode = in_byte(PORTA_TECLADO);

    /* Bit 7 = 1 significa tecla solta (key up) — ignora */
    if (scancode & 0x80) {
        pic_ack(1);
        return;
    }

    /* Converte scancode para ASCII */
    if (scancode < 128 && scancode_map[scancode]) {
        ultimo_char = scancode_map[scancode];
    }

    /* Avisa o PIC que tratamos a interrupcao — obrigatorio */
    pic_ack(1);
}

char teclado_ultimo_char(void) {
    char c = ultimo_char;
    ultimo_char = 0;   /* limpa depois de ler */
    return c;
}

void teclado_init(void) {
    /* Registra o WRAPPER assembly, nao o handler C direto
       O wrapper salva os registradores antes de chamar o C */
    idt_set(33, (uint32_t)teclado_handler_asm, 0x08, 0x8E);
}