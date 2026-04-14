#ifndef AUDIT_H
#define AUDIT_H

/*
 * audit — log imutavel append-only com hash encadeado (djb2).
 *
 * Cada entrada: "T=XXXXXXXX E=event D=details H=hash\n"
 * Hash encadeia a entrada anterior: MITM detectavel por verificacao do log.
 *
 * Arquivo: /var/log/audit.log (max 16KB via vfs_append)
 */

void audit_init(void);
void audit_log(const char *event, const char *details);

/* Imprime as ultimas N linhas do log na shell */
void audit_dump(int n_lines);

#endif
