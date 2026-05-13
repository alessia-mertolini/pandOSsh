#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

#include "./costantiNopoli.h"
#include "../testers/h/tconst.h"
#include "../../phase2/headers/constants.h" // usiamo solo cause_exccode
#include "./initProc.h"
#include <uriscv/liburiscv.h>
 
#define TERM_REG_ADDR(devNo) \
         ((termreg_t *)(START_DEVREG + ((IL_TERMINAL - 3) * 0x80) + ((devNo) * 0x10)))


/*
 * Trasmette il carattere c sul sotto-dispositivo di trasmissione del
 * terminale devNo tramite NSYS5 (DoIO sincrono).
 *
 * Layout device register terminale (Phase 2 Sezione 12.1):
 *   +0x0  RECV_STATUS
 *   +0x4  RECV_COMMAND
 *   +0x8  TRANSM_STATUS
 *   +0xC  TRANSM_COMMAND
 *
 * Valore scritto in TRANSM_COMMAND: (char << 8) | TRANSMITCHAR(2).
 * Restituisce la status word: byte basso = stato, byte 1 = carattere eco.
 */
static int transmitChar(int devNo, char c);


/*
 * Riceve un carattere dal sotto-dispositivo di ricezione del terminale
 * devNo tramite NSYS5 (DoIO sincrono).
 *
 * Scrive RECEIVECHAR(2) in RECV_COMMAND per avviare la ricezione.
 * Restituisce la status word: byte basso = stato, byte 1 = carattere ricevuto.
 */
static int receiveChar(int devNo);


/* -----------------------------------------------------------------------
* SYS2 - Terminate (Sezione 7.1)
* ----------------------------------------------------------------------- */
/*
 * Termina l'U-proc chiamante. Non ritorna mai.
 *
 * sys2Terminate
 *
 * Operazioni comuni di terminazione (Spec Sezione 7.1 e 9):
 *   1. V sul semaforo giusto:
 *        - shell  -> V(masterSemaphore)  sblocca InstantiatorProcess
 *        - figlio -> V(shellSemaphore)   sblocca la shell in attesa su SYS6
 *   2. NSYS2 (TERMPROCESS): elimina il processo e tutti i discendenti.
 *
 * Non ritorna mai.
 */
static void sys2Terminate(support_t *sup, int holdingMutex);


/* -----------------------------------------------------------------------
* SYS4 - WriteTerminal (Sezione 7.2)
* ----------------------------------------------------------------------- */
/*
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = virtAddr   indirizzo virtuale della stringa
 *   a2 = len        numero di caratteri da trasmettere
 *
 * Errori che causano terminazione dell'U-proc (Spec Sezione 7.2):
 *   - len < 0 o len > MAXSTRLENG
 *   - virtAddr o virtAddr+len fuori da [KUSEG, USERSTACKTOP)
 *
 * Ritorno (via LDST):
 *   successo:    a0 = numero caratteri trasmessi
 *   errore I/O:  a0 = -(status word del device)
 */
static void sys4WriteTerminal(support_t *sup);


/* -----------------------------------------------------------------------
* SYS5 - ReadTerminal (Sezione 7.3)
* ----------------------------------------------------------------------- */
/*
* Parametri da sup_exceptState[GENERALEXCEPT]:
*   a1 = virtAddr   indirizzo virtuale del buffer di destinazione
*
* Legge caratteri finché non arriva '\n' o si raggiunge MAXSTRLENG.
 *
 * Errori che causano terminazione dell'U-proc (Spec Sezione 7.3):
 *   - virtAddr fuori da [KUSEG, USERSTACKTOP)
 *
 * Ritorno (via LDST):
 *   successo:    a0 = numero caratteri ricevuti
 *   errore I/O:  a0 = -(status word del device)
 */
static void sys5ReadTerminal(support_t *sup);


/* -----------------------------------------------------------------------
 * SYS6 - Execute (Sezione 7.4)
----------------------------------------------------------------------- */
/*
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = asid   ASID dell'U-proc da avviare
 *
 * Solo la shell può chiamare SYS6; altrimenti si tratta come Program Trap.
 *
 * Flusso:
 *   1. launchUProc(asid): crea il processo tramite NSYS1 (in initProc.c).
 *   2. P(shellSemaphore): blocca la shell.
 *   3. Il figlio al termine chiama SYS2 -> V(shellSemaphore): risveglia la shell.
 *   4. Ritorna con a0 = 0, PC += 4.
 */
static void sys6Execute(support_t *sup);


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
 * Sezione 7 - SYSCALL Exception Handler
 * ----------------------------------------------------------------------- */
/**
 * Gestisce le eccezioni SYSCALL (identificatori > 0) delegate dal Nucleus[cite: 227, 234].
 * Fornisce i servizi di Support Level (SYS2, SYS4, SYS5, SYS6) per l'U-proc corrente 
 * smistando la richiesta in base al codice operativo presente nel registro a0[cite: 238, 241].
*/
static void syscallExceptionHandler(support_t *sup);


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