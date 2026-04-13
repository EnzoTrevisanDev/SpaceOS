#include "kstring.h"

void *k_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *k_memset(void *dst, int c, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

int k_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int k_strncmp(const char *a, const char *b, uint32_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

uint32_t k_strlen(const char *s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

void k_strncpy(char *dst, const char *src, uint32_t n) {
    while (n && *src) { *dst++ = *src++; n--; }
    while (n--) *dst++ = '\0';
}

char k_toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}
