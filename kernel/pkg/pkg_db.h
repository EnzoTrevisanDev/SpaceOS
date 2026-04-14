#ifndef PKG_DB_H
#define PKG_DB_H

#include <stdint.h>

/*
 * Banco de dados de pacotes instalados.
 *
 * Formato de /pkg/db (texto, uma linha por pacote):
 *   "nome versao\n"
 *   ex: "hello 1.0.0\n"
 *
 * Lista de arquivos de cada pacote em /pkg/nome.lst (uma linha por arquivo):
 *   "/bin/hello\n"
 */

/* Retorna 1 se o pacote esta instalado, 0 se nao */
int pkg_is_installed(const char *name);

/* Lista todos os pacotes instalados na shell */
void pkg_list(void);

/* Imprime info de um pacote especifico */
int pkg_info(const char *name);

/* Adiciona pacote ao banco (nome + versao) */
int pkg_db_add(const char *name, const char *version);

/* Remove pacote do banco */
int pkg_db_remove(const char *name);

/* Salva lista de arquivos instalados do pacote */
int pkg_files_save(const char *name, char paths[][64], int n);

/* Carrega lista de arquivos instalados. Retorna quantidade. */
int pkg_files_load(const char *name, char paths[][64], int max);

#endif
