#define VGA_BASE 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25
#define COLOR 0x0F /* 0=fundo preto, F=texto branco */

//ponteiro para o buffer VGA
static unsigned short *vga = (unsigned short *)VGA_BASE;

//onde o cursor vai aparecer
static int col = 0;
static int lin = 0;

// limpa a tela
/* 80*25=2000 slots com espaco em branco */
void clean_screen() {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        vga[i] = ' ' | (COLOR << 8);
    }
    col = 0;
    lin = 0;
}

// escreve um caractere na tela
void write_char(char c) {
    if (c == '\n') {
        col = 0;
        lin++;
        return;
    }

    // calculate where the memory should find the VGA char should go
    int pos = lin * VGA_COLS + col;

    //  write: byte baixo = ASCII, byte alto = cor
    vga[pos] = (unsigned short)((unsigned char)c | (COLOR << 8));
    
    // move the cursor
    col++;
    if (col >= VGA_COLS) {
        col = 0;
        lin++;
    }
    
}

// -- WRINTING --- Percorre a string ate o \0 e chama escrever_char para cada letra
void write(const char *s) {
    while (*s != '\0') {
        write_char(*s);
        s++;
    }
}

// -- KERNEL MAIN ---
// call the assembly code to initialize the kernel and then call the C function

void kernel_main() {
    clean_screen();

    write("hello, world!");
    write("=================================\n");
    write("  MeuOS v0.1 - Bare Metal\n");
    write("=================================\n");
    write("\n");
    write("Kernel inicializado.\n");
    write("VGA text mode funcionando.\n");
    write("\n");
    write("Proximo passo: teclado e interrupcoes.\n");

    /* O OS nao pode retornar — fica aqui para sempre */
    while (1) {}

}
