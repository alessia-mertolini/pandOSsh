#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

/*
 * sysSupport.h
 * Dichiarazioni pubbliche di sysSupport.c (Phase 3 - Level 4).
 *
 * Competenza: General Exception Handler (Sezione 6) +
 *             SYSCALL Exception Handler (Sezione 7).
 */

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include "../../headers/const.h"
#include "costantiNopoli.h"
#include "../../headers/types.h"

/*
 * generalExceptionHandler  (Sezione 6)
 *
 * Entry point per le eccezioni non-TLB passate su dal Nucleus.
 * Registrato in sup_exceptContext[GENERALEXCEPT].c_pc da initProc.c.
 * Smista verso syscallExceptionHandler o programTrapHandler.
 * Non ritorna mai.
 */
void generalExceptionHandler(void);

/*
 * syscallExceptionHandler  (Sezione 7)
 *
 * Gestisce SYS2, SYS4, SYS5, SYS6.
 * Parametro: Support Structure del processo corrente.
 * Non ritorna mai.
 */
void syscallExceptionHandler(support_t *sup);

/*
 * transmitChar  (usata da SYS4)
 *
 * Trasmette il carattere c sul terminale devNo tramite NSYS5.
 * Restituisce la status word del device.
 */
int transmitChar(int devNo, char c);

/*
 * receiveChar  (usata da SYS5)
 *
 * Riceve un carattere dal terminale devNo tramite NSYS5.
 * Restituisce la status word del device (stato in byte basso,
 * carattere in byte successivo).
 */
int receiveChar(int devNo);

#endif /* SYSSUPPORT_H_INCLUDED */