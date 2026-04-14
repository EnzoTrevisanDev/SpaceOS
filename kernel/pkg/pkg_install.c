#include "pkg_install.h"
#include "pkg_db.h"
#include "pkg_solver.h"
#include "pkg_checksum.h"
#include "spk.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"
#include "../disk/kstring.h"
#include "../mem/kheap.h"

/* ---- Helpers internos --------------------------------------------- */

/*
 * Garante que o diretorio pai de um caminho existe.
 * Ex: "/bin/hello" → garante que "/bin" existe.
 */
static void ensure_parent_dir(const char *path) {
    char parent[65];
    k_strncpy(parent, path, 64);
    parent[64] = '\0';

    int last_slash = -1;
    for (int i = 0; parent[i]; i++) {
        if (parent[i] == '/') last_slash = i;
    }
    if (last_slash <= 0) return;  /* raiz ou sem slash */

    parent[last_slash] = '\0';
    /* vfs_mkdir falha silenciosamente se ja existe */
    vfs_mkdir(parent);
}

/*
 * Instala um unico arquivo .spk (sem resolucao de deps).
 * Retorna 0 em sucesso, -1 em falha.
 */
static int instalar_spk(const char *spk_path) {
    /* --- 1. Abre e valida o pacote ---------------------------------- */
    vfs_entry_t e;
    if (vfs_open(spk_path, &e) != 0) {
        shell_write("spk: arquivo nao encontrado: ");
        shell_writeln(spk_path);
        return -1;
    }
    if (e.size > SPK_MAX_SIZE) {
        shell_writeln("spk: pacote muito grande (max 64KB)");
        return -1;
    }
    if (e.size < (uint32_t)sizeof(spk_header_t)) {
        shell_writeln("spk: arquivo muito pequeno");
        return -1;
    }

    /* --- 2. Le o arquivo inteiro na memoria ------------------------- */
    uint8_t *buf = kmalloc(e.size + 1);
    if (!buf) {
        shell_writeln("spk: sem memoria");
        return -1;
    }
    int lidos = vfs_read(spk_path, buf, e.size);
    if (lidos <= 0 || (uint32_t)lidos < e.size) {
        shell_writeln("spk: erro de leitura");
        kfree(buf);
        return -1;
    }

    /* --- 3. Valida header ------------------------------------------- */
    spk_header_t *hdr = (spk_header_t *)buf;
    if (hdr->magic != SPK_MAGIC) {
        shell_writeln("spk: formato invalido (magic errado)");
        kfree(buf);
        return -1;
    }
    if (hdr->version != SPK_VERSION) {
        shell_writeln("spk: versao do formato incompativel");
        kfree(buf);
        return -1;
    }
    if (hdr->num_files == 0 || hdr->num_files > SPK_MAX_FILES) {
        shell_writeln("spk: numero de arquivos invalido");
        kfree(buf);
        return -1;
    }

    /* --- 4. Verifica checksum --------------------------------------- */
    uint32_t header_total = (uint32_t)sizeof(spk_header_t) +
                            hdr->num_files * (uint32_t)sizeof(spk_file_record_t);
    if (header_total + hdr->total_payload_size > (uint32_t)lidos) {
        shell_writeln("spk: tamanho inconsistente");
        kfree(buf);
        return -1;
    }
    const uint8_t *payload = buf + header_total;
    uint32_t csum = pkg_checksum(payload, hdr->total_payload_size);
    if (csum != hdr->checksum) {
        shell_writeln("spk: checksum invalido — pacote corrompido");
        kfree(buf);
        return -1;
    }

    /* --- 5. Verifica se ja esta instalado --------------------------- */
    if (pkg_is_installed(hdr->name)) {
        shell_write("spk: ");
        shell_write(hdr->name);
        shell_writeln(" ja esta instalado (use 'spk remove' primeiro)");
        kfree(buf);
        return -1;
    }

    /* --- 6. Extrai arquivos ----------------------------------------- */
    spk_file_record_t *records = (spk_file_record_t *)(buf + sizeof(spk_header_t));

    /* Rastreia arquivos instalados para rollback */
    char installed[SPK_MAX_FILES][64];
    int n_installed = 0;

    shell_write("Instalando ");
    shell_write(hdr->name);
    shell_write(" ");
    shell_write(hdr->pkg_version);
    shell_writeln("...");

    for (uint32_t i = 0; i < hdr->num_files; i++) {
        spk_file_record_t *rec = &records[i];

        if (rec->offset + rec->size > hdr->total_payload_size) {
            shell_writeln("spk: offset de arquivo invalido");
            goto rollback;
        }

        const uint8_t *file_data = payload + rec->offset;

        /* Cria diretorio pai se necessario */
        ensure_parent_dir(rec->install_path);

        /* Escreve o arquivo */
        if (vfs_write(rec->install_path, file_data, rec->size) < 0) {
            shell_write("spk: erro ao escrever: ");
            shell_writeln(rec->install_path);
            goto rollback;
        }

        k_strncpy(installed[n_installed], rec->install_path, 64);
        n_installed++;

        shell_write("  -> ");
        shell_writeln(rec->install_path);
    }

    /* --- 7. Registra no banco de dados ------------------------------ */
    if (pkg_files_save(hdr->name, installed, n_installed) < 0) {
        shell_writeln("spk: erro ao salvar manifesto");
        goto rollback;
    }
    if (pkg_db_add(hdr->name, hdr->pkg_version) < 0) {
        shell_writeln("spk: erro ao registrar no banco");
        goto rollback;
    }

    shell_write(hdr->name);
    shell_writeln(": instalado com sucesso!");

    kfree(buf);
    return 0;

rollback:
    shell_writeln("spk: rollback — removendo arquivos parciais...");
    for (int i = 0; i < n_installed; i++) {
        vfs_unlink(installed[i]);
    }
    kfree(buf);
    return -1;
}

/* ---- API publica -------------------------------------------------- */

void pkg_init(void) {
    if (!vfs_mounted()) return;

    vfs_entry_t e;

    /* Cria /pkg se nao existe */
    if (vfs_open("/pkg", &e) != 0) {
        if (vfs_mkdir("/pkg") == 0)
            shell_writeln("Criado /pkg");
    }
    /* Cria /pkg/incoming */
    if (vfs_open("/pkg/incoming", &e) != 0)
        vfs_mkdir("/pkg/incoming");

    /* Cria /usr para instalacao de binarios */
    if (vfs_open("/usr", &e) != 0)
        vfs_mkdir("/usr");
    if (vfs_open("/usr/bin", &e) != 0)
        vfs_mkdir("/usr/bin");
}

int pkg_install(const char *spk_path) {
    if (!vfs_mounted()) {
        shell_writeln("spk: sem filesystem montado");
        return -1;
    }

    /* Resolve dependencias e monta ordem de instalacao */
    char order[9][64];  /* max 8 deps + 1 pacote principal */
    int n = pkg_solve_deps(spk_path, order, 9);
    if (n < 0) return -1;

    /* Instala em ordem (deps primeiro) */
    for (int i = 0; i < n; i++) {
        if (instalar_spk(order[i]) != 0) {
            /* Se uma dep falhou, aborta */
            if (i < n - 1) {
                shell_write("spk: falha ao instalar dependencia: ");
                shell_writeln(order[i]);
            }
            return -1;
        }
    }
    return 0;
}

int pkg_remove(const char *name) {
    if (!vfs_mounted()) {
        shell_writeln("spk: sem filesystem montado");
        return -1;
    }

    if (!pkg_is_installed(name)) {
        shell_write("spk: pacote nao instalado: ");
        shell_writeln(name);
        return -1;
    }

    /* Carrega lista de arquivos */
    char paths[8][64];
    int n = pkg_files_load(name, paths, 8);

    /* Remove cada arquivo */
    for (int i = 0; i < n; i++) {
        if (vfs_unlink(paths[i]) == 0) {
            shell_write("  removido: ");
            shell_writeln(paths[i]);
        }
    }

    /* Remove manifesto /pkg/nome.lst */
    char lst_path[32];
    k_strncpy(lst_path, "/pkg/", 6);
    k_strncpy(lst_path + 5, name, 8);
    int base = (int)k_strlen(lst_path);
    k_strncpy(lst_path + base, ".lst", 5);
    vfs_unlink(lst_path);

    /* Remove do banco de dados */
    pkg_db_remove(name);

    shell_write(name);
    shell_writeln(": removido");
    return 0;
}
