#include "pkg_solver.h"
#include "spk.h"
#include "pkg_db.h"
#include "../fs/vfs.h"
#include "../shell/shell.h"
#include "../disk/kstring.h"

/* Buffer estatico para leitura do header */
static uint8_t hdr_buf[sizeof(spk_header_t)];

/*
 * Le apenas o header de um .spk (sem carregar o payload inteiro).
 * Retorna 0 em sucesso, -1 em erro.
 */
static int ler_header(const char *spk_path, spk_header_t *hdr) {
    int n = vfs_read(spk_path, hdr_buf, sizeof(spk_header_t));
    if (n < (int)sizeof(spk_header_t)) return -1;
    k_memcpy(hdr, hdr_buf, sizeof(spk_header_t));
    if (hdr->magic != SPK_MAGIC) return -1;
    return 0;
}

int pkg_solve_deps(const char *spk_path, char install_order[][64], int max) {
    spk_header_t hdr;
    if (ler_header(spk_path, &hdr) != 0) {
        shell_writeln("solver: falha ao ler header do pacote");
        return -1;
    }

    int order_n = 0;

    /* Processa cada dependencia */
    for (uint32_t i = 0; i < hdr.num_deps && i < SPK_MAX_DEPS; i++) {
        const char *dep = hdr.deps[i];
        if (dep[0] == '\0') continue;

        /* Dep ja instalada? */
        if (pkg_is_installed(dep)) continue;

        /* Dep disponivel em /pkg/incoming/? */
        char dep_path[32];
        k_strncpy(dep_path, "/pkg/incoming/", 15);
        k_strncpy(dep_path + 14, dep, 8);  /* max 8 chars FAT32 */
        int base = (int)k_strlen(dep_path);
        k_strncpy(dep_path + base, ".spk", 5);

        vfs_entry_t e;
        if (vfs_open(dep_path, &e) != 0) {
            shell_write("solver: dependencia nao encontrada: ");
            shell_writeln(dep);
            return -1;
        }

        if (order_n >= max) {
            shell_writeln("solver: lista de instalacao cheia");
            return -1;
        }

        /* Adiciona dep antes do pacote principal */
        k_strncpy(install_order[order_n], dep_path, 64);
        order_n++;
    }

    /* Adiciona o pacote principal por ultimo */
    if (order_n >= max) {
        shell_writeln("solver: lista de instalacao cheia");
        return -1;
    }
    k_strncpy(install_order[order_n], spk_path, 64);
    order_n++;

    return order_n;
}
