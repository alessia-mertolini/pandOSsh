/* -----------------------------------------------------------------------
 * initial.c - PandOSsh Phase 2 - Nucleus Initialization
 *
 * Contiene main() e definisce tutte le variabili globali del nucleo.
 * Esegue l'inizializzazione e avvia il file di test.
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

// funzione di test per il debug
extern void test(void);

/* -----------------------------------------------------------------------
 * Variabili globali del Nucleus
 * Dichiarate extern in scheduler.h -> visibili a tutti i moduli.
 * ----------------------------------------------------------------------- */

// Numero di processi avviati e non ancora terminati
int processCount;

// Numero di processi nello stato di "waiting" per I/O o timer request
int softBlockCount;

// Coda dei processi pronti, ordinata per priorità
struct list_head readyQueue;

// Puntatore al PCB del processo in esecuzione (stato = "running")
pcb_t *currentProcess;

/* Semafori device: uno per ogni sub-device + uno per il Pseudo-clock.
 * Indicizzazione:
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
 *   idx = (IL_TERMINAL - 3) * DEVPERINT + devNo = 32 + devNo
 * Formula per terminali RX:
 *   idx = (IL_TERMINAL - 3) * DEVPERINT + DEVPERINT + devNo = 40 + devNo
 */
int deviceSemaphores[SEMDEVLEN];

// Entry point di pandOSsh
int main(void) {
    // Inizializzazione del passUpVector per il processore 0
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;

    // Gestore degli eventi TLB da exceptions.c
    extern void uTLB_RefillHandler(void);
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK;

    // Gestore delle eccezioni da exceptions.c
    extern void exceptionHandler(void);
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = KERNELSTACK;

    // Inizializzazione strutture di fase 1
    initPcbs();
    initASL();

    // Inizializzazione variabili globali del nucleo
    processCount = 0;
    softBlockCount = 0;
    mkEmptyProcQ(&readyQueue);
    currentProcess = NULL;

    // Tutti i sdevice semaphores inizializzati a 0
    for (int i = 0; i < SEMDEVLEN; i++) {
        deviceSemaphores[i] = 0;
    }

    //Carica l'Interval Timer con 100ms (Pseudo-clock tick)/
    LDIT(PSECOND); //qui viene settato il time slice che poi viene usato in tutto il SO

    //Istanzia il primo processo e lo inserisce nella Ready Queue/
    pcb_t *firstProcess = allocPcb();

    /* allocPcb() non dovrebbe fallire: la pcbFree è appena stata
     * inizializzata con MAXPROC elementi. Controllo difensivo. */
    if (firstProcess == NULL) {
        PANIC();
    }

    // Processor state del primo processo
    firstProcess->p_s.mie = MIE_ALL; // Interrupt abilitati (mie = MIE_ALL)
    firstProcess->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // Kernel mode on
    RAMTOP(firstProcess->p_s.reg_sp);   // SP = top della RAM
    firstProcess->p_s.pc_epc  = (memaddr)test; // PC a test
    
    /*
        Inoltre allocPcb() gia' inizializza:
            - Process Tree fields a NULL
            - p_time = 0
            - p_semAdd = NULL
            - p_supportStruct = NULL
     */

    // Inserisce nella Ready Queue e aggiorna il contatore
    insertProcQ(&readyQueue, firstProcess);
    processCount++;

    //Avvia lo scheduler (non ritorna mai)
    scheduler();

    /* UNREACHABLE - main() non deve mai tornare */
    return 0;
}