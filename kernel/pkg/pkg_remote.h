#ifndef PKG_REMOTE_H
#define PKG_REMOTE_H

/*
 * pkg_remote — busca e download de pacotes via HTTP/1.0 sobre TCP.
 *
 * Protocolo: HTTP simples (sem TLS — futuro v1.x).
 * Seguranca: checksum djb2 do payload verifica integridade.
 *            Autenticidade real requer TLS (MITM e possivel).
 *
 * Repositorio padrao: configurado em pkg_remote_set_repo().
 * Default QEMU user-mode: http://10.0.2.2:8080/
 */

/*
 * pkg_remote_search — lista pacotes disponiveis no repositorio.
 * Exibe resultado na shell.
 * Retorna 0 em sucesso, -1 em falha de rede.
 */
int pkg_remote_search(const char *query);

/*
 * pkg_remote_download — baixa pacote e salva em /pkg/incoming/nome.spk.
 * Retorna 0 em sucesso, -1 em falha.
 */
int pkg_remote_download(const char *pkg_name);

/* Define URL base do repositorio (ex: "10.0.2.2:8080") */
void pkg_remote_set_repo(const char *host_port);

#endif
