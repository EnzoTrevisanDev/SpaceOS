#include "tls.h"
#include "tcp.h"
#include "../disk/kstring.h"

/* ================================================================== */
/* SHA-256                                                            */
/* ================================================================== */

#define ROR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(e,f,g)  (((e)&(f))^(~(e)&(g)))
#define MAJ(a,b,c) (((a)&(b))^((a)&(c))^((b)&(c)))
#define EP0(x) (ROR32(x,2)^ROR32(x,13)^ROR32(x,22))
#define EP1(x) (ROR32(x,6)^ROR32(x,11)^ROR32(x,25))
#define SIG0(x) (ROR32(x,7)^ROR32(x,18)^((x)>>3))
#define SIG1(x) (ROR32(x,17)^ROR32(x,19)^((x)>>10))

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *block) {
    uint32_t w[64], a,b,c,d,e,f,g,h,t1,t2;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)
              |((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for (int i = 16; i < 64; i++)
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + sha256_k[i] + w[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

void sha256_init(sha256_ctx_t *ctx) {
    ctx->count = 0;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
}

void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    uint32_t i = (uint32_t)(ctx->count & 63);
    ctx->count += len;
    for (uint32_t j = 0; j < len; j++) {
        ctx->buf[i++] = data[j];
        if (i == 64) { sha256_transform(ctx, ctx->buf); i = 0; }
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t digest[32]) {
    uint32_t i = (uint32_t)(ctx->count & 63);
    ctx->buf[i++] = 0x80;
    if (i > 56) { while (i < 64) ctx->buf[i++] = 0; sha256_transform(ctx, ctx->buf); i = 0; }
    while (i < 56) ctx->buf[i++] = 0;
    uint64_t bits = ctx->count * 8;
    for (int k = 7; k >= 0; k--) { ctx->buf[56+k] = (uint8_t)(bits & 0xFF); bits >>= 8; }
    /* fix: se count*8 > 2^32, ignoramos; ok para mensagens < 512MB */
    /* Store bit count big-endian */
    uint64_t bc = ctx->count * 8;
    for (int k = 0; k < 8; k++) ctx->buf[56+k] = (uint8_t)(bc >> (56 - k*8));
    sha256_transform(ctx, ctx->buf);
    for (int k = 0; k < 8; k++) {
        digest[k*4]   = (uint8_t)(ctx->state[k] >> 24);
        digest[k*4+1] = (uint8_t)(ctx->state[k] >> 16);
        digest[k*4+2] = (uint8_t)(ctx->state[k] >> 8);
        digest[k*4+3] = (uint8_t)(ctx->state[k]);
    }
}

void sha256(const uint8_t *data, uint32_t len, uint8_t digest[32]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

/* ================================================================== */
/* HMAC-SHA256                                                        */
/* ================================================================== */

void hmac_sha256(const uint8_t *key, uint32_t klen,
                 const uint8_t *msg, uint32_t mlen,
                 uint8_t mac[32]) {
    uint8_t k0[64], ipad[64], opad[64], inner[32];
    k_memset(k0, 0, 64);
    if (klen > 64) sha256(key, klen, k0);
    else k_memcpy(k0, key, klen);
    for (int i = 0; i < 64; i++) { ipad[i] = k0[i] ^ 0x36; opad[i] = k0[i] ^ 0x5C; }
    sha256_ctx_t ctx;
    sha256_init(&ctx); sha256_update(&ctx, ipad, 64); sha256_update(&ctx, msg, mlen);
    sha256_final(&ctx, inner);
    sha256_init(&ctx); sha256_update(&ctx, opad, 64); sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, mac);
}

/* ================================================================== */
/* ChaCha20                                                           */
/* ================================================================== */

#define ROTL32(v,n) (((v)<<(n))|((v)>>(32-(n))))
#define QR(a,b,c,d) \
    a+=b; d^=a; d=ROTL32(d,16); \
    c+=d; b^=c; b=ROTL32(b,12); \
    a+=b; d^=a; d=ROTL32(d,8);  \
    c+=d; b^=c; b=ROTL32(b,7);

void chacha20_block(uint32_t out[16], const uint32_t in[16]) {
    uint32_t x[16];
    k_memcpy(x, in, 64);
    for (int i = 0; i < 10; i++) {
        QR(x[0],x[4],x[8], x[12]); QR(x[1],x[5],x[9], x[13]);
        QR(x[2],x[6],x[10],x[14]); QR(x[3],x[7],x[11],x[15]);
        QR(x[0],x[5],x[10],x[15]); QR(x[1],x[6],x[11],x[12]);
        QR(x[2],x[7],x[8], x[13]); QR(x[3],x[4],x[9], x[14]);
    }
    for (int i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

void chacha20_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                      uint32_t counter,
                      const uint8_t *in, uint8_t *out, uint32_t len) {
    uint32_t state[16], keystream[16];
    static const uint8_t sigma[] = "expand 32-byte k";
    k_memcpy(&state[0], sigma, 16);
    k_memcpy(&state[4], key,   32);
    state[12] = counter;
    k_memcpy(&state[13], nonce, 12);

    for (uint32_t i = 0; i < len; ) {
        chacha20_block(keystream, state);
        state[12]++;
        uint8_t *ks = (uint8_t *)keystream;
        for (int j = 0; j < 64 && i < len; j++, i++)
            out[i] = in[i] ^ ks[j];
    }
}

/* ================================================================== */
/* Poly1305                                                           */
/* ================================================================== */

/* Aritmetica em 130 bits usando 5 limbs de 26 bits */
void poly1305_mac(const uint8_t key[32], const uint8_t *msg, uint32_t len,
                  uint8_t tag[16]) {
    /* r: primeiros 16 bytes da chave, clamped */
    uint32_t r0,r1,r2,r3,r4, s0,s1,s2,s3;
    uint8_t rb[16];
    k_memcpy(rb, key, 16);
    rb[3]  &= 15; rb[7]  &= 15; rb[11] &= 15; rb[15] &= 15;
    rb[4]  &= 252; rb[8]  &= 252; rb[12] &= 252;

    /* Carrega r em 5 limbs de 26 bits */
    r0 = ( (uint32_t)rb[0]|(uint32_t)rb[1]<<8|(uint32_t)rb[2]<<16|(uint32_t)rb[3]<<24) & 0x3FFFFFF;
    r1 = (((uint32_t)rb[3]>>2)|(uint32_t)rb[4]<<6|(uint32_t)rb[5]<<14|(uint32_t)rb[6]<<22|(uint32_t)rb[7]<<30) & 0x3FFFFFF;
    r2 = (((uint32_t)rb[6]>>4)|(uint32_t)rb[7]<<4|(uint32_t)rb[8]<<12|(uint32_t)rb[9]<<20|(uint32_t)rb[10]<<28) & 0x3FFFFFF;
    r3 = (((uint32_t)rb[9]>>6)|(uint32_t)rb[10]<<2|(uint32_t)rb[11]<<10|(uint32_t)rb[12]<<18|(uint32_t)rb[13]<<26) & 0x3FFFFFF;
    r4 = (((uint32_t)rb[12]>>8)|(uint32_t)rb[13]<<0|(uint32_t)rb[14]<<8|(uint32_t)rb[15]<<16) & 0x3FFFFFF;

    s0=r1*5; s1=r2*5; s2=r3*5; s3=r4*5;

    /* Acumulador */
    uint32_t h0=0,h1=0,h2=0,h3=0,h4=0;

    while (len > 0) {
        uint32_t chunk = (len >= 16) ? 16 : len;
        uint8_t blk[17];
        k_memset(blk, 0, 17);
        k_memcpy(blk, msg, chunk);
        blk[chunk] = 1;  /* pad bit */
        msg += chunk; len -= chunk;

        uint32_t t0 = (uint32_t)blk[0]|(uint32_t)blk[1]<<8|(uint32_t)blk[2]<<16|(uint32_t)blk[3]<<24;
        h0 += t0 & 0x3FFFFFF;
        h1 += (((uint32_t)blk[3]>>2)|(uint32_t)blk[4]<<6|(uint32_t)blk[5]<<14|(uint32_t)blk[6]<<22) & 0x3FFFFFF;
        h2 += (((uint32_t)blk[6]>>4)|(uint32_t)blk[7]<<4|(uint32_t)blk[8]<<12|(uint32_t)blk[9]<<20) & 0x3FFFFFF;
        h3 += (((uint32_t)blk[9]>>6)|(uint32_t)blk[10]<<2|(uint32_t)blk[11]<<10|(uint32_t)blk[12]<<18) & 0x3FFFFFF;
        h4 += ((uint32_t)blk[12]>>8)|(uint32_t)blk[13]|(uint32_t)blk[14]<<8|(uint32_t)blk[15]<<16|(uint32_t)blk[16]<<24;

        /* Multiplicacao mod (2^130-5) */
        uint64_t d0,d1,d2,d3,d4;
        d0=(uint64_t)h0*r0+(uint64_t)h1*s3+(uint64_t)h2*s2+(uint64_t)h3*s1+(uint64_t)h4*s0;
        d1=(uint64_t)h0*r1+(uint64_t)h1*r0+(uint64_t)h2*s3+(uint64_t)h3*s2+(uint64_t)h4*s1;
        d2=(uint64_t)h0*r2+(uint64_t)h1*r1+(uint64_t)h2*r0+(uint64_t)h3*s3+(uint64_t)h4*s2;
        d3=(uint64_t)h0*r3+(uint64_t)h1*r2+(uint64_t)h2*r1+(uint64_t)h3*r0+(uint64_t)h4*s3;
        d4=(uint64_t)h0*r4+(uint64_t)h1*r3+(uint64_t)h2*r2+(uint64_t)h3*r1+(uint64_t)h4*r0;

        h0=(uint32_t)(d0>>0)&0x3FFFFFF; uint32_t c=(uint32_t)(d0>>26);
        d1+=c; h1=(uint32_t)(d1>>0)&0x3FFFFFF; c=(uint32_t)(d1>>26);
        d2+=c; h2=(uint32_t)(d2>>0)&0x3FFFFFF; c=(uint32_t)(d2>>26);
        d3+=c; h3=(uint32_t)(d3>>0)&0x3FFFFFF; c=(uint32_t)(d3>>26);
        d4+=c; h4=(uint32_t)(d4>>0)&0x3FFFFFF; c=(uint32_t)(d4>>26);
        h0 += c*5; c = h0>>26; h0 &= 0x3FFFFFF; h1 += c;
    }

    /* Reducao final mod (2^130-5) */
    uint32_t c = h1>>26; h1 &= 0x3FFFFFF; h2 += c;
    c = h2>>26; h2 &= 0x3FFFFFF; h3 += c;
    c = h3>>26; h3 &= 0x3FFFFFF; h4 += c;
    c = h4>>26; h4 &= 0x3FFFFFF; h0 += c*5;
    c = h0>>26; h0 &= 0x3FFFFFF; h1 += c;

    /* Computa h - (2^130-5) para ver se precisa reduzir */
    uint32_t g0,g1,g2,g3,g4;
    g0 = h0+5; c=g0>>26; g0 &= 0x3FFFFFF;
    g1 = h1+c; c=g1>>26; g1 &= 0x3FFFFFF;
    g2 = h2+c; c=g2>>26; g2 &= 0x3FFFFFF;
    g3 = h3+c; c=g3>>26; g3 &= 0x3FFFFFF;
    g4 = h4+c - (1u<<26);
    uint32_t mask = (g4 >> 31) - 1;  /* 0xFFFFFFFF se g4 < 2^25 */
    h0 = (h0 & ~mask) | (g0 & mask);
    h1 = (h1 & ~mask) | (g1 & mask);
    h2 = (h2 & ~mask) | (g2 & mask);
    h3 = (h3 & ~mask) | (g3 & mask);
    h4 = (h4 & ~mask) | (g4 & mask);

    /* Soma s (ultimos 16 bytes da chave) */
    uint32_t f0,f1,f2,f3;
    const uint8_t *sk = key + 16;
    f0=(uint32_t)sk[0]|(uint32_t)sk[1]<<8|(uint32_t)sk[2]<<16|(uint32_t)sk[3]<<24;
    f1=(uint32_t)sk[4]|(uint32_t)sk[5]<<8|(uint32_t)sk[6]<<16|(uint32_t)sk[7]<<24;
    f2=(uint32_t)sk[8]|(uint32_t)sk[9]<<8|(uint32_t)sk[10]<<16|(uint32_t)sk[11]<<24;
    f3=(uint32_t)sk[12]|(uint32_t)sk[13]<<8|(uint32_t)sk[14]<<16|(uint32_t)sk[15]<<24;

    uint64_t F;
    F=(uint64_t)((h0|(h1<<26))&0xFFFFFFFF)+f0; tag[0]=(uint8_t)F; tag[1]=(uint8_t)(F>>8); tag[2]=(uint8_t)(F>>16); tag[3]=(uint8_t)(F>>24); F>>=32;
    F+=(uint64_t)((h1>>6|(h2<<20))&0xFFFFFFFF)+f1; tag[4]=(uint8_t)F; tag[5]=(uint8_t)(F>>8); tag[6]=(uint8_t)(F>>16); tag[7]=(uint8_t)(F>>24); F>>=32;
    F+=(uint64_t)((h2>>12|(h3<<14))&0xFFFFFFFF)+f2; tag[8]=(uint8_t)F; tag[9]=(uint8_t)(F>>8); tag[10]=(uint8_t)(F>>16); tag[11]=(uint8_t)(F>>24); F>>=32;
    F+=(uint64_t)((h3>>18|(h4<<8))&0xFFFFFFFF)+f3; tag[12]=(uint8_t)F; tag[13]=(uint8_t)(F>>8); tag[14]=(uint8_t)(F>>16); tag[15]=(uint8_t)(F>>24);
}

/* ================================================================== */
/* ChaCha20-Poly1305 AEAD                                            */
/* ================================================================== */

int chacha20poly1305_encrypt(
    const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t *aad, uint32_t aad_len,
    const uint8_t *plain, uint32_t plain_len,
    uint8_t *cipher_out, uint8_t tag_out[16]) {

    /* Gera OTK (one-time key para Poly1305): bloco ChaCha20 com counter=0 */
    uint8_t otk[32];
    uint8_t zero32[32]; k_memset(zero32, 0, 32);
    chacha20_encrypt(key, nonce, 0, zero32, otk, 32);

    /* Cifra os dados com counter=1 */
    chacha20_encrypt(key, nonce, 1, plain, cipher_out, plain_len);

    /* Computa MAC sobre AAD + ciphertext */
    static uint8_t mac_input[TLS_MAX_RECORD + 64];
    uint32_t mi = 0;
    k_memcpy(mac_input + mi, aad, aad_len); mi += aad_len;
    /* Pad AAD a multiplo de 16 */
    while (mi & 15) mac_input[mi++] = 0;
    k_memcpy(mac_input + mi, cipher_out, plain_len); mi += plain_len;
    while (mi & 15) mac_input[mi++] = 0;
    /* Lengths (little-endian 64-bit) */
    for (int i = 0; i < 8; i++) mac_input[mi++] = (uint8_t)(aad_len >> (i*8));
    for (int i = 0; i < 8; i++) mac_input[mi++] = (uint8_t)(plain_len >> (i*8));

    poly1305_mac(otk, mac_input, mi, tag_out);
    return 0;
}

int chacha20poly1305_decrypt(
    const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t *aad, uint32_t aad_len,
    const uint8_t *cipher, uint32_t cipher_len,
    const uint8_t tag[16], uint8_t *plain_out) {

    uint8_t otk[32];
    uint8_t zero32[32]; k_memset(zero32, 0, 32);
    chacha20_encrypt(key, nonce, 0, zero32, otk, 32);

    /* Verifica MAC */
    static uint8_t mac_input[TLS_MAX_RECORD + 64];
    uint32_t mi = 0;
    k_memcpy(mac_input + mi, aad, aad_len); mi += aad_len;
    while (mi & 15) mac_input[mi++] = 0;
    k_memcpy(mac_input + mi, cipher, cipher_len); mi += cipher_len;
    while (mi & 15) mac_input[mi++] = 0;
    for (int i = 0; i < 8; i++) mac_input[mi++] = (uint8_t)(aad_len >> (i*8));
    for (int i = 0; i < 8; i++) mac_input[mi++] = (uint8_t)(cipher_len >> (i*8));

    uint8_t expected_tag[16];
    poly1305_mac(otk, mac_input, mi, expected_tag);

    /* Comparacao em tempo constante */
    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= expected_tag[i] ^ tag[i];
    if (diff) return -1;  /* autenticacao falhou */

    chacha20_encrypt(key, nonce, 1, cipher, plain_out, cipher_len);
    return 0;
}

/* ================================================================== */
/* SpaceTLS — canal seguro PSK sobre TCP                             */
/* ================================================================== */

#define STLS_MAGIC 0x534C5300u   /* "SLS\0" */
#define STLS_VER   1

typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;      /* 1=handshake, 2=data, 3=alert */
    uint16_t length;    /* payload length */
} __attribute__((packed)) stls_record_hdr_t;

#define STLS_MAX_SOCKS 4
static struct {
    int      tcp_sock;
    uint8_t  send_key[32];
    uint8_t  recv_key[32];
    uint8_t  send_nonce[12];
    uint8_t  recv_nonce[12];
    uint32_t send_seq;
    uint32_t recv_seq;
    int      ready;
} tls_socks[STLS_MAX_SOCKS];

static void nonce_inc(uint8_t nonce[12], uint32_t seq) {
    /* XOR sequencia nos ultimos 4 bytes do nonce (little-endian) */
    nonce[8]  ^= (uint8_t)(seq);
    nonce[9]  ^= (uint8_t)(seq >> 8);
    nonce[10] ^= (uint8_t)(seq >> 16);
    nonce[11] ^= (uint8_t)(seq >> 24);
}

int tls_connect(int tcp_sock, const uint8_t *psk, uint8_t psk_len) {
    /* Encontra slot livre */
    int slot = -1;
    for (int i = 0; i < STLS_MAX_SOCKS; i++) {
        if (!tls_socks[i].ready) { slot = i; break; }
    }
    if (slot < 0) return -1;

    tls_socks[slot].tcp_sock  = tcp_sock;
    tls_socks[slot].send_seq  = 0;
    tls_socks[slot].recv_seq  = 0;
    tls_socks[slot].ready     = 0;

    /* Derива chaves via HMAC-SHA256(psk, "spacetls_send"/"spacetls_recv") */
    static const uint8_t lbl_send[] = "spacetls_send_key";
    static const uint8_t lbl_recv[] = "spacetls_recv_key";
    hmac_sha256(psk, psk_len, lbl_send, 17, tls_socks[slot].send_key);
    hmac_sha256(psk, psk_len, lbl_recv, 17, tls_socks[slot].recv_key);

    /* Nonce inicial: derivado via HMAC(psk, "spacetls_nonce") */
    uint8_t nonce_material[32];
    static const uint8_t lbl_nonce[] = "spacetls_nonce";
    hmac_sha256(psk, psk_len, lbl_nonce, 14, nonce_material);
    k_memcpy(tls_socks[slot].send_nonce, nonce_material,     12);
    k_memcpy(tls_socks[slot].recv_nonce, nonce_material + 12, 12);

    /* Handshake: envia magic + version */
    static uint8_t hs_pkt[8];
    stls_record_hdr_t *hdr = (stls_record_hdr_t *)hs_pkt;
    hdr->magic   = STLS_MAGIC;
    hdr->version = STLS_VER;
    hdr->type    = 1;
    hdr->length  = 0;
    tcp_send(tcp_sock, hs_pkt, 8);

    /* Aguarda resposta */
    static uint8_t resp[8];
    if (tcp_recv(tcp_sock, resp, 8, 2000) < 8) return -1;
    stls_record_hdr_t *rhdr = (stls_record_hdr_t *)resp;
    if (rhdr->magic != STLS_MAGIC || rhdr->version != STLS_VER) return -1;

    tls_socks[slot].ready = 1;
    return slot;
}

int tls_send(int sock, const uint8_t *data, uint16_t len) {
    if (sock < 0 || sock >= STLS_MAX_SOCKS || !tls_socks[sock].ready) return -1;

    static uint8_t pkt[TLS_MAX_RECORD + 24];
    stls_record_hdr_t *hdr = (stls_record_hdr_t *)pkt;
    hdr->magic   = STLS_MAGIC;
    hdr->version = STLS_VER;
    hdr->type    = 2;
    hdr->length  = (uint16_t)(len + 16);  /* cipher + tag */

    uint8_t *cipher = pkt + 8;
    uint8_t *tag    = pkt + 8 + len;

    /* Usa nonce XOR seq para garantir unicidade */
    uint8_t nonce[12];
    k_memcpy(nonce, tls_socks[sock].send_nonce, 12);
    nonce_inc(nonce, tls_socks[sock].send_seq++);

    chacha20poly1305_encrypt(tls_socks[sock].send_key, nonce,
                             (uint8_t *)hdr, 8,
                             data, len, cipher, tag);

    return tcp_send(tls_socks[sock].tcp_sock, pkt, (uint16_t)(8 + len + 16));
}

int tls_recv(int sock, uint8_t *buf, uint16_t max, uint32_t timeout_ms) {
    if (sock < 0 || sock >= STLS_MAX_SOCKS || !tls_socks[sock].ready) return -1;

    static uint8_t pkt[TLS_MAX_RECORD + 24];
    int n = tcp_recv(tls_socks[sock].tcp_sock, pkt, (uint16_t)sizeof(pkt), timeout_ms);
    if (n < 8) return -1;

    stls_record_hdr_t *hdr = (stls_record_hdr_t *)pkt;
    if (hdr->magic != STLS_MAGIC || hdr->type != 2) return -1;

    uint16_t payload_len = (uint16_t)(hdr->length - 16);
    uint8_t *cipher = pkt + 8;
    uint8_t *tag    = pkt + 8 + payload_len;
    if (payload_len > max) payload_len = max;

    uint8_t nonce[12];
    k_memcpy(nonce, tls_socks[sock].recv_nonce, 12);
    nonce_inc(nonce, tls_socks[sock].recv_seq++);

    if (chacha20poly1305_decrypt(tls_socks[sock].recv_key, nonce,
                                 (uint8_t *)hdr, 8,
                                 cipher, payload_len, tag, buf) != 0)
        return -1;

    return (int)payload_len;
}

void tls_close(int sock) {
    if (sock < 0 || sock >= STLS_MAX_SOCKS) return;
    tcp_close(tls_socks[sock].tcp_sock);
    tls_socks[sock].ready = 0;
}

int tls_ready(int sock) {
    return (sock >= 0 && sock < STLS_MAX_SOCKS && tls_socks[sock].ready);
}
