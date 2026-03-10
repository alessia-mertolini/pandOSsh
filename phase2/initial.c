/* -----------------------------------------------------------------------
 * initial.c - PandOSsh Phase 2 - Nucleus Initialization
 *
 * Contiene main() e definisce tutte le variabili globali del Nucleus.
 * Esegue l'inizializzazione one-time e avvia il primo processo (test).
 * Al termine chiama scheduler(), che non ritorna mai.
 * ----------------------------------------------------------------------- */

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "../headers/const.h"
#include "../headers/listx.h"
#include "../headers/types.h"
#include "./headers/scheduler.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

/* -----------------------------------------------------------------------
 * Dichiarazione della funzione test (fornita esternamente per il debug)
 * ----------------------------------------------------------------------- */
extern void test(void);

/* -----------------------------------------------------------------------
 * Variabili globali del Nucleus
 * Dichiarate extern in scheduler.h -> visibili a tutti i moduli.
 * ----------------------------------------------------------------------- */

/* Numero di processi avviati e non ancora terminati */
int processCount;

/* Numero di processi bloccati per I/O o Pseudo-clock */
int softBlockCount;

/* Coda dei processi pronti, ordinata per priorità */
struct list_head readyQueue;

/* Puntatore al PCB del processo in esecuzione (NULL = nessuno) */
pcb_t *currentProcess;

/* Semafori device: uno per ogni sub-device + uno per il Pseudo-clock.
 * Indicizzazione (vedere anche interrupts.c di Grazia):
 *
 *   Linea  3 (Disk)     : indici  0 -  7 [priorita' piu' alta]
 *   Linea  4 (Flash)    : indici  8 - 15
 *   Linea  5 (Ethernet) : indici 16 - 23
 *   Linea  6 (Printer)  : indici 24 - 31
 *   Linea  7 Term TX    : indici 32 - 39
 *   Linea  7 Term RX    : indici 40 - 47
 *   Pseudo-clock        : indice 48  (PSEUDOCLOCK_SEM_IDX)
 *
 * Formula per linee 3-6:
 *   idx = (line - 3) * DEVPERINT + devNo
 * Formula per terminali TX (priorità più alta):
 *   idx = (IL_TERMINAL - 3) * DEVPERINT + devNo          = 32 + devNo
 * Formula per terminali RX:
 *   idx = (IL_TERMINAL - 3) * DEVPERINT + DEVPERINT + devNo = 40 + devNo
 */
int deviceSemaphores[SEMDEVLEN];

/* -----------------------------------------------------------------------
 * main() - entry point del Nucleus
 * ----------------------------------------------------------------------- */
int main(void) {
    /* ------------------------------------------------------------------
     * 1. Popola il Pass Up Vector del Processore 0
     * ------------------------------------------------------------------ */
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;

    /* Handler TLB-Refill: da definire in exceptions.c */
    extern void uTLB_RefillHandler(void);
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK;

    /* Handler eccezioni generali: funzione placeholder in exceptions.c */
    extern void exceptionHandler(void);
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = KERNELSTACK;

    /* ------------------------------------------------------------------
     * 2. Inizializza le strutture dati di Phase 1
     * ------------------------------------------------------------------ */
    initPcbs();
    initASL();

    /* ------------------------------------------------------------------
     * 3. Inizializza le variabili globali del Nucleus
     * ------------------------------------------------------------------ */
    processCount = 0;
    softBlockCount = 0;
    mkEmptyProcQ(&readyQueue);
    currentProcess = NULL;

    /* Tutti i semafori device inizializzati a 0 (sincronizzazione, non ME) */
    for (int i = 0; i < SEMDEVLEN; i++) {
        deviceSemaphores[i] = 0;
    }

    /* ------------------------------------------------------------------
     * 4. Carica l'Interval Timer con 100ms (Pseudo-clock tick)
     * ------------------------------------------------------------------ */
    LDIT(PSECOND);

    /* ------------------------------------------------------------------
     * 5. Istanzia il primo processo e lo inserisce nella Ready Queue
     * ------------------------------------------------------------------ */
    pcb_t *firstProcess = allocPcb();

    /* allocPcb() non dovrebbe fallire: la pcbFree è appena stata
     * inizializzata con MAXPROC elementi. Controllo difensivo. */
    if (firstProcess == NULL) {
        PANIC();
    }

    /* Processor state del primo processo:
     *   - Interrupt abilitati (mie = MIE_ALL)
     *   - Kernel-mode con MPIE settato (status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M)
     *   - SP = RAMTOP (usa l'ultimo frame RAM come stack)
     *   - PC = indirizzo di test
     */
    firstProcess->p_s.mie = MIE_ALL;
    firstProcess->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
    RAMTOP(firstProcess->p_s.reg_sp);   /* SP = top della RAM               */
    firstProcess->p_s.pc_epc  = (memaddr)test;

    /* Campi PCB aggiuntivi (già azzerati da allocPcb, ma espliciti per chiarezza) */
    firstProcess->p_parent = NULL;
    firstProcess->p_time = 0;
    firstProcess->p_semAdd = NULL;
    firstProcess->p_supportStruct = NULL;
    
    // ? firstProcess->p_prio = 0;
    // ? inizializzazione di p_pid?

    /* Inserisce nella Ready Queue e aggiorna il contatore */
    insertProcQ(&readyQueue, firstProcess);
    processCount++;

    /* ------------------------------------------------------------------
     * 6. Avvia lo scheduler (non ritorna mai)
     * ------------------------------------------------------------------ */
    scheduler();

    /* UNREACHABLE - main() non deve mai tornare */
    return 0;
}