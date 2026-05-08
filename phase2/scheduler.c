#include "./headers/scheduler.h"

// Definizione di start Time Of Dispatch, dichiarata extern in scheduler.h
cpu_t start_tod;


void scheduler(void) {
    currentProcess = NULL;

    /* ----------------------------------------------------------------
     * CASO 1: c'è almeno un processo pronto -> dispatch
     * ---------------------------------------------------------------- */
    if (!emptyProcQ(&readyQueue)) {

        /* Estrae il PCB a priorità massima */
        currentProcess = removeProcQ(&readyQueue);

        /* Carica il PLT: allo scadere genera interrupt linea 1 (PLT) */
        setTIMER(TIMESLICE * *((unsigned int *) TIMESCALEADDR)); // questo è il time slice specifico e deciso da initial.c

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
     * ---------------------------------------------------------------- */
    if (softBlockCount > 0) {
        /* Disabilita il PLT: sia resettando il timer sia togliendo il bit in MIE */
        setTIMER(TIMESLICE);
        setMIE(MIE_ALL & ~MIE_MTIE_MASK);

        /* Abilita globalmente la ricezione degli interrupt */
        unsigned int status = getSTATUS();
        status |= MSTATUS_MIE_MASK;
        setSTATUS(status);

        /* Attende il prossimo interrupt (device o Interval Timer).
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