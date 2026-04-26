/*
 * sysSupport.c
 * Phase 3 - Level 4: Support Level
 *
 * NON implementa:
 *   - Program Trap handler (Sezione 8)  -> vmSupport.c 
 *   - Pager / swap pool                 -> vmSupport.c 
 *   - TLB-Refill handler                -> exceptions.c 
 *   - spawnUProc / initProc             -> initProc.c
 */

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include "../../headers/const.h"
#include "sysSupport.h"
#include "costantiNopoli.h"

/* -----------------------------------------------------------------------
 * Simboli esterni forniti dagli altri moduli
 * ----------------------------------------------------------------------- */

/* initProc.c */
extern int  masterSemaphore;   /* P'ato da InstantiatorProcess, V'ato dalla shell alla fine */
extern int  shellSemaphore;    /* P'ato dalla shell in SYS6, V'ato dal figlio in SYS2       */
extern void spawnUProc(int asid); /* crea e lancia un U-proc tramite NSYS1                  */

/* vmSupport.c (Grazia) - Sezione 8 */
extern void programTrapHandler(support_t *sup);

/* -----------------------------------------------------------------------
 * Helper privati
 * ----------------------------------------------------------------------- */

/*
 * isShellProcess
 * Ritorna 1 se il processo è la shell (ASID == SHELL_ASID).
 */
static int isShellProcess(support_t *sup) {
    return (sup->sup_asid == SHELL_ASID);
}

/*
 * terminateCurrentUProc
 *
 * Operazioni comuni di terminazione (Spec Sezione 7.1 e 9):
 *   1. V sul semaforo giusto:
 *        - shell  -> V(masterSemaphore)  sblocca InstantiatorProcess
 *        - figlio -> V(shellSemaphore)   sblocca la shell in attesa su SYS6
 *   2. NSYS2 (TERMPROCESS): elimina il processo e tutti i discendenti.
 *
 * Non ritorna mai.
 */
static void terminateCurrentUProc(support_t *sup) {
    if (isShellProcess(sup)) {
        SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
    } else {
        SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
    }
    SYSCALL(TERMPROCESS, 0, 0, 0);
    /* non ritorna */
}

/* -----------------------------------------------------------------------
 * I/O terminale - basso livello (Sezione 7.2, 7.3)
 * ----------------------------------------------------------------------- */

/*
 * transmitChar
 *
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
int transmitChar(int devNo, char c) {
    termreg_t *termReg = TERM_REG_ADDR(devNo);

    /* Costruisce il comando: byte alto = carattere, byte basso = TRANSMITCHAR. */
    unsigned int command =
        ((unsigned int)(unsigned char)c << TERM_CHAR_SHIFT) | TRANSMITCHAR;

    return SYSCALL(DOIO, (int)&(termReg->transm_command), (int)command, 0);
}

/*
 * receiveChar
 *
 * Riceve un carattere dal sotto-dispositivo di ricezione del terminale
 * devNo tramite NSYS5 (DoIO sincrono).
 *
 * Scrive RECEIVECHAR(2) in RECV_COMMAND per avviare la ricezione.
 * Restituisce la status word: byte basso = stato, byte 1 = carattere ricevuto.
 */
int receiveChar(int devNo) {
    termreg_t *termReg = TERM_REG_ADDR(devNo);

    return SYSCALL(DOIO, (int)&(termReg->recv_command), RECEIVECHAR, 0);
}

/* -----------------------------------------------------------------------
 * SYS2 - Terminate (Sezione 7.1)
 * ----------------------------------------------------------------------- */

/*
 * sys2Terminate
 * Termina l'U-proc chiamante. Non ritorna mai.
 */
static void sys2Terminate(support_t *sup) {
    terminateCurrentUProc(sup);
}

/* -----------------------------------------------------------------------
 * SYS4 - WriteTerminal (Sezione 7.2)
 * ----------------------------------------------------------------------- */

/*
 * sys4WriteTerminal
 *
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = virtAddr   indirizzo virtuale della stringa
 *   a2 = len        numero di caratteri da trasmettere
 *
 * Errori che causano terminazione dell'U-proc (Spec Sezione 7.2):
 *   - len < 0 o len > MAXSTRLENG
 *   - virtAddr o virtAddr+len fuori da [KUSEG, KUSEG_TOP)
 *
 * Ritorno (via LDST):
 *   successo:    a0 = numero caratteri trasmessi
 *   errore I/O:  a0 = -(status word del device)
 */
static void sys4WriteTerminal(support_t *sup) {
    state_t *savedState = &sup->sup_exceptState[GENERALEXCEPT];
    char    *virtAddr   = (char *)savedState->reg_a1;
    int      len        = (int)  savedState->reg_a2;

    /* Il terminale della shell è il numero 0. */
    int devNo = SHELL_TERMINAL;

    /* Validazione lunghezza. */
    if (len < 0 || len > MAXSTRLENG) {
        terminateCurrentUProc(sup);
        /* non ritorna */
    }

    /* Validazione indirizzo: la stringa deve stare interamente in kuseg. */
    if ((unsigned int)virtAddr < KUSEG ||
        (unsigned int)virtAddr + (unsigned int)len > KUSEG_TOP) {
        terminateCurrentUProc(sup);
        /* non ritorna */
    }

    /* Trasmissione carattere per carattere. */
    int transmitted = 0;
    int i;
    for (i = 0; i < len; i++) {
        int status = transmitChar(devNo, virtAddr[i]);

        /* Controlla solo il byte di stato. */
        if ((status & TERM_STATUS_MASK) != OKCHARTRANS) {
            /* Errore device: ritorna il negativo dello status word. */
            savedState->reg_a0 = (unsigned int)(-(status));
            savedState->pc_epc += WORDLEN;
            LDST(savedState);
            /* non ritorna */
        }
        transmitted++;
    }

    /* Successo. */
    savedState->reg_a0 = (unsigned int)transmitted;
    savedState->pc_epc += WORDLEN;
    LDST(savedState);
}

/* -----------------------------------------------------------------------
 * SYS5 - ReadTerminal (Sezione 7.3)
 * ----------------------------------------------------------------------- */

/*
 * sys5ReadTerminal
 *
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = virtAddr   indirizzo virtuale del buffer di destinazione
 *
 * Legge caratteri finché non arriva '\n' o si raggiunge MAXSTRLENG.
 *
 * Errori che causano terminazione dell'U-proc (Spec Sezione 7.3):
 *   - virtAddr fuori da [KUSEG, KUSEG_TOP)
 *
 * Ritorno (via LDST):
 *   successo:    a0 = numero caratteri ricevuti
 *   errore I/O:  a0 = -(status word del device)
 */
static void sys5ReadTerminal(support_t *sup) {
    state_t *savedState = &sup->sup_exceptState[GENERALEXCEPT];
    char    *virtAddr   = (char *)savedState->reg_a1;

    int devNo = SHELL_TERMINAL;

    /* Validazione indirizzo. */
    if ((unsigned int)virtAddr < KUSEG ||
        (unsigned int)virtAddr >= KUSEG_TOP) {
        terminateCurrentUProc(sup);
        /* non ritorna */
    }

    int received = 0;
    while (1) {
        int  status = receiveChar(devNo);
        int  stCode = status & TERM_STATUS_MASK;
        char c      = (char)((status & TERM_CHAR_MASK) >> TERM_CHAR_SHIFT);

        if (stCode != OKCHARTRANS) {
            /* OKCHARTRANS == 5 vale sia per trasmissione che ricezione
             * (CHARRECV == 5 è lo stesso valore in pandos_const.h). */
            savedState->reg_a0 = (unsigned int)(-(status));
            savedState->pc_epc += WORDLEN;
            LDST(savedState);
            /* non ritorna */
        }

        virtAddr[received] = c;
        received++;

        /* Fine riga o buffer pieno. */
        if (c == '\n' || received >= MAXSTRLENG) {
            break;
        }
    }

    savedState->reg_a0 = (unsigned int)received;
    savedState->pc_epc += WORDLEN;
    LDST(savedState);
}

/* -----------------------------------------------------------------------
 * SYS6 - Execute (Sezione 7.4)
 * ----------------------------------------------------------------------- */

/*
 * sys6Execute
 *
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = asid   ASID dell'U-proc da avviare
 *
 * Solo la shell può chiamare SYS6; altrimenti si tratta come Program Trap.
 *
 * Flusso:
 *   1. spawnUProc(asid): crea il processo tramite NSYS1 (in initProc.c).
 *   2. P(shellSemaphore): blocca la shell.
 *   3. Il figlio al termine chiama SYS2 -> V(shellSemaphore): risveglia la shell.
 *   4. Ritorna con a0 = 0, PC += 4.
 */
static void sys6Execute(support_t *sup) {
    state_t *savedState = &sup->sup_exceptState[GENERALEXCEPT];
    int asid = (int)savedState->reg_a1;

    /* Solo la shell può eseguire SYS6 (Spec Sezione 7.4). */
    if (!isShellProcess(sup)) {
        programTrapHandler(sup);
        /* non ritorna */
    }

    /* Crea e lancia l'U-proc (initProc.c). */
    spawnUProc(asid);

    /* Blocca la shell fino alla terminazione del figlio. */
    SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);

    /* Figlio terminato: riprende la shell. */
    savedState->reg_a0 = 0;
    savedState->pc_epc += WORDLEN;
    LDST(savedState);
}

/* -----------------------------------------------------------------------
 * Sezione 7 - SYSCALL Exception Handler
 * ----------------------------------------------------------------------- */

/*
 * syscallExceptionHandler
 *
 * Smista le SYSCALL con a0 > 0 verso i gestori specifici.
 * SYSCALL non supportate -> programTrapHandler (Grazia, Sezione 8).
 * Non ritorna mai.
 */
void syscallExceptionHandler(support_t *sup) {
    state_t *savedState = &sup->sup_exceptState[GENERALEXCEPT];
    int sysNum = (int)savedState->reg_a0;

    switch (sysNum) {
        case TERMINATE:      /* 2 */
            sys2Terminate(sup);
            break;

        case WRITETERMINAL:  /* 4 */
            sys4WriteTerminal(sup);
            break;

        case READTERMINAL:   /* 5 */
            sys5ReadTerminal(sup);
            break;

        case EXECUTE:        /* 6 */
            sys6Execute(sup);
            break;

        default:
            /* SYSCALL non supportata: tratta come Program Trap. */
            programTrapHandler(sup);
            break;
    }
}

/* -----------------------------------------------------------------------
 * Sezione 6 - General Exception Handler
 * ----------------------------------------------------------------------- */

/*
 * generalExceptionHandler
 *
 * Entry point per tutte le eccezioni non-TLB passate su dal Nucleus
 * al Support Level (Spec Phase 3 Sezione 6).
 *
 * Passi:
 *   1. NSYS8 (GETSUPPORTPTR): recupera la Support Structure del processo.
 *   2. Legge il Cause register da sup_exceptState[GENERALEXCEPT].
 *   3. Smista:
 *        excCode == SYSEXCEPTION (8) -> syscallExceptionHandler
 *        altro                       -> programTrapHandler (Grazia)
 *
 * Nota: i TLB exception (excCode 24-28) non arrivano qui; il Nucleus li
 * smista al Pager tramite sup_exceptContext[PGFAULTEXCEPT] (Gabriele).
 *
 * Non ritorna mai.
 */
void generalExceptionHandler(void) {
    /* Recupera la Support Structure tramite NSYS8. */
    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Caso anomalo: il Nucleus non avrebbe dovuto passare qui senza sup. */
    if (sup == NULL) {
        SYSCALL(TERMPROCESS, 0, 0, 0);
        /* non ritorna */
    }

    /* Legge l'exception code dal Cause register dello stato salvato.
     * GETEXECCODE (0x0000007C) maschera i bit 2-6; poi shift di CAUSESHIFT(2)
     * per ottenere il valore numerico del codice. */
    unsigned int cause   = sup->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;

    if (excCode == SYSEXCEPTION) {
        /* SYSCALL con a0 > 0 (Sezione 7). */
        syscallExceptionHandler(sup);
    } else {
        /* Program Trap (Sezione 8) -> Grazia. */
        programTrapHandler(sup);
    }

    /* non ritorna mai */
}