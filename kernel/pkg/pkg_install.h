#ifndef PKG_INSTALL_H
#define PKG_INSTALL_H

/*
 * pkg_init — cria estrutura de diretorios /pkg/ se nao existir.
 * Deve ser chamado durante kernel_main apos vfs_init().
 */
void pkg_init(void);

/*
 * pkg_install — instala um pacote a partir de um arquivo .spk.
 *   - Verifica magic e checksum
 *   - Resolve e instala dependencias primeiro
 *   - Extrai arquivos para seus install_path
 *   - Registra no banco de dados
 *   - Em caso de falha: rollback (apaga arquivos ja escritos)
 *
 * Retorna 0 em sucesso, -1 em falha.
 */
int pkg_install(const char *spk_path);

/*
 * pkg_remove — desinstala um pacote pelo nome.
 *   - Remove todos os arquivos listados no manifesto
 *   - Remove o manifesto
 *   - Remove do banco de dados
 *
 * Retorna 0 em sucesso, -1 em falha.
 */
int pkg_remove(const char *name);

#endif
