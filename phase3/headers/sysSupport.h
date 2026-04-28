#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include "../../headers/const.h"

/* --- Funzioni esportate --------------------------------------------------- */

/* General exception handler del Support Level [Section 6]
 * Entry point impostato in sup_exceptContext[GENERALEXCEPT].pc             */
void generalExceptionHandler(void);

/* SYSCALL exception handler del Support Level [Section 7]
 * Smista le syscall con numero >= 1 (positive)                             */
void syscallSupportHandler(support_t *sup, state_t *excState);

/* Program Trap handler del Support Level [Section 8]
 * Termina il U-proc in modo ordinato (equivalente a SYS2).
 * ATTENZIONE: se il chiamante detiene il mutex sulla Swap Pool,
 * deve rilasciarlo PRIMA di invocare questa funzione.                      */
void programTrapSupportHandler(support_t *sup);

#endif