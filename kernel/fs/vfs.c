#include "vfs.h"
#include "fat32.h"
#include "../disk/kstring.h"
#include "../mem/kheap.h"

/* -----------------------------------------------------------------------
 * Estado interno do VFS
 * ----------------------------------------------------------------------- */
static struct {
    int      montado;
    char     path[256];   /* caminho atual, ex: "/docs/src" */
    uint32_t cluster;     /* cluster do diretorio atual */
} cwd;

/* -----------------------------------------------------------------------
 * Utilitarios de caminho
 * ----------------------------------------------------------------------- */

/*
 * path_split_last — separa o ultimo componente de um caminho.
 * Ex: "/docs/src/main.c" → parent="/docs/src", last="main.c"
 * Modifica parent_buf (copia do path). last aponta dentro de path.
 */
static void path_split_last(const char *path, char *parent_buf, const char **last) {
    uint32_t len = k_strlen(path);
    k_strncpy(parent_buf, path, 255);
    parent_buf[255] = '\0';

    /* Encontra a ultima barra */
    int slash = -1;
    for (int i = (int)len - 1; i >= 0; i--) {
        if (path[i] == '/') { slash = i; break; }
    }

    if (slash <= 0) {
        /* Sem barra, ou barra so na raiz */
        parent_buf[0] = '\0';       /* pai = cwd */
        *last = path;
    } else {
        parent_buf[slash] = '\0';
        *last = path + slash + 1;
    }
}

/*
 * resolve_path — converte um caminho em (cluster, nome11 da entrada final).
 * Se path referencia um diretorio, cluster_out = cluster do diretorio.
 * Se path referencia um arquivo, cluster_out = cluster do PAI e name11_out preenchido.
 *
 * Modo dir=1: retorna cluster do diretorio referenciado.
 * Modo dir=0: retorna cluster do pai + name11 da entrada.
 */
static int resolve_dir(const char *path, uint32_t *cluster_out) {
    uint32_t cur;

    if (path[0] == '/') {
        cur = fat32_root_cluster();
        path++;  /* pula a barra inicial */
    } else {
        cur = cwd.cluster;
    }

    /* Copia para buffer mutavel */
    char buf[256];
    k_strncpy(buf, path, 255);
    buf[255] = '\0';

    /* Itera sobre cada componente separado por '/' */
    char *tok = buf;
    while (*tok) {
        /* Encontra proximo separador */
        char *sep = tok;
        while (*sep && *sep != '/') sep++;
        int has_more = (*sep == '/');
        *sep = '\0';

        if (k_strlen(tok) == 0 || (tok[0] == '.' && tok[1] == '\0')) {
            /* "." — fica no mesmo diretorio */
        } else {
            /* Busca o componente no diretorio atual */
            char name11[12];
            fat32_normalize(tok, name11);

            fat32_entry_t e;
            if (fat32_find(cur, name11, &e) != 0) return -1;
            if (!e.is_dir) {
                /* So aceita diretorio no meio do caminho */
                if (has_more) return -1;
                /* Ultimo componente pode ser arquivo — retorna cluster do pai */
                *cluster_out = cur;
                return 1;  /* 1 = ultimo componente e arquivo */
            }
            cur = e.first_cluster;
        }

        tok = has_more ? sep + 1 : sep;
    }

    *cluster_out = cur;
    return 0;  /* 0 = cluster e o proprio diretorio */
}

/* -----------------------------------------------------------------------
 * vfs_init
 * ----------------------------------------------------------------------- */
void vfs_init(void) {
    cwd.montado = 0;
    k_strncpy(cwd.path, "/", 255);
    cwd.cluster = 2;

    if (fat32_init() == 0) {
        cwd.montado  = 1;
        cwd.cluster  = fat32_root_cluster();
    }
}

int vfs_mounted(void) {
    return cwd.montado;
}

const char *vfs_getcwd(void) {
    return cwd.path;
}

/* -----------------------------------------------------------------------
 * vfs_open — resolve path para uma entrada
 * ----------------------------------------------------------------------- */
int vfs_open(const char *path, vfs_entry_t *out) {
    if (!cwd.montado) return -1;

    /* Separa pai e nome do ultimo componente */
    char parent_buf[256];
    const char *last;
    path_split_last(path, parent_buf, &last);

    /* Resolve o diretorio pai */
    uint32_t parent_cluster;
    if (k_strlen(parent_buf) == 0) {
        /* Sem prefixo de caminho — usa cwd */
        parent_cluster = (path[0] == '/') ? fat32_root_cluster() : cwd.cluster;
    } else {
        if (resolve_dir(parent_buf, &parent_cluster) < 0) return -1;
    }

    /* Busca a entrada dentro do pai */
    char name11[12];
    fat32_normalize(last, name11);

    fat32_entry_t fe;
    if (fat32_find(parent_cluster, name11, &fe) != 0) return -1;

    k_strncpy(out->name, fe.name, 12);
    out->is_dir  = fe.is_dir;
    out->cluster = fe.first_cluster;
    out->size    = fe.size;
    return 0;
}

/* -----------------------------------------------------------------------
 * vfs_readdir
 * ----------------------------------------------------------------------- */
int vfs_readdir(const char *path, vfs_entry_t *entries, int max) {
    if (!cwd.montado) return -1;

    uint32_t dir_cluster;

    if (path == 0 || k_strlen(path) == 0) {
        dir_cluster = cwd.cluster;
    } else {
        if (resolve_dir(path, &dir_cluster) < 0) return -1;
    }

    /* Converte fat32_entry_t → vfs_entry_t */
    fat32_entry_t fe[64];
    int max_fe = (max < 64) ? max : 64;
    int n = fat32_readdir(dir_cluster, fe, max_fe);
    if (n < 0) return -1;

    for (int i = 0; i < n; i++) {
        k_strncpy(entries[i].name, fe[i].name, 12);
        entries[i].is_dir  = fe[i].is_dir;
        entries[i].cluster = fe[i].first_cluster;
        entries[i].size    = fe[i].size;
    }
    return n;
}

/* -----------------------------------------------------------------------
 * vfs_read
 * ----------------------------------------------------------------------- */
int vfs_read(const char *path, uint8_t *buf, uint32_t max_size) {
    if (!cwd.montado) return -1;

    vfs_entry_t e;
    if (vfs_open(path, &e) != 0) return -1;
    if (e.is_dir) return -1;

    uint32_t sz = (e.size < max_size) ? e.size : max_size;
    return fat32_read(e.cluster, sz, buf);
}

/* -----------------------------------------------------------------------
 * vfs_write — cria ou sobrescreve arquivo
 * ----------------------------------------------------------------------- */
int vfs_write(const char *path, const uint8_t *buf, uint32_t size) {
    if (!cwd.montado) return -1;

    char parent_buf[256];
    const char *last;
    path_split_last(path, parent_buf, &last);

    uint32_t parent_cluster;
    if (k_strlen(parent_buf) == 0) {
        parent_cluster = (path[0] == '/') ? fat32_root_cluster() : cwd.cluster;
    } else {
        if (resolve_dir(parent_buf, &parent_cluster) < 0) return -1;
    }

    char name11[12];
    fat32_normalize(last, name11);

    /* Verifica se arquivo ja existe */
    fat32_entry_t fe;
    if (fat32_find(parent_cluster, name11, &fe) == 0) {
        /* Arquivo existe — apaga e recria (sobrescreve simples) */
        fat32_delete(parent_cluster, name11);
    }

    /* Cria nova entrada */
    uint32_t cluster = fat32_create(parent_cluster, name11, FAT32_ATTR_ARCHIVE);
    if (cluster == 0) return -1;

    return fat32_write_file(cluster, buf, size);
}

/* -----------------------------------------------------------------------
 * vfs_mkdir
 * ----------------------------------------------------------------------- */
int vfs_mkdir(const char *path) {
    if (!cwd.montado) return -1;

    char parent_buf[256];
    const char *last;
    path_split_last(path, parent_buf, &last);

    uint32_t parent_cluster;
    if (k_strlen(parent_buf) == 0) {
        parent_cluster = (path[0] == '/') ? fat32_root_cluster() : cwd.cluster;
    } else {
        if (resolve_dir(parent_buf, &parent_cluster) < 0) return -1;
    }

    char name11[12];
    fat32_normalize(last, name11);

    uint32_t c = fat32_create(parent_cluster, name11, FAT32_ATTR_DIR);
    return (c == 0) ? -1 : 0;
}

/* -----------------------------------------------------------------------
 * vfs_unlink
 * ----------------------------------------------------------------------- */
int vfs_unlink(const char *path) {
    if (!cwd.montado) return -1;

    char parent_buf[256];
    const char *last;
    path_split_last(path, parent_buf, &last);

    uint32_t parent_cluster;
    if (k_strlen(parent_buf) == 0) {
        parent_cluster = (path[0] == '/') ? fat32_root_cluster() : cwd.cluster;
    } else {
        if (resolve_dir(parent_buf, &parent_cluster) < 0) return -1;
    }

    char name11[12];
    fat32_normalize(last, name11);
    return fat32_delete(parent_cluster, name11);
}

/* -----------------------------------------------------------------------
 * vfs_rename — renomeia ou move arquivo (mesmo volume)
 * ----------------------------------------------------------------------- */
int vfs_rename(const char *old_path, const char *new_path) {
    if (!cwd.montado) return -1;

    char old_parent[256], new_parent[256];
    const char *old_last, *new_last;
    path_split_last(old_path, old_parent, &old_last);
    path_split_last(new_path, new_parent, &new_last);

    uint32_t old_cluster, new_cluster;

    if (k_strlen(old_parent) == 0)
        old_cluster = (old_path[0] == '/') ? fat32_root_cluster() : cwd.cluster;
    else if (resolve_dir(old_parent, &old_cluster) < 0) return -1;

    if (k_strlen(new_parent) == 0)
        new_cluster = (new_path[0] == '/') ? fat32_root_cluster() : cwd.cluster;
    else if (resolve_dir(new_parent, &new_cluster) < 0) return -1;

    char old11[12], new11[12];
    fat32_normalize(old_last, old11);
    fat32_normalize(new_last, new11);

    if (old_cluster == new_cluster) {
        /* Mesmo diretorio: renomeia no lugar */
        return fat32_rename(old_cluster, old11, new11);
    } else {
        /* Diretorio diferente: le entrada, cria no destino, apaga na origem */
        fat32_entry_t fe;
        if (fat32_find(old_cluster, old11, &fe) != 0) return -1;

        /* Cria nova entrada no destino com mesmo cluster */
        /* Nota: esta implementacao simplificada nao recria a entrada com o
           cluster existente — em FAT32 completo voce copiaria o dirent.
           Para v0.5 a abordagem correta requer acesso direto ao raw dirent. */
        return -1;  /* mv entre diretorios: implementar em versao futura */
    }
}

/* -----------------------------------------------------------------------
 * vfs_chdir
 * ----------------------------------------------------------------------- */
int vfs_chdir(const char *path) {
    if (!cwd.montado) return -1;

    uint32_t new_cluster;
    char new_path[256];

    if (path[0] == '/') {
        /* Caminho absoluto */
        if (resolve_dir(path, &new_cluster) < 0) return -1;
        k_strncpy(new_path, path, 255);
    } else if (k_strcmp(path, "..") == 0) {
        /* Sobe um nivel */
        fat32_entry_t dotdot;
        char name11[12];
        fat32_normalize("..", name11);
        /* ".." tem nome ".." no diretorio atual */
        k_memset(name11, ' ', 11);
        name11[0] = '.';
        name11[1] = '.';
        name11[11] = '\0';

        if (fat32_find(cwd.cluster, name11, &dotdot) != 0) return -1;
        new_cluster = dotdot.first_cluster;
        if (new_cluster == 0) new_cluster = fat32_root_cluster();

        /* Remove ultimo componente do path */
        k_strncpy(new_path, cwd.path, 255);
        uint32_t len = k_strlen(new_path);
        if (len > 1) {
            /* Remove tudo ate a ultima barra */
            for (int i = (int)len - 1; i > 0; i--) {
                if (new_path[i] == '/') { new_path[i] = '\0'; break; }
            }
            if (k_strlen(new_path) == 0) {
                new_path[0] = '/';
                new_path[1] = '\0';
            }
        }
    } else {
        /* Caminho relativo — busca no cwd */
        char name11[12];
        fat32_normalize(path, name11);

        fat32_entry_t fe;
        if (fat32_find(cwd.cluster, name11, &fe) != 0) return -1;
        if (!fe.is_dir) return -1;

        new_cluster = fe.first_cluster;

        /* Constroi novo path */
        k_strncpy(new_path, cwd.path, 255);
        uint32_t len = k_strlen(new_path);
        if (new_path[len - 1] != '/') {
            new_path[len] = '/';
            new_path[len + 1] = '\0';
        }
        /* Concatena o nome do diretorio */
        uint32_t nlen = k_strlen(new_path);
        k_strncpy(new_path + nlen, path, 255 - nlen);
    }

    cwd.cluster = new_cluster;
    k_strncpy(cwd.path, new_path, 255);
    cwd.path[255] = '\0';
    return 0;
}
