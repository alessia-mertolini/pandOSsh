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
#include "../../headers/const.h"
#include "./costantiNopoli.h"
#include "../testers/h/tconst.h"
#include "../../headers/types.h"

extern int swapPoolSemaphore;

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
 * receiveChar (usata da SYS5)
 *
 * Riceve un carattere dal terminale devNo tramite NSYS5.
 * Restituisce la status word del device (stato in byte basso,
 * carattere in byte successivo).
 */
int receiveChar(int devNo);

/*
 * programTrapSupportHandler  (Sezione 8)
 *
 * Termina il processo in modo ordinato.
 * holdingMutex: 1 se il chiamante detiene già swapPoolSemaphore
 *               (es. path di errore nel pager), 0 altrimenti.
 * Non ritorna mai.
 */
void programTrapSupportHandler(support_t *sup, int holdingMutex);

#define TERM_REG_ADDR(devNo) \
        ((termreg_t *)(START_DEVREG + ((IL_TERMINAL - 3) * 0x80) + ((devNo) * 0x10)))

#endif /* SYSSUPPORT_H_INCLUDED */