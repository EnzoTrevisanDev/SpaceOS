#include "audit.h"
#include "vfs.h"
#include "../disk/kstring.h"
#include "../proc/sched.h"
#include "../shell/shell.h"

#define AUDIT_PATH "/var/log/audit.log"

static uint32_t last_hash = 5381;

/* djb2 hash com seed */
static uint32_t djb2(const uint8_t *data, uint32_t len, uint32_t seed) {
    uint32_t h = seed;
    for (uint32_t i = 0; i < len; i++)
        h = ((h << 5) + h) + data[i];
    return h;
}

static void u32_to_hex(uint32_t v, char *out) {
    static const char *hx = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) { out[i] = hx[v & 0xF]; v >>= 4; }
    out[8] = '\0';
}

/* Copia ate max-1 chars, terminando com \0 */
static int append_str(char *buf, int pos, const char *s, int max) {
    while (*s && pos < max - 1) buf[pos++] = *s++;
    return pos;
}

void audit_log(const char *event, const char *details) {
    char entry[256];
    char hex[9];
    int pos = 0;

    uint32_t ticks = sched_ticks();

    pos = append_str(entry, pos, "T=", 256);
    u32_to_hex(ticks, hex);
    pos = append_str(entry, pos, hex, 256);

    pos = append_str(entry, pos, " E=", 256);
    pos = append_str(entry, pos, event, 256);

    pos = append_str(entry, pos, " D=", 256);
    /* Trunca details a 64 chars */
    int dl = 0;
    while (details[dl] && dl < 64) { entry[pos++] = details[dl++]; }

    pos = append_str(entry, pos, " H=", 256);
    last_hash = djb2((const uint8_t *)entry, (uint32_t)pos, last_hash);
    u32_to_hex(last_hash, hex);
    pos = append_str(entry, pos, hex, 256);

    if (pos < 255) entry[pos++] = '\n';
    entry[pos] = '\0';

    vfs_append(AUDIT_PATH, (const uint8_t *)entry, (uint32_t)pos);
}

void audit_init(void) {
    if (!vfs_mounted()) return;
    vfs_mkdir("/var");
    vfs_mkdir("/var/log");
    audit_log("BOOT", "SpaceOS kernel started");
}

void audit_dump(int n_lines) {
    static uint8_t buf[4096];
    int n = vfs_read(AUDIT_PATH, buf, sizeof(buf) - 1);
    if (n <= 0) { shell_writeln("(log vazio)"); return; }
    buf[n] = '\0';

    /* Conta linhas totais */
    int total = 0;
    for (int i = 0; i < n; i++) if (buf[i] == '\n') total++;

    /* Encontra ponto de inicio para as ultimas n_lines */
    int skip = total - n_lines;
    if (skip < 0) skip = 0;
    int seen = 0;
    int start = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\n') {
            seen++;
            if (seen == skip) { start = i + 1; break; }
        }
    }

    /* Imprime a partir de start */
    for (int i = start; i < n; i++) {
        if (buf[i] == '\n') shell_writeln("");
        else shell_write_char((char)buf[i]);
    }
}
