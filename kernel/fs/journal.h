#ifndef JOURNAL_H
#define JOURNAL_H

/*
 * journal — write-ahead log para SpaceFS.
 *
 * Antes de qualquer escrita em bloco de dados:
 *   1. journal_begin(block_num, new_data)  — grava entrada no journal
 *   2. Realiza escrita real no bloco
 *   3. journal_commit(tx_id)               — marca transacao como completa
 *
 * Na montagem, journal_recover() replays entradas nao commitadas.
 *
 * Layout no disco (setores 35-98):
 *   Setor 35:     Journal meta (head, tail, next_txid)
 *   Setores 36-97: 31 entradas x 2 setores (meta + dados)
 */

#include <stdint.h>

void     journal_init(void);
void     journal_recover(void);
uint32_t journal_begin(uint32_t block_num, const uint8_t *new_data);
void     journal_commit(uint32_t tx_id);

#endif
