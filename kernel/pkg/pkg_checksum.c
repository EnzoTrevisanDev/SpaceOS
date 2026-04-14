#include "pkg_checksum.h"

/*
 * djb2 hash — simples e rapido para verificacao de integridade.
 * Equivalente a: hash = hash * 33 + byte  (para cada byte)
 */
uint32_t pkg_checksum(const uint8_t *data, uint32_t len) {
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < len; i++)
        hash = ((hash << 5) + hash) + data[i];
    return hash;
}
