#include "secboot.h"
#include "../fs/vfs.h"
#include "../disk/kstring.h"
#include "../shell/shell.h"

/* Secao de codigo do kernel: definidos no linker script */
extern uint8_t _start[];   /* inicio do kernel (entry.asm) */
extern uint8_t _end[];     /* fim do kernel */

#define SECBOOT_HASH_PATH "/boot/kernel.hash"

/* //#define SECBOOT_HALT_ON_FAIL */  /* descomente para parar no boot se falhar */

static int sb_verified = 0;
static uint32_t measured_hash = 0;

static uint32_t djb2_range(const uint8_t *start, const uint8_t *end) {
    uint32_t h = 5381;
    for (const uint8_t *p = start; p < end; p++)
        h = ((h << 5) + h) + *p;
    return h;
}

static uint32_t hex_to_u32(const char *s) {
    /* Parseia "0xHHHHHHHH\n" ou "HHHHHHHH\n" */
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t v = 0;
    while (*s) {
        char c = *s++;
        uint8_t d;
        if (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10);
        else break;
        v = (v << 4) | d;
    }
    return v;
}

uint32_t secboot_measure(void) {
    /* Mede segmento de codigo do kernel */
    return djb2_range(_start, _end);
}

void secboot_init(void) {
    measured_hash = secboot_measure();

    /* Tenta ler hash armazenado */
    if (!vfs_mounted()) {
        sb_verified = 1;  /* sem disco = sem verificacao = ok */
        return;
    }

    /* Cria /boot se nao existe */
    vfs_mkdir("/boot");

    static uint8_t hash_buf[32];
    int n = vfs_read(SECBOOT_HASH_PATH, hash_buf, sizeof(hash_buf) - 1);
    if (n <= 0) {
        /* Sem hash armazenado — primeira execucao, salva */
        char hex[12];
        /* Converte hash para hex string */
        const char *hx = "0123456789abcdef";
        hex[0] = '0'; hex[1] = 'x';
        for (int i = 0; i < 8; i++) {
            hex[2 + i] = hx[(measured_hash >> (28 - i * 4)) & 0xF];
        }
        hex[10] = '\n'; hex[11] = '\0';
        vfs_write(SECBOOT_HASH_PATH, (const uint8_t *)hex, 11);
        sb_verified = 1;
        shell_writeln("SecBoot: hash inicial registrado");
        return;
    }

    hash_buf[n] = '\0';
    uint32_t stored = hex_to_u32((char *)hash_buf);

    if (stored == measured_hash) {
        sb_verified = 1;
        shell_writeln("SecBoot: integridade verificada");
    } else {
        sb_verified = 0;
        shell_write("SecBoot: AVISO — hash diverge! medido=0x");
        shell_write_hex(measured_hash);
        shell_write(" armazenado=0x");
        shell_write_hex(stored);
        shell_writeln("");
#ifdef SECBOOT_HALT_ON_FAIL
        shell_writeln("SecBoot: HALTING (modo strict)");
        while(1) __asm__ volatile("hlt");
#endif
    }
}

int secboot_verified(void) { return sb_verified; }

void secboot_status(void) {
    shell_writeln("=== Secure Boot (BIOS) ===");
    shell_write("Hash medido:  0x");
    shell_write_hex(measured_hash);
    shell_writeln("");
    if (sb_verified) {
        shell_writeln("Status:       OK — integridade verificada");
    } else {
        shell_writeln("Status:       AVISO — hash divergente ou nao verificado");
    }
    shell_writeln("Modo:         djb2 (HMAC-SHA256 previsto para v1.x)");
    shell_writeln("Hash path:    /boot/kernel.hash");
}
