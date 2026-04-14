#ifndef PKG_CHECKSUM_H
#define PKG_CHECKSUM_H

#include <stdint.h>

/*
 * pkg_checksum — hash djb2 sobre os bytes do payload.
 * Usado para verificar integridade do pacote.
 * Futuro: substituir por HMAC-SHA256 para seguranca real.
 */
uint32_t pkg_checksum(const uint8_t *data, uint32_t len);

#endif
