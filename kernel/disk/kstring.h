#ifndef KSTRING_H
#define KSTRING_H

#include <stdint.h>

void    *k_memcpy(void *dst, const void *src, uint32_t n);
void    *k_memset(void *dst, int c, uint32_t n);
int      k_strcmp(const char *a, const char *b);
int      k_strncmp(const char *a, const char *b, uint32_t n);
uint32_t k_strlen(const char *s);
void     k_strncpy(char *dst, const char *src, uint32_t n);

/* Converte char para maiusculo (A-Z) */
char k_toupper(char c);

#endif
