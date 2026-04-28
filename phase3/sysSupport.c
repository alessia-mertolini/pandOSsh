// sysSupport.c  –  Phase 3: Support Level exception handlers

#include "headers/sysSupport.h"
#include "headers/vmSupport.h"
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>

/* =========================================================================
 * programTrapSupportHandler  [Section 8]
 *   Termina il U-proc in modo ordinato.
 *   Se il processo detiene la mutua esclusione sulla Swap Pool,
 *   il chiamante deve rilasciarla PRIMA di invocare questa funzione.
 *   Esegue le stesse operazioni di SYS2: V sul semaforo master/shell,
 *   poi NSYS2 per terminare il processo.
 * ========================================================================= */
void programTrapSupportHandler(support_t *sup) {
    (void)sup;  /* non usato direttamente, ma utile per future estensioni   */

    /* V sul masterSemaphore (se il processo è la shell) oppure
       sul shellSemaphore (se è un programma figlio della shell).
       La logica corretta dipende dall'architettura shell/initProc;
       qui si esegue la V generica sul masterSemaphore [Section 9].
       In un'implementazione completa: distinguere shell da child.           */
    extern int masterSemaphore;
    SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);

    /* Termina il processo tramite NSYS2                                     */
    SYSCALL(TERMPROCESS, 0, 0, 0);

    /* UNREACHABLE – NSYS2 non ritorna mai                                   */
}

/* =========================================================================
 * generalExceptionHandler  [Section 6]
 *   Entry point passato nella sup_exceptContext[GENERALEXCEPT].
 *   Smista tra SYSCALL (codici > 0) e Program Trap.
 * ========================================================================= */
void generalExceptionHandler(void) {
    /* Recupera la Support Structure del processo corrente                   */
    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Lo stato salvato al momento dell'eccezione generale                   */
    state_t *excState = &sup->sup_exceptState[GENERALEXCEPT];

    /* Legge il codice di eccezione dal registro Cause                       */
    unsigned int excCode = (excState->cause & GETEXECCODE) >> 2;

    if (excCode == EXC_SYSCALL) {
        /* Incrementa il PC di 4 per tornare all'istruzione successiva       */
        excState->pc_epc += WORDLEN;
        syscallSupportHandler(sup, excState);
    } else {
        /* Qualsiasi altra eccezione (trap) termina il processo              */
        programTrapSupportHandler(sup);
    }
}

/* =========================================================================
 * syscallSupportHandler  [Section 7]
 *   Gestisce le SYSCALL con numero >= 1 (positive) per i U-proc.
 *   Il numero di syscall è in a0; parametri in a1, a2, a3.
 * ========================================================================= */
void syscallSupportHandler(support_t *sup, state_t *excState) {
    int sysNum = (int)excState->reg_a0;

    switch (sysNum) {
        case TERMINATE:       /* SYS2 – termina il U-proc [Section 7.1]    */
            programTrapSupportHandler(sup);
            break;

        case WRITETERMINAL:   /* SYS4 – scrive sul terminale [Section 7.2] */
            /* Da implementare: writeTerminal(sup, excState)                */
            break;

        case READTERMINAL:    /* SYS5 – legge dal terminale [Section 7.3]  */
            /* Da implementare: readTerminal(sup, excState)                 */
            break;

        case EXECUTE:         /* SYS6 – esegue un nuovo U-proc [Section 7.4]*/
            /* Da implementare: execute(sup, excState)                      */
            break;

        default:
            /* SYSCALL non riconosciuta → tratta come program trap          */
            programTrapSupportHandler(sup);
            break;
    }
}