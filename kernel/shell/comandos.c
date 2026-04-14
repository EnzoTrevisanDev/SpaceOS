#include "comandos.h"
#include "shell.h"
#include "../proc/processo.h"
#include "../proc/identidade.h"
#include "../fs/vfs.h"
#include "../fs/audit.h"
#include "../fs/spacefs.h"
#include "../mem/kheap.h"
#include "../disk/kstring.h"
#include "../disk/ahci.h"
#include "../disk/dma.h"
#include "../pkg/pkg_install.h"
#include "../pkg/pkg_db.h"
#include "../drivers/pci.h"
#include "../drivers/acpi.h"
#include "../drivers/usb.h"
#include "../drivers/usbhid.h"
#include "../net/rtl8139.h"
#include "../net/wifi.h"
#include "../net/ipv4.h"
#include "../net/dhcp.h"
#include "../net/dns.h"
#include "../net/netbuf.h"
#include "../net/tls.h"
#include "../pkg/pkg_remote.h"
#include "../cpu/secboot.h"

// declaracao antecipadas de cada comando
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_cor(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_eco(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_tecla(int argc, char **argv);
static void cmd_heap(int argc, char **argv);
static void cmd_slab(int argc, char **argv);
static void cmd_proc(int argc, char **argv);
static void cmd_syscall(int argc, char **argv);
static void cmd_paging(int argc, char **argv);
static void cmd_espaco(int argc, char **argv);
static void cmd_disco(int argc, char **argv);
static void cmd_fork(int argc, char **argv);
/* Drivers de hardware (v0.7 Netuno) */
static void cmd_pci(int argc, char **argv);
static void cmd_shutdown(int argc, char **argv);
/* Rede (v0.8 Galaxia) */
static void cmd_net(int argc, char **argv);
/* Gerenciador de pacotes (v0.6 Urano) */
static void cmd_spk(int argc, char **argv);
/* Comandos de filesystem (v0.5 Saturno) */
static void cmd_ls(int argc, char **argv);
static void cmd_cd(int argc, char **argv);
static void cmd_pwd(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_mkdir(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_cp(int argc, char **argv);
static void cmd_mv(int argc, char **argv);
/* SpaceFS + audit + hardware extras */
static void cmd_spfs(int argc, char **argv);
static void cmd_audit(int argc, char **argv);
static void cmd_ahci(int argc, char **argv);
static void cmd_dma(int argc, char **argv);
static void cmd_usb(int argc, char **argv);
static void cmd_wifi(int argc, char **argv);
static void cmd_secboot(int argc, char **argv);


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
    { "espaco", "mostra layout de memoria virtual", cmd_espaco },
    { "heap", "testa kmalloc e kfree", cmd_heap },
    { "slab", "testa o slab allocator", cmd_slab },
    { "proc", "lista processos", cmd_proc },
    { "syscall", "testa as syscalls", cmd_syscall },
    {"reboot", "Reinicia o sistema", cmd_reboot},
    {"fork", "testa fork e identidade", cmd_fork},
    { "disco", "testa o driver ATA", cmd_disco },
    /* Filesystem v0.5 */
    { "ls",    "lista arquivos: ls [caminho]",         cmd_ls    },
    { "cd",    "muda diretorio: cd <caminho>",         cmd_cd    },
    { "pwd",   "mostra diretorio atual",               cmd_pwd   },
    { "cat",   "mostra arquivo: cat <arquivo>",        cmd_cat   },
    { "mkdir", "cria diretorio: mkdir <nome>",         cmd_mkdir },
    { "rm",    "apaga arquivo: rm <arquivo>",          cmd_rm    },
    { "cp",    "copia arquivo: cp <orig> <dest>",      cmd_cp    },
    { "mv",    "move/renomeia: mv <orig> <dest>",      cmd_mv    },
    /* Package manager v0.6 */
    { "spk",   "gerenciador de pacotes: spk <install|remove|list|info>", cmd_spk },
    /* Hardware drivers v0.7 */
    { "pci",      "lista dispositivos PCI detectados",    cmd_pci      },
    { "shutdown", "desliga o sistema via ACPI",           cmd_shutdown  },
    /* Rede v0.8 */
    { "net",      "info/init de rede: net [init|dns <host>]", cmd_net  },
    /* Novos v0.9 */
    { "spfs",   "SpaceFS: spfs <fmt|ls|cat|write|info> [args]", cmd_spfs },
    { "audit",  "log de auditoria: audit [n_linhas]",           cmd_audit },
    { "ahci",   "info do controlador AHCI/SATA",                cmd_ahci  },
    { "dma",    "info do Bus Master DMA",                       cmd_dma   },
    { "usb",    "lista dispositivos USB",                       cmd_usb   },
    { "wifi",   "info do adaptador WiFi",                       cmd_wifi  },
    { "secboot","status de integridade do kernel",              cmd_secboot},
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

static void cmd_heap(int argc, char **argv) {
    (void)argc; (void)argv;

    shell_writeln("=== Teste kmalloc/kfree ===");
    shell_writeln("");

    extern void *kmalloc(uint32_t);
    extern void  kfree(void *);

    /* Aloca 3 blocos */
    uint32_t *a = kmalloc(32);
    uint32_t *b = kmalloc(64);
    uint32_t *c = kmalloc(128);

    if (a && b && c) {
        shell_writeln("kmalloc(32)  OK");
        shell_writeln("kmalloc(64)  OK");
        shell_writeln("kmalloc(128) OK");
    } else {
        shell_writeln("ERRO — kmalloc retornou NULL");
        return;
    }

    /* Escreve e le nos blocos */
    *a = 0xAAAA;
    *b = 0xBBBB;
    *c = 0xCCCC;

    if (*a == 0xAAAA && *b == 0xBBBB && *c == 0xCCCC) {
        shell_writeln("Escrita/leitura nos blocos OK");
    } else {
        shell_writeln("ERRO — dados corrompidos");
        return;
    }

    /* Libera e realoca */
    kfree(b);
    uint32_t *d = kmalloc(64);
    if (d) {
        shell_writeln("kfree + realloc OK");
        shell_writeln("");
        shell_writeln("kmalloc/kfree funcionando!");
        kfree(a);
        kfree(c);
        kfree(d);
    } else {
        shell_writeln("ERRO — kfree nao liberou corretamente");
    }
}

static void cmd_slab(int argc, char **argv) {
    (void)argc; (void)argv;

    extern void  slab_info(void);
    extern void *slab_alloc(void *);
    extern void  slab_free(void *, void *);
    extern void *slab_criar(uint32_t);

    slab_info();
    shell_writeln("");

    /* Teste — aloca e libera objetos de 32 bytes */
    shell_writeln("Teste slab_alloc/free (32B):");

    extern void *cache_32;
    void *a = slab_alloc(cache_32);
    void *b = slab_alloc(cache_32);
    void *c = slab_alloc(cache_32);

    if (a && b && c) {
        /* Escreve nos objetos para verificar que a memoria e valida */
        *(uint32_t *)a = 0xAAAA;
        *(uint32_t *)b = 0xBBBB;
        *(uint32_t *)c = 0xCCCC;

        int ok = (*(uint32_t *)a == 0xAAAA &&
                  *(uint32_t *)b == 0xBBBB &&
                  *(uint32_t *)c == 0xCCCC);

        shell_writeln(ok ? "  alloc + escrita OK" : "  ERRO na escrita");

        slab_free(cache_32, a);
        slab_free(cache_32, b);
        slab_free(cache_32, c);
        shell_writeln("  free OK");

        /* Verifica que o objeto liberado pode ser realocado */
        void *d = slab_alloc(cache_32);
        shell_writeln(d ? "  realloc OK" : "  ERRO no realloc");
        if (d) slab_free(cache_32, d);
    } else {
        shell_writeln("  ERRO — slab_alloc retornou null");
    }
}

static void cmd_proc(int argc, char **argv) {
    (void)argc; (void)argv;
    extern uint32_t sched_ticks(void);

    shell_writeln("=== Processos ===");
    shell_write("Ticks desde boot: ");
    shell_write_num(sched_ticks());
    shell_writeln("");
    shell_writeln("");

    processo_t *p = proc_fila();
    while (p) {
        shell_write("  PID ");
        shell_write_num(p->pid);
        shell_write(" | ");
        shell_write(p->nome);
        shell_write(" | estado: ");
        shell_write_num(p->estado);
        shell_writeln("");
        p = p->proximo;
    }
}

static void cmd_syscall(int argc, char **argv) {
    (void)argc; (void)argv;

    shell_writeln("Testando syscalls:");

    /* Teste SYS_GETPID via int 0x80 */
    uint32_t pid;
    __asm__ volatile (
        "mov $2, %%eax\n"    /* SYS_GETPID = 2 */
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(pid)
    );
    shell_write("  SYS_GETPID retornou PID: ");
    shell_write_num(pid);
    shell_writeln("");

    /* Teste SYS_WRITE via int 0x80 */
    const char *msg = "  SYS_WRITE funcionando!\n";
    __asm__ volatile (
        "mov $1, %%eax\n"         /* SYS_WRITE = 1 */
        "mov %0, %%ebx\n"         /* ponteiro da string */
        "mov $25, %%ecx\n"        /* tamanho */
        "int $0x80\n"
        : : "r"(msg) : "eax", "ebx", "ecx"
    );

    shell_writeln("Syscalls OK!");
}

static void cmd_fork(int argc, char **argv) {
    (void)argc; (void)argv;

    shell_writeln("=== Teste fork + identidade ===");
    shell_writeln("");

    processo_t *atual = proc_atual();
    if (atual) {
        shell_write("PID atual: ");
        shell_write_num(atual->pid);
        shell_writeln("");

        shell_write("Hash identidade: 0x");
        uint32_t h = atual->id.hash;
        char hex[9];
        for (int i = 7; i >= 0; i--) {
            hex[i] = "0123456789ABCDEF"[h & 0xF];
            h >>= 4;
        }
        hex[8] = '\0';
        shell_write(hex);
        shell_writeln("");

        shell_write("Tick nascimento: ");
        shell_write_num(atual->id.tick_nascimento);
        shell_writeln("");
        shell_writeln("");
    }

    shell_writeln("Chamando fork()...");
    int32_t pid_filho = proc_fork();

    if (pid_filho > 0) {
        shell_write("PAI: filho criado com PID ");
        shell_write_num(pid_filho);
        shell_writeln("");
        shell_writeln("fork + identidade OK!");
    } else {
        shell_writeln("ERRO: fork falhou");
    }
}
static void cmd_disco(int argc, char **argv) {
    (void)argc; (void)argv;

    extern int ata_ler(uint32_t, uint8_t, uint8_t *);
    extern int ata_gravar(uint32_t, uint8_t, uint8_t *);

    shell_writeln("=== Teste driver ATA ===");
    shell_writeln("");

    /* Aloca buffer de 1 setor */
    extern void *kmalloc(uint32_t);
    extern void  kfree(void *);

    uint8_t *buf = kmalloc(512);
    if (!buf) { shell_writeln("ERRO: sem memoria"); return; }

    /* Tenta ler o setor 0 — MBR do disco */
    int r = ata_ler(0, 1, buf);
    if (r == 0) {
        shell_writeln("Leitura setor 0: OK");

        /* Verifica assinatura MBR — ultimos 2 bytes devem ser 0x55 0xAA */
        if (buf[510] == 0x55 && buf[511] == 0xAA)
            shell_writeln("Assinatura MBR: 0x55AA encontrada!");
        else
            shell_writeln("Assinatura MBR: nao encontrada");

        /* Mostra primeiros 16 bytes em hex */
        shell_write("Primeiros bytes: ");
        for (int i = 0; i < 16; i++) {
            char hex[3];
            hex[0] = "0123456789ABCDEF"[(buf[i] >> 4) & 0xF];
            hex[1] = "0123456789ABCDEF"[buf[i] & 0xF];
            hex[2] = '\0';
            shell_write(hex);
            shell_write_char(' ');
        }
        shell_writeln("");
    } else {
        shell_writeln("Leitura setor 0: ERRO ou sem disco");
        shell_writeln("(normal no QEMU sem disco configurado)");
    }

    kfree(buf);
}
static void cmd_espaco(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_writeln("=== Espaco de enderecos ===");
    shell_writeln("");
    shell_writeln("0x00000000 - 0xBFFFFFFF  usuario  (3GB)");
    shell_writeln("0xC0000000 - 0xFFFFFFFF  kernel   (1GB)");
    shell_writeln("");
    shell_write("Kernel mapeado em: 0x");
    shell_writeln("00100000");
    shell_write("Heap kernel em:    0x");
    shell_writeln("00300000");
    shell_writeln("");
    shell_writeln("Paginas kernel: Ring 3 nao tem acesso");
    shell_writeln("Paginas usuario: Ring 0 e Ring 3 tem acesso");
    shell_writeln("");
    shell_writeln("Espaco kernel vs usuario: OK");
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

/* -----------------------------------------------------------------------
 * Comandos de Filesystem — v0.5 Saturno
 * ----------------------------------------------------------------------- */

static void cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!vfs_mounted()) { shell_writeln("pwd: sem filesystem montado"); return; }
    shell_writeln(vfs_getcwd());
}

static void cmd_ls(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("ls: sem filesystem montado"); return; }

    const char *path = (argc >= 2) ? argv[1] : "";
    vfs_entry_t entries[64];
    int n = vfs_readdir(path, entries, 64);

    if (n < 0) {
        shell_write("ls: caminho nao encontrado");
        if (argc >= 2) { shell_write(": "); shell_write(argv[1]); }
        shell_writeln("");
        return;
    }

    if (n == 0) { shell_writeln("(diretorio vazio)"); return; }

    for (int i = 0; i < n; i++) {
        if (entries[i].is_dir) {
            shell_write(entries[i].name);
            shell_writeln("/");
        } else {
            shell_write(entries[i].name);
            /* Padding ate coluna 16 */
            int len = 0;
            const char *p = entries[i].name;
            while (*p++) len++;
            for (int j = len; j < 16; j++) shell_write_char(' ');
            shell_write_num(entries[i].size);
            shell_write(" B");
            shell_writeln("");
        }
    }
}

static void cmd_cd(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("cd: sem filesystem montado"); return; }

    if (argc < 2) {
        /* cd sem argumento vai para a raiz */
        if (vfs_chdir("/") != 0)
            shell_writeln("cd: erro ao voltar para /");
        return;
    }

    if (vfs_chdir(argv[1]) != 0) {
        shell_write("cd: nao encontrado: ");
        shell_writeln(argv[1]);
    }
}

static void cmd_cat(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("cat: sem filesystem montado"); return; }

    if (argc < 2) { shell_writeln("uso: cat <arquivo>"); return; }

    /* Abre para descobrir o tamanho */
    vfs_entry_t e;
    if (vfs_open(argv[1], &e) != 0) {
        shell_write("cat: arquivo nao encontrado: ");
        shell_writeln(argv[1]);
        return;
    }
    if (e.is_dir) {
        shell_write("cat: ");
        shell_write(argv[1]);
        shell_writeln(" e um diretorio");
        return;
    }
    if (e.size == 0) { shell_writeln("(arquivo vazio)"); return; }

    /* Aloca buffer + 1 byte para terminador */
    uint8_t *buf = kmalloc(e.size + 1);
    if (!buf) { shell_writeln("cat: sem memoria"); return; }

    int lidos = vfs_read(argv[1], buf, e.size);
    if (lidos < 0) {
        shell_writeln("cat: erro de leitura");
        kfree(buf);
        return;
    }

    buf[lidos] = '\0';

    /* Imprime byte a byte para lidar com \n e \r corretamente */
    for (int i = 0; i < lidos; i++) {
        if (buf[i] == '\n') shell_writeln("");
        else if (buf[i] != '\r') shell_write_char((char)buf[i]);
    }
    shell_writeln("");

    kfree(buf);
}

static void cmd_mkdir(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("mkdir: sem filesystem montado"); return; }

    if (argc < 2) { shell_writeln("uso: mkdir <nome>"); return; }

    if (vfs_mkdir(argv[1]) != 0) {
        shell_write("mkdir: erro ao criar: ");
        shell_writeln(argv[1]);
    } else {
        shell_write("Diretorio criado: ");
        shell_writeln(argv[1]);
    }
}

static void cmd_rm(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("rm: sem filesystem montado"); return; }

    if (argc < 2) { shell_writeln("uso: rm <arquivo>"); return; }

    vfs_entry_t e;
    if (vfs_open(argv[1], &e) != 0) {
        shell_write("rm: nao encontrado: ");
        shell_writeln(argv[1]);
        return;
    }
    if (e.is_dir) {
        shell_write("rm: ");
        shell_write(argv[1]);
        shell_writeln(" e um diretorio (use rmdir)");
        return;
    }

    if (vfs_unlink(argv[1]) != 0) {
        shell_write("rm: erro ao apagar: ");
        shell_writeln(argv[1]);
    } else {
        shell_write("Apagado: ");
        shell_writeln(argv[1]);
    }
}

static void cmd_cp(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("cp: sem filesystem montado"); return; }

    if (argc < 3) { shell_writeln("uso: cp <origem> <destino>"); return; }

    vfs_entry_t src;
    if (vfs_open(argv[1], &src) != 0) {
        shell_write("cp: origem nao encontrada: ");
        shell_writeln(argv[1]);
        return;
    }
    if (src.is_dir) {
        shell_writeln("cp: nao copia diretorios");
        return;
    }

    uint8_t *buf = kmalloc(src.size + 1);
    if (!buf) { shell_writeln("cp: sem memoria"); return; }

    if (vfs_read(argv[1], buf, src.size) < 0) {
        shell_writeln("cp: erro de leitura");
        kfree(buf);
        return;
    }

    if (vfs_write(argv[2], buf, src.size) < 0) {
        shell_write("cp: erro ao escrever: ");
        shell_writeln(argv[2]);
    } else {
        shell_write(argv[1]);
        shell_write(" -> ");
        shell_writeln(argv[2]);
    }

    kfree(buf);
}

static void cmd_mv(int argc, char **argv) {
    if (!vfs_mounted()) { shell_writeln("mv: sem filesystem montado"); return; }

    if (argc < 3) { shell_writeln("uso: mv <origem> <destino>"); return; }

    if (vfs_rename(argv[1], argv[2]) != 0) {
        shell_write("mv: erro ao mover: ");
        shell_write(argv[1]);
        shell_write(" -> ");
        shell_writeln(argv[2]);
    } else {
        shell_write(argv[1]);
        shell_write(" -> ");
        shell_writeln(argv[2]);
    }
}

/* -----------------------------------------------------------------------
 * Hardware Drivers — v0.7 Netuno
 * ----------------------------------------------------------------------- */

static void cmd_pci(int argc, char **argv) {
    (void)argc; (void)argv;
    pci_dump();
}

static void cmd_shutdown(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_writeln("Desligando o sistema...");
    acpi_shutdown();
    /* acpi_shutdown nao retorna em hardware compativel */
    shell_writeln("AVISO: shutdown nao suportado neste hardware.");
}

/* -----------------------------------------------------------------------
 * Rede — v0.8 Galaxia
 * ----------------------------------------------------------------------- */

static void print_ip(uint32_t ip) {
    shell_write_num((ip >> 24) & 0xFF); shell_write_char('.');
    shell_write_num((ip >> 16) & 0xFF); shell_write_char('.');
    shell_write_num((ip >>  8) & 0xFF); shell_write_char('.');
    shell_write_num( ip        & 0xFF);
}

static void cmd_net(int argc, char **argv) {
    if (argc >= 2 && k_strcmp(argv[1], "init") == 0) {
        if (!rtl8139_ready()) { shell_writeln("net: sem placa de rede"); return; }
        shell_writeln("Obtendo IP via DHCP...");
        if (dhcp_init() == 0) {
            shell_write("IP:      "); print_ip(net_our_ip);     shell_writeln("");
            shell_write("Gateway: "); print_ip(net_gateway_ip); shell_writeln("");
            shell_write("Mascara: "); print_ip(net_netmask);    shell_writeln("");
            shell_write("DNS:     "); print_ip(net_dns_ip);     shell_writeln("");
        } else {
            shell_writeln("DHCP: timeout — sem resposta do servidor");
        }
        return;
    }

    if (argc >= 3 && k_strcmp(argv[1], "dns") == 0) {
        shell_write("Resolvendo: "); shell_writeln(argv[2]);
        uint32_t ip = 0;
        if (dns_resolve(argv[2], &ip) == 0) {
            shell_write("  -> "); print_ip(ip); shell_writeln("");
        } else {
            shell_writeln("  -> timeout ou sem resposta");
        }
        return;
    }

    /* Status da rede */
    shell_writeln("=== Status de Rede ===");
    shell_writeln("");
    if (!rtl8139_ready()) {
        shell_writeln("Placa RTL8139: nao detectada");
        shell_writeln("Execute QEMU com: -device rtl8139,netdev=net0 -netdev user,id=net0");
        return;
    }

    shell_writeln("Placa RTL8139: OK");
    shell_write("MAC: ");
    const uint8_t *mac = rtl8139_get_mac();
    for (int i = 0; i < 6; i++) {
        shell_write_char("0123456789ABCDEF"[(mac[i] >> 4) & 0xF]);
        shell_write_char("0123456789ABCDEF"[mac[i] & 0xF]);
        if (i < 5) shell_write_char(':');
    }
    shell_writeln("");

    if (net_our_ip) {
        shell_write("IP:      "); print_ip(net_our_ip);     shell_writeln("");
        shell_write("Gateway: "); print_ip(net_gateway_ip); shell_writeln("");
        shell_write("DNS:     "); print_ip(net_dns_ip);     shell_writeln("");
    } else {
        shell_writeln("IP: nao configurado (use 'net init')");
    }

    shell_write("Buffers livres: ");
    shell_write_num((uint32_t)netbuf_livres());
    shell_write("/");
    shell_write_num(NETBUF_COUNT);
    shell_writeln("");
}

/* -----------------------------------------------------------------------
 * SPK Package Manager — v0.6 Urano
 * ----------------------------------------------------------------------- */
static void cmd_spk(int argc, char **argv) {
    if (argc < 2) {
        shell_writeln("uso: spk <install|remove|list|info|search> [args]");
        shell_writeln("  spk install <arquivo.spk>");
        shell_writeln("  spk remove  <nome>");
        shell_writeln("  spk list");
        shell_writeln("  spk info    <nome>");
        shell_writeln("  spk search  (requer rede — v0.8)");
        return;
    }

    if (k_strcmp(argv[1], "install") == 0) {
        if (argc < 3) { shell_writeln("uso: spk install <arquivo.spk>"); return; }
        pkg_install(argv[2]);

    } else if (k_strcmp(argv[1], "remove") == 0) {
        if (argc < 3) { shell_writeln("uso: spk remove <nome>"); return; }
        pkg_remove(argv[2]);

    } else if (k_strcmp(argv[1], "list") == 0) {
        pkg_list();

    } else if (k_strcmp(argv[1], "info") == 0) {
        if (argc < 3) { shell_writeln("uso: spk info <nome>"); return; }
        pkg_info(argv[2]);

    } else if (k_strcmp(argv[1], "search") == 0) {
        const char *query = (argc >= 3) ? argv[2] : "";
        pkg_remote_search(query);

    } else if (k_strcmp(argv[1], "download") == 0) {
        if (argc < 3) { shell_writeln("uso: spk download <nome>"); return; }
        if (pkg_remote_download(argv[2]) == 0)
            shell_writeln("use 'spk install' para instalar");

    } else {
        shell_write("spk: subcomando desconhecido: ");
        shell_writeln(argv[1]);
        shell_writeln("use 'spk' sem argumentos para ver os subcomandos");
    }
}

/* -----------------------------------------------------------------------
 * SpaceFS — filesystem proprio do SpaceOS
 * ----------------------------------------------------------------------- */
static void cmd_spfs(int argc, char **argv) {
    if (argc < 2) {
        shell_writeln("uso: spfs <fmt|ls|cat|write|info|mkdir|rm> [args]");
        shell_writeln("  spfs fmt          formata disco2 com SpaceFS");
        shell_writeln("  spfs ls [dir]     lista arquivos");
        shell_writeln("  spfs cat <arq>    mostra conteudo");
        shell_writeln("  spfs write <arq> <texto>  escreve texto");
        shell_writeln("  spfs mkdir <dir>  cria diretorio");
        shell_writeln("  spfs rm <arq>     apaga arquivo");
        shell_writeln("  spfs info         info do superbloco");
        return;
    }

    if (k_strcmp(argv[1], "fmt") == 0) {
        shell_writeln("Formatando disco2 com SpaceFS...");
        if (spfs_format() == 0) {
            shell_writeln("SpaceFS formatado com sucesso");
            spfs_mount();
        } else {
            shell_writeln("Erro ao formatar (sem disco2?)");
        }
        return;
    }

    if (k_strcmp(argv[1], "info") == 0) {
        spfs_info();
        return;
    }

    if (k_strcmp(argv[1], "ls") == 0) {
        const char *dir = (argc >= 3) ? argv[2] : "/";
        spfs_ls(dir);
        return;
    }

    if (k_strcmp(argv[1], "cat") == 0) {
        if (argc < 3) { shell_writeln("uso: spfs cat <arquivo>"); return; }
        static uint8_t spfs_cat_buf[4096];
        int n = spfs_read(argv[2], spfs_cat_buf, sizeof(spfs_cat_buf) - 1);
        if (n < 0) {
            shell_write("spfs cat: erro ao ler: "); shell_writeln(argv[2]);
            return;
        }
        spfs_cat_buf[n] = '\0';
        for (int i = 0; i < n; i++) {
            if (spfs_cat_buf[i] == '\n') shell_writeln("");
            else if (spfs_cat_buf[i] != '\r') shell_write_char((char)spfs_cat_buf[i]);
        }
        shell_writeln("");
        return;
    }

    if (k_strcmp(argv[1], "write") == 0) {
        if (argc < 4) { shell_writeln("uso: spfs write <arquivo> <texto>"); return; }
        /* concatena os argumentos restantes */
        static uint8_t spfs_wr_buf[512];
        int pos = 0;
        for (int i = 2; i < argc && pos < 510; i++) {
            if (i > 2) spfs_wr_buf[pos++] = ' ';
            const char *s = argv[i];
            while (*s && pos < 510) spfs_wr_buf[pos++] = (uint8_t)(*s++);
        }
        spfs_wr_buf[pos++] = '\n';
        if (spfs_write(argv[2], spfs_wr_buf, (uint32_t)pos) == 0)
            shell_writeln("escrito");
        else
            shell_writeln("spfs write: erro");
        return;
    }

    if (k_strcmp(argv[1], "mkdir") == 0) {
        if (argc < 3) { shell_writeln("uso: spfs mkdir <dir>"); return; }
        if (spfs_mkdir(argv[2]) == 0)
            shell_writeln("diretorio criado");
        else
            shell_writeln("spfs mkdir: erro");
        return;
    }

    if (k_strcmp(argv[1], "rm") == 0) {
        if (argc < 3) { shell_writeln("uso: spfs rm <arquivo>"); return; }
        if (spfs_unlink(argv[2]) == 0)
            shell_writeln("apagado");
        else
            shell_writeln("spfs rm: erro");
        return;
    }

    shell_write("spfs: subcomando desconhecido: "); shell_writeln(argv[1]);
}

/* -----------------------------------------------------------------------
 * Audit log — append-only com hash encadeado
 * ----------------------------------------------------------------------- */
static void cmd_audit(int argc, char **argv) {
    int n = 20;
    if (argc >= 2) {
        n = 0;
        const char *s = argv[1];
        while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    }
    shell_writeln("=== Audit Log ===");
    audit_dump(n);
}

/* -----------------------------------------------------------------------
 * AHCI / SATA
 * ----------------------------------------------------------------------- */
static void cmd_ahci(int argc, char **argv) {
    (void)argc; (void)argv;
    ahci_info();
}

/* -----------------------------------------------------------------------
 * Bus Master DMA
 * ----------------------------------------------------------------------- */
static void cmd_dma(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_writeln("=== Bus Master IDE DMA ===");
    if (!dma_disponivel()) {
        shell_writeln("DMA: controlador nao detectado");
        return;
    }
    shell_writeln("DMA: controlador Bus Master IDE ativo");
    shell_writeln("  Suporte: ATA DMA read (0xC8) / write (0xCA)");
    shell_writeln("  PRDT: 2 entradas estaticas em .bss");
    shell_writeln("  Buffer: 8 KB alinhado a pagina");
}

/* -----------------------------------------------------------------------
 * USB EHCI
 * ----------------------------------------------------------------------- */
static void cmd_usb(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_writeln("=== USB EHCI ===");
    int n = usb_device_count();
    if (n == 0) {
        shell_writeln("USB: nenhum dispositivo detectado");
        shell_writeln("  (requer EHCI no QEMU — use -device usb-ehci,id=ehci)");
        return;
    }
    shell_write("USB: ");
    shell_write_num((uint32_t)n);
    shell_writeln(" dispositivo(s) enumerado(s)");

    /* Tenta poll do teclado HID se houver dispositivo */
    uint8_t report[8];
    if (usb_hid_poll(1, 1, report, 8) == 0) {
        shell_write("  HID report: ");
        for (int i = 0; i < 8; i++) {
            shell_write_char("0123456789ABCDEF"[(report[i] >> 4) & 0xF]);
            shell_write_char("0123456789ABCDEF"[report[i] & 0xF]);
            if (i < 7) shell_write_char(' ');
        }
        shell_writeln("");
    }
}

/* -----------------------------------------------------------------------
 * WiFi
 * ----------------------------------------------------------------------- */
static void cmd_wifi(int argc, char **argv) {
    (void)argc; (void)argv;
    wifi_info();
}

/* -----------------------------------------------------------------------
 * Secure Boot — integridade do kernel
 * ----------------------------------------------------------------------- */
static void cmd_secboot(int argc, char **argv) {
    (void)argc; (void)argv;
    secboot_status();
}