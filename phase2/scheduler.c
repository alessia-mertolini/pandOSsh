/* -----------------------------------------------------------------------
 * scheduler.c - PandOSsh Phase 2 - Scheduler
 *
 * Implementa lo scheduler round-robin preemptivo con time slice di 5ms
 * tramite il Processor Local Timer (PLT).
 *
 * ALGORITMO DI DISPATCH:
 *   1. Prende il PCB in testa alla readyQueue (priorità massima).
 *   2. Carica il PLT con TIMESLICE (5ms).
 *   3. Salva il TOD corrente in start_tod (contabilità CPU).
 *   4. LDST sul processor state del PCB -> il processo riprende.
 *
 * READY QUEUE VUOTA:
 *   processCount == 0              -> HALT  (tutto terminato, ok)
 *   processCount > 0, softBlock > 0 -> WAIT  (attesa interrupt)
 *   processCount > 0, softBlock == 0 -> PANIC (deadlock)
 *
 * NOTA su start_tod:
 *   Prima di bloccare o terminare currentProcess, exceptions.c e
 *   interrupts.c devono aggiornare p_time così:
 *
 *     cpu_t now; STCK(now);
 *     currentProcess->p_time += (now - start_tod);
 * ----------------------------------------------------------------------- */

#include "./headers/scheduler.h"

/* -----------------------------------------------------------------------
 * Definizione di start_tod (dichiarata extern in scheduler.h)
 * ----------------------------------------------------------------------- */
cpu_t start_tod;


/* -----------------------------------------------------------------------
 * scheduler()
 * ----------------------------------------------------------------------- */
void scheduler(void) {
    currentProcess = NULL;

    /* ----------------------------------------------------------------
     * CASO 1: c'è almeno un processo pronto -> dispatch
     * ---------------------------------------------------------------- */
    if (!emptyProcQ(&readyQueue)) {

        /* Estrae il PCB a priorità massima */
        currentProcess = removeProcQ(&readyQueue);

        /* Carica il PLT: allo scadere genera interrupt linea 1 (PLT) */
        setTIMER(TIMESLICE); //questo è il time slice specifico e deciso da initial.c

        /* Marca temporale TOD di inizio quanto */
        STCK(start_tod);

        /* Dispatch: carica il processor state del processo.
         * LDST non ritorna. */
        LDST(&(currentProcess->p_s));
        /*UNREACHABLE*/
    }

    /* ----------------------------------------------------------------
     * CASO 2: nessun processo esistente -> lavoro completato
     * ---------------------------------------------------------------- */
    if (processCount == 0) {
        HALT();
        /* UNREACHABLE */
    }

    /* ----------------------------------------------------------------
     * CASO 3: processi bloccati su I/O o Pseudo-clock -> Wait State
     *
     * Prima di WAIT occorre:
     *   a) disabilitare il PLT (MIE_MTIE_MASK) per evitare che il primo
     *      interrupt sia un PLT scaduto mentre non c'è currentProcess
     *   b) abilitare la gestione globale degli interrupt (MSTATUS_MIE_MASK)
     * ---------------------------------------------------------------- */
    if (softBlockCount > 0) {
        /* Disabilita solo il PLT, lascia attivi tutti gli altri interrupt */
        setMIE(MIE_ALL & ~MIE_MTIE_MASK);

        /* Abilita globalmente la ricezione degli interrupt */
        unsigned int status = getSTATUS();
        status |= MSTATUS_MIE_MASK;
        setSTATUS(status);

        /* Idle: attende il prossimo interrupt (device o Interval Timer).
         * L'exception handler riprenderà il controllo e chiamerà di
         * nuovo scheduler(). WAIT non ritorna. */
        WAIT();
        /* UNREACHABLE */
    }

    /* ----------------------------------------------------------------
     * CASO 4: processCount > 0 && softBlockCount == 0 -> DEADLOCK
     * ---------------------------------------------------------------- */
    PANIC();
    /* UNREACHABLE */
}