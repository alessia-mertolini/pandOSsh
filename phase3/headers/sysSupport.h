#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

#include "./costantiNopoli.h"
#include "../testers/h/tconst.h"
#include "../../phase2/headers/constants.h" // usiamo solo cause_exccode
#include "./initProc.h"
#include <uriscv/liburiscv.h>

#define TERMREGADDR(devNo) (termreg_t*)(TERM0ADDR + (devNo * 0x10))

/* -----------------------------------------------------------------------
 * Sezione 6 - General Exception Handler
 * ----------------------------------------------------------------------- */
/*
 * Entry point per tutte le eccezioni non-TLB passate su dal Nucleus
 * al Support Level (Spec Phase 3 Sezione 6).
 *
 * Passi:
 *   1. NSYS8 (GETSUPPORTPTR): recupera la Support Structure del processo.
 *   2. Legge il Cause register da sup_exceptState[GENERALEXCEPT].
 *   3. Smista:
 *        excCode == SYSEXCEPTION (8) -> syscallExceptionHandler
 *        altro                       -> programTrapSupportHandler (Grazia)
 *
 * Nota: i TLB exception (excCode 24-28) non arrivano qui; il Nucleus li
 * smista al Pager tramite sup_exceptContext[PGFAULTEXCEPT] (Gabriele).
 *
 * Non ritorna mai.
 */
void generalExceptionHandler(void);


/* -----------------------------------------------------------------------
 * Sezione 8 - Programm Trap Support Handler
 * ----------------------------------------------------------------------- */
/**
 * Termina il processo in modo ordinato.
 * holdingMutex: 1 se il chiamante detiene già swapPoolSemaphore
 *               (es. path di errore nel pager), 0 altrimenti.
 * Non ritorna mai.
 */
void programTrapSupportHandler(support_t *sup, int holdingMutex);



#endif