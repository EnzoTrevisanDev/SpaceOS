#include "shell.h"
#include "comandos.h"
#include "../teclado.h"


// estado da tela
#define VGA_BASE 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25

static unsigned short *vga = (unsigned short *)VGA_BASE;
static int cur_col = 0;
static int cur_lin = 0;
static unsigned char cor_atual = 0x0F; /* branco sobre preto */

//buffer de input
static char input_buf[MAX_INPUT];
static int input_len = 0;

// historico
static char historico[HISTORICO_MAX][MAX_INPUT];
static int   hist_total  = 0;   /* quantos comandos foram guardados */
static int   hist_nav    = -1;  /* posicao atual na navegacao (-1 = sem navegar) */
//=====================
//====func de tela=====
//=====================
void shell_clear(void){
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++){
        vga[i] = (unsigned short)(' ' | (cor_atual << 8));
    }
    cur_col = 0;
    cur_lin = 0;
}

//rola a tela uma linha para cima
static void scroll(void) {
    // copia linhas 1..24 para 0..23
    for (int i = 0; i < (VGA_ROWS - 1) * VGA_COLS; i++)
        vga[i] = vga[i + VGA_COLS];
    /* limpa a ultima linha */
    for (int i = (VGA_ROWS - 1) * VGA_COLS; i < VGA_ROWS * VGA_COLS; i++)
        vga[i] = (unsigned short)(' ' | (cor_atual << 8));
    cur_lin = VGA_ROWS - 1;
}

void shell_write_char(char c){
    if (c == '\n'){
        cur_col = 0;
        cur_lin++;
        if( cur_lin >= VGA_ROWS) scroll();
        return;
    }
    if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            vga[cur_lin * VGA_COLS + cur_col] = ' ' | (cor_atual << 8);
        }
        return;
    }
    vga[cur_lin * VGA_COLS + cur_col] = (unsigned short)((unsigned char)c | (cor_atual << 8));
    cur_col++;
    if (cur_col >= VGA_COLS) {
        cur_col = 0;
        cur_lin++;
        if (cur_lin >= VGA_ROWS) scroll();
    }
}

void shell_write(const char *s) {
    while(*s) shell_write_char(*s++);
}
void shell_writeln(const char *s) {
    shell_write(s);
    shell_write_char('\n');
}

void shell_set_cor(unsigned char c) {
    cor_atual = c | 0x00;  /* fundo sempre preto, so muda o texto */
}


void shell_write_num(uint32_t n){
    if (n == 0){ shell_write_char('0'); return;}
    char buf[12];
    int i = 0;
    while(n > 0){
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    //inverte
    for (int j = i - 1; j >= 0; j--){
        shell_write_char(buf[j]);
    }
}

//=====================
//====  prompt   ======
//=====================

static void prompt(void){
    unsigned char cor_salva = cor_atual;
    shell_set_cor(0x0A); //verde
    shell_write("SpaceOS");
    shell_set_cor(0x07); // cinza
    shell_write("> ");
    cor_atual = cor_salva;
}
//=================================================
//====  PARSER — divide a string em argv[]   ======
//=================================================
static char  tokens[MAX_INPUT];
static char *argv[ARGS_MAX];
static int   argc;

static void parser(const char *entrada){
    argc = 0;
    int i = 0, j = 0;

    //copia para buffer mutavel
    while(entrada[i] && j < MAX_INPUT - 1){
        tokens[j++] = entrada[i++];
    }
    tokens[j] = '\0';

    i = 0;
    while (tokens[i] != '\0' && argc < ARGS_MAX){
        //pula espacos
        while(tokens[i] == ' ') i++;
        if (tokens[i] == '\0') break;

        //inicio de um token
        argv[argc++] = &tokens[i];

        //avanca ate o proximo espaco ou fim
        while (tokens[i] != ' ' && tokens[i] != '\0') i++;

        if(tokens[i] == ' ') tokens[i++] = '\0';
    }

}
//=====================
//====  Historico   ===
//=====================
static void str_copia(char *dst, const char *src, int max){
    int i = 0;
    while (src[i] && i < max - 1) {dst[i] = src[i]; i++;}
    dst[i] = '\0';
}

static void historico_salvar(const char *cmd){
    //nao salva comando vazio ou igual ao ultimo
    if (cmd[0] == '\0') return;
    if (hist_total > 0) {
        int ultimo = (hist_total - 1) % HISTORICO_MAX;
        int igual = 1;
        for (int i = 0; cmd[i] || historico[ultimo][i]; i++) {
            if (cmd[i] != historico[ultimo][i]) { igual = 0; break; }
        }
        if (igual) return;
    }
    str_copia(historico[hist_total % HISTORICO_MAX], cmd, MAX_INPUT);
    hist_total++;
}
/* Carrega comando do historico na posicao relativa ao final */
static void historico_carregar(int pos) {
    if (hist_total == 0) return;

    /* apaga o input atual na tela */
    for (int i = 0; i < input_len; i++) {
        cur_col--;
        if (cur_col < 0) cur_col = 0;
        vga[cur_lin * VGA_COLS + cur_col] = ' ' | (cor_atual << 8);
    }
    input_len = 0;

    /* calcula indice real */
    int idx = (hist_total - 1 - pos) % HISTORICO_MAX;
    if (idx < 0) idx += HISTORICO_MAX;
    const char *cmd = historico[idx];

    /* copia para input e exibe */
    int i = 0;
    while (cmd[i] && input_len < MAX_INPUT - 1) {
        input_buf[input_len++] = cmd[i];
        shell_write_char(cmd[i]);
        i++;
    }
    input_buf[input_len] = '\0';
}

//==================================
//====  LOOP PRINCIPAL DA SHELL   ==
//==================================
void shell_init(void){
    shell_clear();
    shell_set_cor(0x0F);

    shell_writeln("================================");
    shell_writeln("  SpaceOS v0.2 - Lua");
    shell_writeln("  Shell inicializada");
    shell_writeln("  Digite 'help' para comecar");
    shell_writeln("================================");
    shell_writeln("");

    comandos_init();
    prompt();
        /* importa o teclado */
    extern char teclado_ultimo_char(void);

    // Main loop
    while (1) {
        char c = teclado_ultimo_char();
        if (!c) continue;

        /* ENTER — processa o comando */
        if (c == '\n') {
            shell_write_char('\n');
            input_buf[input_len] = '\0';

            if (input_len > 0) {
                historico_salvar(input_buf);
                hist_nav = -1;

                parser(input_buf);
                if (comandos_executar(argc, argv) == -1) {
                    shell_write("comando nao encontrado: ");
                    shell_writeln(input_buf);
                }
            }

            input_len = 0;
            input_buf[0] = '\0';
            prompt();
            continue;
        }

        /* BACKSPACE */
        if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                shell_write_char('\b');
            }
            continue;
        }
        //SETA para cima
        if ((unsigned char)c == TECLA_SETA_CIMA) {
            if (hist_total == 0) continue;
            if (hist_nav < hist_total - 1) hist_nav++;
            historico_carregar(hist_nav);
            continue;
        }

        //Seta para baixo
        if ((unsigned char)c == TECLA_SETA_BAIXO) {
            if (hist_nav > 0) {
                hist_nav--;
                historico_carregar(hist_nav);
            } else if (hist_nav == 0){
                for (int i = 0; i < input_len; i++){
                    cur_col--;
                    if (cur_col < 0) cur_col = 0;
                    vga[cur_lin * VGA_COLS + cur_col] = ' ' | (cor_atual << 8);
                }
                input_len = 0;
                input_buf[0] = '\0';
                hist_nav = -1;
            }
            continue;
        }    
        /* Caracteres normais */
        if (c >= ' ' && input_len < MAX_INPUT - 1) {
            input_buf[input_len++] = c;
            shell_write_char(c);
            hist_nav = -1;
        }
    }

}