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
/* 0x00 */ 0,
/* 0x01 */ 27,       /* ESC */
/* 0x02 */ '1',
/* 0x03 */ '2',
/* 0x04 */ '3',
/* 0x05 */ '4',
/* 0x06 */ '5',
/* 0x07 */ '6',
/* 0x08 */ '7',
/* 0x09 */ '8',
/* 0x0A */ '9',
/* 0x0B */ '0',
/* 0x0C */ '-',
/* 0x0D */ '=',
/* 0x0E */ '\b',     /* Backspace */
/* 0x0F */ '\t',     /* Tab */
/* 0x10 */ 'q',
/* 0x11 */ 'w',
/* 0x12 */ 'e',
/* 0x13 */ 'r',
/* 0x14 */ 't',
/* 0x15 */ 'y',
/* 0x16 */ 'u',
/* 0x17 */ 'i',
/* 0x18 */ 'o',
/* 0x19 */ 'p',
/* 0x1A */ '`',      /* ~ no ABNT2 */
/* 0x1B */ '[',
/* 0x1C */ '\n',     /* Enter */
/* 0x1D */ 0,        /* Ctrl esq */
/* 0x1E */ 'a',
/* 0x1F */ 's',
/* 0x20 */ 'd',
/* 0x21 */ 'f',
/* 0x22 */ 'g',
/* 0x23 */ 'h',
/* 0x24 */ 'j',
/* 0x25 */ 'k',
/* 0x26 */ 'l',
/* 0x27 */ ';',      /* c cedilha no ABNT2 */
/* 0x28 */ '\'',
/* 0x29 */ '\'',
/* 0x2A */ 0,        /* Shift esq */
/* 0x2B */ ']',
/* 0x2C */ 'z',
/* 0x2D */ 'x',
/* 0x2E */ 'c',
/* 0x2F */ 'v',
/* 0x30 */ 'b',
/* 0x31 */ 'n',
/* 0x32 */ 'm',
/* 0x33 */ ',',
/* 0x34 */ '.',
/* 0x35 */ '/',
/* 0x36 */ 0,        /* Shift dir */
/* 0x37 */ '*',
/* 0x38 */ 0,        /* Alt */
/* 0x39 */ ' ',      /* Espaco */
/* 0x3A */ 0,        /* Caps Lock */
/* 0x3B */ 0,        /* F1 */
/* 0x3C */ 0,        /* F2 */
/* 0x3D */ 0,        /* F3 */
/* 0x3E */ 0,        /* F4 */
/* 0x3F */ 0,        /* F5 */
/* 0x40 */ 0,        /* F6 */
/* 0x41 */ 0,        /* F7 */
/* 0x42 */ 0,        /* F8 */
/* 0x43 */ 0,        /* F9 */
/* 0x44 */ 0,        /* F10 */
/* 0x45 */ 0,        /* Num Lock */
/* 0x46 */ 0,        /* Scroll Lock */
/* 0x47 */ '7',      /* Num 7 */
/* 0x48 */ '8',      /* Num 8 / Seta cima SEM E0 */
/* 0x49 */ '9',      /* Num 9 */
/* 0x4A */ '-',      /* Num - */
/* 0x4B */ '4',      /* Num 4 / Seta esq SEM E0 */
/* 0x4C */ '5',      /* Num 5 */
/* 0x4D */ '6',      /* Num 6 / Seta dir SEM E0 */
/* 0x4E */ '+',      /* Num + */
/* 0x4F */ '1',      /* Num 1 */
/* 0x50 */ '2',      /* Num 2 / Seta baixo SEM E0 */
/* 0x51 */ '3',      /* Num 3 */
/* 0x52 */ '0',      /* Num 0 */
/* 0x53 */ '.',      /* Num . */
/* 0x54 */ 0,
/* 0x55 */ 0,
/* 0x56 */ '\\',     /* tecla extra ABNT2 entre Z e Shift */
/* 0x57 */ 0,        /* F11 */
/* 0x58 */ 0,        /* F12 */
/* 0x59 */ 0,
/* 0x5A */ 0,
/* 0x5B */ 0,
/* 0x5C */ 0,
/* 0x5D */ 0,
/* 0x5E */ 0,
/* 0x5F */ 0,
/* 0x60 */ 0,
/* 0x61 */ 0,
/* 0x62 */ 0,
/* 0x63 */ 0,
/* 0x64 */ 0,
/* 0x65 */ 0,
/* 0x66 */ 0,
/* 0x67 */ 0,
/* 0x68 */ 0,
/* 0x69 */ 0,
/* 0x6A */ 0,
/* 0x6B */ 0,
/* 0x6C */ 0,
/* 0x6D */ 0,
/* 0x6E */ 0,
/* 0x6F */ 0,
/* 0x70 */ 0,
/* 0x71 */ 0,
/* 0x72 */ 0,
/* 0x73 */ '/',      /* / do ABNT2 — tecla extra */
/* 0x74 */ 0,
/* 0x75 */ 0,
/* 0x76 */ 0,
/* 0x77 */ 0,
/* 0x78 */ 0,
/* 0x79 */ 0,
/* 0x7A */ 0,
/* 0x7B */ 0,
/* 0x7C */ 0,
/* 0x7D */ 0,
/* 0x7E */ 0,
/* 0x7F */ 0,
};

#define TECLA_SETA_CIMA 0x81
#define TECLA_SETA_BAIXO 0x82 
#define TECLA_SETA_ESQ 0x83
#define TECLA_SETA_DIR 0x84 

static char ultimo_char = 0;
static int prefixo_e0 = 0; // 1 se o ultimo byte foi 0xE0
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
    //static uint8_t debug_buf[4] = {0,0,0,0};
    //static int debug_idx = 0;
    //debug_buf[debug_idx % 4] = scancode;
    //debug_idx++;

    if (scancode & 0x80) {
        prefixo_e0 = 0;
        pic_ack(1);
        return;
    }
    //prefixo de tecla especial
    if (scancode == 0xE0){
        prefixo_e0 = 1;
        pic_ack(1);
        return;
    }

    if(prefixo_e0) {
        prefixo_e0 = 0;
        //segundo byte da sequencia especial
        switch (scancode)
        {
            case 0x48: ultimo_char = TECLA_SETA_CIMA;  break;
            case 0x50: ultimo_char = TECLA_SETA_BAIXO; break;
            case 0x4B: ultimo_char = TECLA_SETA_ESQ;   break;
            case 0x4D: ultimo_char = TECLA_SETA_DIR;   break;
            default:   /* scancode desconhecido — manda como valor especial para debug */
                ultimo_char = (char)(0x90 + scancode); break;
        }
        pic_ack(1);
        return;
    }
    if (scancode == 0x48) { ultimo_char = TECLA_SETA_CIMA;  pic_ack(1); return; }
    if (scancode == 0x50) { ultimo_char = TECLA_SETA_BAIXO; pic_ack(1); return; }
    if (scancode == 0x4B) { ultimo_char = TECLA_SETA_ESQ;   pic_ack(1); return; }
    if (scancode == 0x4D) { ultimo_char = TECLA_SETA_DIR;   pic_ack(1); return; }

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