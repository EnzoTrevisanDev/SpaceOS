#include "pkg_db.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"
#include "../disk/kstring.h"

#define PKG_DB_PATH  "/pkg/db"
#define PKG_DB_MAX   2048  /* max 32 pacotes * ~60 bytes por linha */

/* Buffer estatico — evita alocacao grande na stack */
static uint8_t db_buf[PKG_DB_MAX];
static uint8_t lst_buf[1024];   /* para arquivos .lst */

/* ---- Helpers internos -------------------------------------------- */

/*
 * Le o DB inteiro em db_buf. Retorna tamanho ou 0 se nao existe.
 */
static int db_ler(void) {
    vfs_entry_t e;
    if (vfs_open(PKG_DB_PATH, &e) != 0) return 0;
    int n = vfs_read(PKG_DB_PATH, db_buf, PKG_DB_MAX - 1);
    if (n < 0) n = 0;
    db_buf[n] = '\0';
    return n;
}

/*
 * Extrai o primeiro token (palavra) de uma linha apontada por p.
 * Copia ate max-1 bytes em out. Retorna ponteiro para o char apos o token.
 */
static const char *extrair_token(const char *p, char *out, int max) {
    int i = 0;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r' && i < max - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return p;
}

/* ---- API publica -------------------------------------------------- */

int pkg_is_installed(const char *name) {
    int n = db_ler();
    if (n == 0) return 0;

    const char *p = (const char *)db_buf;
    while (*p) {
        char nome[33];
        const char *next = extrair_token(p, nome, 33);
        if (k_strcmp(nome, name) == 0) return 1;
        /* avanca ate proximo \n */
        while (*next && *next != '\n') next++;
        if (*next == '\n') next++;
        p = next;
    }
    return 0;
}

void pkg_list(void) {
    int n = db_ler();
    if (n == 0) {
        shell_writeln("Nenhum pacote instalado.");
        return;
    }

    shell_writeln("Pacotes instalados:");
    shell_writeln("");

    const char *p = (const char *)db_buf;
    int count = 0;
    while (*p) {
        char nome[33], versao[17];
        const char *q = extrair_token(p, nome, 33);
        while (*q == ' ') q++;
        q = extrair_token(q, versao, 17);

        shell_write("  ");
        shell_write(nome);
        /* padding ate coluna 12 */
        int len = (int)k_strlen(nome);
        for (int i = len; i < 10; i++) shell_write_char(' ');
        shell_write(versao);
        shell_writeln("");
        count++;

        while (*q && *q != '\n') q++;
        if (*q == '\n') q++;
        p = q;
    }
    shell_writeln("");
    shell_write("Total: ");
    shell_write_num((uint32_t)count);
    shell_writeln(" pacote(s)");
}

int pkg_info(const char *name) {
    int n = db_ler();
    if (n == 0) {
        shell_write(name);
        shell_writeln(": nao instalado");
        return -1;
    }

    const char *p = (const char *)db_buf;
    while (*p) {
        char nome[33], versao[17];
        const char *q = extrair_token(p, nome, 33);
        while (*q == ' ') q++;
        q = extrair_token(q, versao, 17);

        if (k_strcmp(nome, name) == 0) {
            shell_write("Nome:    "); shell_writeln(nome);
            shell_write("Versao:  "); shell_writeln(versao);

            /* lista arquivos instalados */
            char paths[8][64];
            int nf = pkg_files_load(name, paths, 8);
            if (nf > 0) {
                shell_writeln("Arquivos:");
                for (int i = 0; i < nf; i++) {
                    shell_write("  "); shell_writeln(paths[i]);
                }
            }
            return 0;
        }

        while (*q && *q != '\n') q++;
        if (*q == '\n') q++;
        p = q;
    }

    shell_write(name);
    shell_writeln(": nao instalado");
    return -1;
}

int pkg_db_add(const char *name, const char *version) {
    /* Le DB atual */
    int n = db_ler();

    /* Constroi nova linha: "nome versao\n" */
    char linha[64];
    k_strncpy(linha, name, 32);
    int pos = (int)k_strlen(linha);
    linha[pos++] = ' ';
    k_strncpy(linha + pos, version, 16);
    pos += (int)k_strlen(linha + pos);
    linha[pos++] = '\n';
    linha[pos] = '\0';

    /* Concatena ao buffer existente */
    uint32_t nova_linha_len = (uint32_t)k_strlen(linha);
    if ((uint32_t)n + nova_linha_len >= PKG_DB_MAX) {
        shell_writeln("pkg_db: banco cheio");
        return -1;
    }

    k_memcpy(db_buf + n, linha, nova_linha_len + 1);
    n += (int)nova_linha_len;

    return vfs_write(PKG_DB_PATH, db_buf, (uint32_t)n);
}

int pkg_db_remove(const char *name) {
    int n = db_ler();
    if (n == 0) return -1;

    /* Reconstroi DB sem a linha do pacote */
    static uint8_t novo_buf[PKG_DB_MAX];
    int novo_n = 0;

    const char *p = (const char *)db_buf;
    while (*p) {
        /* encontra inicio e fim da linha */
        const char *linha_start = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        uint32_t linha_len = (uint32_t)(p - linha_start);

        /* extrai nome da linha */
        char nome[33];
        extrair_token(linha_start, nome, 33);

        /* copia linha apenas se nao for o pacote a remover */
        if (k_strcmp(nome, name) != 0) {
            k_memcpy(novo_buf + novo_n, linha_start, linha_len);
            novo_n += (int)linha_len;
        }
    }
    novo_buf[novo_n] = '\0';

    if (novo_n == 0) {
        /* banco ficou vazio — apaga o arquivo */
        vfs_unlink(PKG_DB_PATH);
        return 0;
    }
    return vfs_write(PKG_DB_PATH, novo_buf, (uint32_t)novo_n);
}

int pkg_files_save(const char *name, char paths[][64], int n) {
    /* Monta caminho: /pkg/nome.lst */
    char lst_path[32];
    k_strncpy(lst_path, "/pkg/", 6);
    k_strncpy(lst_path + 5, name, 8);  /* max 8 chars p/ FAT32 */
    int base_len = (int)k_strlen(lst_path);
    k_strncpy(lst_path + base_len, ".lst", 5);

    /* Constroi conteudo */
    int pos = 0;
    for (int i = 0; i < n; i++) {
        uint32_t plen = k_strlen(paths[i]);
        if (pos + (int)plen + 1 >= 1023) break;
        k_memcpy(lst_buf + pos, paths[i], plen);
        pos += (int)plen;
        lst_buf[pos++] = '\n';
    }
    lst_buf[pos] = '\0';

    return vfs_write(lst_path, lst_buf, (uint32_t)pos);
}

int pkg_files_load(const char *name, char paths[][64], int max) {
    char lst_path[32];
    k_strncpy(lst_path, "/pkg/", 6);
    k_strncpy(lst_path + 5, name, 8);
    int base_len = (int)k_strlen(lst_path);
    k_strncpy(lst_path + base_len, ".lst", 5);

    int n = vfs_read(lst_path, lst_buf, 1023);
    if (n <= 0) return 0;
    lst_buf[n] = '\0';

    int count = 0;
    const char *p = (const char *)lst_buf;
    while (*p && count < max) {
        int i = 0;
        while (*p && *p != '\n' && i < 63)
            paths[count][i++] = *p++;
        paths[count][i] = '\0';
        if (*p == '\n') p++;
        if (i > 0) count++;
    }
    return count;
}
