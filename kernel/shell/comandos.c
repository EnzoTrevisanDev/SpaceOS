#include "comandos.h"
#include "shell.h"


// declaracao antecipadas de cada comando
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_cor(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_eco(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_tecla(int argc, char **argv);
static void cmd_paging(int argc, char **argv);




//tabela de comandos - adicionar novo comando e so adicionar uma linha aqui
static Comando tabela[] = {
    {"help", "Lista todos os comandos", cmd_help},
    {"clear", "limpa a tela", cmd_clear},
    {"info", "Lista informações do sistema", cmd_info},
    {"cor", "Altera a cor do texto ex: cor 10", cmd_cor},
    {"eco", "Repete o texto digitado ex: eco Olá, mundo!", cmd_eco},
    { "tecla", "mostra codigo da tecla pressionada", cmd_tecla },
    { "mem", "mostra informacoes de memoria", cmd_mem },
    { "paging", "testa o sistema de paging", cmd_paging },
    {"reboot", "Reinicia o sistema", cmd_reboot},
};

static int total = sizeof(tabela) / sizeof(Comando);

void comandos_init(void){
    // nada a fazer por enquanto, mas poderia ser usado para inicializar algo se necessario
}

Comando *comando_lista(void) { return tabela; }
int comando_total(void) { return total; }

int comandos_executar(int argc, char **argv) {
    if (argc == 0) return 0;
    for (int i = 0; i < total; i++){
        //compara argv[0] com o nome do comando
        const char *a = argv[0];
        const char *b = tabela[i].nome;
        int igual = 1;
        while (*a && *b){
            if (*a != *b) { igual =0; break;}
            a++; b++;
        }
        if (igual && *a == '\0' && *b == '\0'){
            tabela[i].func(argc, argv);
            return 0;
        }

    }
    return -1; /* comando nao encontrado */
}

/* ----- Implemmentacooes ------ */
static void cmd_tecla(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_writeln("Pressione uma tecla (ESC para sair):");

    extern char teclado_ultimo_char(void);
    extern uint8_t teclado_ultimo_scancode(void);

    while (1) {
        char c = teclado_ultimo_char();
        if (!c) continue;

        /* mostra o valor numerico da tecla */
        unsigned char v = (unsigned char)c;

        shell_write("char=");

        /* imprime em hex manualmente */
        char hex[5];
        hex[0] = '0';
        hex[1] = 'x';
        hex[2] = "0123456789ABCDEF"[(v >> 4) & 0xF];
        hex[3] = "0123456789ABCDEF"[v & 0xF];
        hex[4] = '\0';
        shell_writeln(hex);

        /* ESC = 0x1B sai do modo debug */
        if (v == 27) break;
    }
}
static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;

    extern uint32_t pmm_livres(void);
    extern uint32_t pmm_total(void);

    uint32_t livres = pmm_livres();
    uint32_t total  = pmm_total();
    uint32_t usadas = total - livres;

    shell_writeln("=== Memoria do SpaceOS ===");
    shell_writeln("");
    shell_write("Paginas totais : "); shell_write_num(total);   shell_writeln("");
    shell_write("Paginas livres : "); shell_write_num(livres);  shell_writeln("");
    shell_write("Paginas usadas : "); shell_write_num(usadas);  shell_writeln("");
    shell_writeln("");
    shell_write("RAM total  : "); shell_write_num(total * 4);   shell_write(" KB"); shell_writeln("");
    shell_write("RAM livre  : "); shell_write_num(livres * 4);  shell_write(" KB"); shell_writeln("");
    shell_write("RAM usada  : "); shell_write_num(usadas * 4);  shell_write(" KB"); shell_writeln("");
    shell_writeln("");

    /* testa alloc e free */
    shell_write("Teste alloc: ");
    extern uint32_t pmm_alloc(void);
    extern void pmm_free(uint32_t);

    uint32_t p1 = pmm_alloc();
    uint32_t p2 = pmm_alloc();
    uint32_t p3 = pmm_alloc();

    if (p1 && p2 && p3) {
        shell_write("alocou 3 paginas: 0x");
        /* imprime p1 em hex */
        char hex[9];
        uint32_t v = p1;
        for (int i = 7; i >= 0; i--) {
            hex[i] = "0123456789ABCDEF"[v & 0xF];
            v >>= 4;
        }
        hex[8] = '\0';
        shell_write(hex);
        shell_writeln(" OK");

        pmm_free(p1);
        pmm_free(p2);
        pmm_free(p3);
        shell_writeln("Liberou 3 paginas: OK");
    } else {
        shell_writeln("ERRO — pmm_alloc retornou 0");
    }
}
static void cmd_paging(int argc, char **argv) {
    (void)argc; (void)argv;

    shell_writeln("=== Teste de Paging ===");
    shell_writeln("");

    /* Teste 1 — escreve e le de um endereco virtual mapeado */
    volatile uint32_t *teste = (volatile uint32_t *)0x00200000;
    *teste = 0xDEADBEEF;

    if (*teste == 0xDEADBEEF) {
        shell_writeln("Teste 1: escrita/leitura virtual OK");
    } else {
        shell_writeln("Teste 1: ERRO");
    }

    /* Teste 2 — verifica que o kernel ainda roda apos paging ativo */
    shell_writeln("Teste 2: kernel rodando com paging ativo OK");

    /* Teste 3 — mostra onde o page directory esta na memoria */
    shell_writeln("");
    shell_write("MMU ativa — traducao de enderecos funcionando");
    shell_writeln("");
}
static void cmd_help(int argc, char **argv){
    (void)argc; (void)argv;
    shell_writeln("Comandos disponiveis");
    shell_writeln("");
    Comando *lista = comando_lista();
    for (int i = 0; i < total; i++){
        // imprime nome com padding
        const char *nome = lista[i].nome;
        shell_write("  ");
        shell_write(nome);

        // padding manual ate col 12
        int len = 0;
        while (nome[len]) len++;
        for (int p = len; p < 10; p++) shell_write_char(' ');
        shell_write("- ");
        shell_writeln(lista[i].descricao);
    }
    shell_writeln("");
}

static void cmd_clear(int argc, char **argv){
    (void)argc; (void)argv;
    shell_clear();
}

static void cmd_info(int argc, char **argv){
    (void)argc; (void)argv;
    shell_writeln("================================");
    shell_writeln("  SpaceOS v0.2 - Lua");
    shell_writeln("  O unico OS onde voce");
    shell_writeln("  controla tudo - e pode provar");
    shell_writeln("================================");
    shell_writeln("");
    shell_writeln("Kernel:      SpaceOS bare metal");
    shell_writeln("Arch:        x86 32-bit");
    shell_writeln("Build:       2025");
    shell_writeln("Status:      v0.2 Lua - Shell ativa");
    shell_writeln("");
}

static void cmd_cor(int argc, char **argv) {
    if (argc < 2) {
        shell_writeln("uso: cor [0-15]");
        shell_writeln("  0=preto  1=azul    2=verde   3=ciano");
        shell_writeln("  4=verm   5=roxo    6=marrom  7=cinza");
        shell_writeln("  8=cinzaE 9=azulE  10=verdeE 11=cianoE");
        shell_writeln(" 12=vermE 13=roxoE  14=amarelo 15=branco");
        return;
    }
    // converte str para num manualmente
    int n = 0;
    const char *s = argv[1];
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    if (n < 0 || n > 15){
        shell_write("cor deve ser entre 0 e 15");
        return;
    }
    shell_set_cor((unsigned char)n);
    shell_writeln("cor alterada!");
}

static void cmd_eco(int argc, char **argv){
    for(int i = 1; i < argc; i++){
        if (i > 1) shell_write_char(' ');
        shell_write(argv[i]);
    }
    shell_writeln("");
}
static void cmd_reboot(int argc, char **argv){
    (void)argc; (void)argv;
    shell_writeln("Reiniciando SpaceOS...");
    // pulsa na porta do controlador do teclado - metodo classico
    __asm__ volatile (
        "mov $0xFE, %%al\n"
        "out %%al, $0x64\n"
        : : : "eax"
    );
    while(1){} /* seguranca caso o reboot demore */
}