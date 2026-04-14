#include "pkg_remote.h"
#include "pkg_install.h"
#include "../net/tcp.h"
#include "../net/dns.h"
#include "../net/ipv4.h"
#include "../net/net_util.h"
#include "../net/rtl8139.h"
#include "../fs/vfs.h"
#include "../mem/kheap.h"
#include "../shell/shell.h"
#include "../disk/kstring.h"

/* Repositorio padrao: QEMU user-mode gateway */
static char repo_host[32] = "10.0.2.2";
static uint16_t repo_port = 8080;

void pkg_remote_set_repo(const char *host_port) {
    /* Formato esperado: "host:port" ou "host" (porta padrao 8080) */
    int i = 0;
    while (host_port[i] && host_port[i] != ':' && i < 31) {
        repo_host[i] = host_port[i];
        i++;
    }
    repo_host[i] = '\0';
    if (host_port[i] == ':') {
        repo_port = 0;
        i++;
        while (host_port[i] >= '0' && host_port[i] <= '9') {
            repo_port = (uint16_t)(repo_port * 10 + (host_port[i] - '0'));
            i++;
        }
    }
}

/* Resolve host para IP (usa IP direto se for formato "a.b.c.d") */
static uint32_t resolve_host(void) {
    /* Tenta parsear como IP literal */
    uint32_t ip = 0;
    int octets = 0;
    const char *p = repo_host;
    while (*p && octets < 4) {
        uint32_t oct = 0;
        while (*p >= '0' && *p <= '9') { oct = oct * 10 + (*p - '0'); p++; }
        ip = (ip << 8) | (oct & 0xFF);
        octets++;
        if (*p == '.') p++;
    }
    if (octets == 4) return ip;

    /* Resolve via DNS */
    uint32_t resolved = 0;
    if (dns_resolve(repo_host, &resolved) == 0) return resolved;
    return 0;
}

/* Envia GET e le corpo HTTP ate conexao fechar */
static int http_get(int sock, const char *path,
                     uint8_t *out_buf, uint32_t max_size,
                     uint32_t *out_len) {
    /* Monta request */
    static char req[256];
    int pos = 0;
    const char *method = "GET ";
    while (*method) req[pos++] = *method++;
    while (*path) req[pos++] = *path++;
    const char *rest = " HTTP/1.0\r\nHost: ";
    while (*rest) req[pos++] = *rest++;
    const char *h = repo_host;
    while (*h) req[pos++] = *h++;
    const char *end = "\r\n\r\n";
    while (*end) req[pos++] = *end++;
    req[pos] = '\0';

    tcp_send(sock, (const uint8_t *)req, (uint16_t)pos);

    /* Le resposta */
    static uint8_t http_buf[256];
    uint32_t total = 0;
    int header_done = 0;
    uint32_t header_end_pos = 0;

    while (total < max_size) {
        int n = tcp_recv(sock, http_buf, 256, 3000);
        if (n <= 0) break;

        /* Pula header HTTP (procura "\r\n\r\n") */
        if (!header_done) {
            for (int i = 0; i < n - 3; i++) {
                if (http_buf[i]=='\r' && http_buf[i+1]=='\n' &&
                    http_buf[i+2]=='\r' && http_buf[i+3]=='\n') {
                    header_done = 1;
                    header_end_pos = (uint32_t)(i + 4);
                    break;
                }
            }
            if (!header_done) continue;
            /* Copia parte do corpo que veio junto com o header */
            uint32_t body_start = header_end_pos;
            uint32_t body_in_first = (uint32_t)n - body_start;
            if (body_in_first > 0 && total + body_in_first <= max_size) {
                k_memcpy(out_buf + total, http_buf + body_start, body_in_first);
                total += body_in_first;
            }
        } else {
            uint32_t to_copy = (total + (uint32_t)n <= max_size) ?
                               (uint32_t)n : (max_size - total);
            k_memcpy(out_buf + total, http_buf, to_copy);
            total += to_copy;
        }
    }

    *out_len = total;
    return (total > 0) ? 0 : -1;
}

/* ---- API publica ------------------------------------------------- */

int pkg_remote_search(const char *query) {
    if (!rtl8139_ready()) {
        shell_writeln("spk search: sem rede (inicialize com 'net init')");
        return -1;
    }

    uint32_t server_ip = resolve_host();
    if (!server_ip) {
        shell_writeln("spk search: nao foi possivel resolver o repositorio");
        return -1;
    }

    int sock = tcp_connect(server_ip, repo_port);
    if (sock < 0) {
        shell_writeln("spk search: conexao recusada");
        return -1;
    }

    static uint8_t list_buf[4096];
    uint32_t list_len = 0;
    int ret = http_get(sock, "/packages/list", list_buf, sizeof(list_buf), &list_len);
    tcp_close(sock);

    if (ret < 0 || list_len == 0) {
        shell_writeln("spk search: repositorio nao respondeu");
        return -1;
    }

    /* Exibe lista filtrada pela query */
    list_buf[list_len] = '\0';
    shell_writeln("Pacotes disponiveis:");
    shell_writeln("");

    char *p = (char *)list_buf;
    while (*p) {
        char *line = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') *p++ = '\0';

        /* Filtra por query (vazio = mostra tudo) */
        if (!query || !*query) {
            shell_write("  "); shell_writeln(line);
        } else {
            /* busca simples: query em qualquer posicao */
            const char *q = query;
            const char *l = line;
            int found = 0;
            while (*l) {
                if (*l == *q) {
                    const char *ll = l, *qq = q;
                    while (*qq && *ll == *qq) { ll++; qq++; }
                    if (!*qq) { found = 1; break; }
                }
                l++;
            }
            if (found) { shell_write("  "); shell_writeln(line); }
        }
    }
    return 0;
}

int pkg_remote_download(const char *pkg_name) {
    if (!rtl8139_ready()) {
        shell_writeln("spk: sem rede");
        return -1;
    }

    uint32_t server_ip = resolve_host();
    if (!server_ip) return -1;

    int sock = tcp_connect(server_ip, repo_port);
    if (sock < 0) return -1;

    /* Path: /packages/pkgname.spk */
    static char path[64];
    k_strncpy(path, "/packages/", 11);
    k_strncpy(path + 10, pkg_name, 8);
    int base = (int)k_strlen(path);
    k_strncpy(path + base, ".spk", 5);

    /* Baixa para buffer (max 64KB) */
    uint8_t *buf = kmalloc(64 * 1024);
    if (!buf) { tcp_close(sock); return -1; }

    uint32_t downloaded = 0;
    int ret = http_get(sock, path, buf, 64 * 1024, &downloaded);
    tcp_close(sock);

    if (ret < 0 || downloaded == 0) {
        shell_write("spk: download falhou: ");
        shell_writeln(pkg_name);
        kfree(buf);
        return -1;
    }

    /* Salva em /pkg/incoming/nome.spk */
    static char spk_path[32];
    k_strncpy(spk_path, "/pkg/incoming/", 15);
    k_strncpy(spk_path + 14, pkg_name, 8);
    base = (int)k_strlen(spk_path);
    k_strncpy(spk_path + base, ".spk", 5);

    ret = vfs_write(spk_path, buf, downloaded);
    kfree(buf);

    if (ret < 0) {
        shell_writeln("spk: erro ao salvar pacote no disco");
        return -1;
    }

    shell_write("Baixado: "); shell_writeln(spk_path);
    shell_write_num(downloaded); shell_writeln(" bytes");
    return 0;
}

