#ifndef PKG_SOLVER_H
#define PKG_SOLVER_H

/*
 * pkg_solve_deps — resolve dependencias de um pacote .spk.
 *
 * Para cada dependencia declarada no header:
 *   1. Se ja esta instalada → OK, nao precisa instalar
 *   2. Se existe em /pkg/incoming/depname.spk → adiciona na ordem antes
 *   3. Caso contrario → erro (dep nao encontrada)
 *
 * install_order[][64]: paths dos .spk a instalar em ordem (dep primeiro)
 * O proprio pacote spk_path e sempre o ultimo da lista.
 *
 * Retorna: numero de pacotes na ordem (>= 1), ou -1 em erro.
 */
int pkg_solve_deps(const char *spk_path, char install_order[][64], int max);

#endif
