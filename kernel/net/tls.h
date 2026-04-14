#ifndef TLS_H
#define TLS_H

/*
 * tls — criptografia ChaCha20-Poly1305 + camada de registro TLS.
 *
 * Implementa:
 *   - ChaCha20 stream cipher (RFC 7539)
 *   - Poly1305 MAC (RFC 7539)
 *   - ChaCha20-Poly1305 AEAD (RFC 7905)
 *   - SHA-256 (FIPS 180-4)
 *   - HMAC-SHA256
 *
 * Uso para SPK remoto com PSK (pre-shared key):
 *   tls_connect(sock, psk, psk_len)  — handshake simplificado
 *   tls_send(sock, data, len)
 *   tls_recv(sock, buf, max, timeout)
 *   tls_close(sock)
 *
 * Nota: sem verificacao de certificado (MITM possivel sem PKI).
 * TLS completo com X.509 esta previsto para v1.x.
 */

#include <stdint.h>

/* ---- SHA-256 ------------------------------------------------------- */
typedef struct { uint32_t state[8]; uint64_t count; uint8_t buf[64]; } sha256_ctx_t;
void     sha256_init(sha256_ctx_t *ctx);
void     sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void     sha256_final(sha256_ctx_t *ctx, uint8_t digest[32]);
void     sha256(const uint8_t *data, uint32_t len, uint8_t digest[32]);

/* ---- HMAC-SHA256 --------------------------------------------------- */
void hmac_sha256(const uint8_t *key, uint32_t klen,
                 const uint8_t *msg, uint32_t mlen,
                 uint8_t mac[32]);

/* ---- ChaCha20 ------------------------------------------------------ */
void chacha20_block(uint32_t out[16], const uint32_t in[16]);
void chacha20_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                      uint32_t counter,
                      const uint8_t *in, uint8_t *out, uint32_t len);

/* ---- Poly1305 ------------------------------------------------------ */
void poly1305_mac(const uint8_t key[32], const uint8_t *msg, uint32_t len,
                  uint8_t tag[16]);

/* ---- ChaCha20-Poly1305 AEAD ---------------------------------------- */
int chacha20poly1305_encrypt(
    const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t *aad, uint32_t aad_len,
    const uint8_t *plain, uint32_t plain_len,
    uint8_t *cipher_out, uint8_t tag_out[16]);

int chacha20poly1305_decrypt(
    const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t *aad, uint32_t aad_len,
    const uint8_t *cipher, uint32_t cipher_len,
    const uint8_t tag[16], uint8_t *plain_out);

/* ---- SpaceTLS (canal seguro PSK) ---------------------------------- */
#define TLS_MAX_RECORD 4096

int  tls_connect(int tcp_sock, const uint8_t *psk, uint8_t psk_len);
int  tls_send(int sock, const uint8_t *data, uint16_t len);
int  tls_recv(int sock, uint8_t *buf, uint16_t max, uint32_t timeout_ms);
void tls_close(int sock);
int  tls_ready(int sock);

#endif
