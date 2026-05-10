// File coordinatore
// Risiede la funzione TEST che funge da instantiator Process
// E' compito di questa funzione:
// inizializzare: SWAP POOL TABLE, SEMAPHORE POOL TABLE -> lo fa chimando initSwapStructs
// oltre a fare anche altre cose

/*
    // * REVISIONE CODICE SECONDO SPECIFICHE
    - Sezione 3 corretta
    - Sezione 4.1 corretta
    - Sezione 4.2 corretta
    - Sezione 5.1 corretta ma teniamo d'occhio cmd
    - Sezione 5.2 corretta (solo TLBCLR al momento)
    - Sezione 5.3 corretta
    - Sezione 5.4 corretta
    - Sezione 6 corretta
    - Sezione 7.1 corretta
    - Sezione 7.2 corretta

    - Sezione 9 corretta

    - Sezione 10
        - updateTLB optimized
        - free frame of process optimized
        - pageReplacementAlgorithm optimized
        - allocazione tlb e exc. handler stack direttamente dalla RAM
        - allocate/deallocate per le support struct

    - Sezione 11 corretta
*/

#include "./headers/initProc.h"
#include "headers/vmSupport.h"

extern void initSwapStructures(); // importata da vmSupport.h

// ----- Variabili esportate -------------

// Semaforo per I/O su terminale
int readTerminalSemaphore;
int writeTerminalSemaphore;

int  masterSemaphore;   /* shell container */
int  shellSemaphore;   /* semaforo per i processi iniziati dalla shell */

// Array dei semafori dei device - 8 flash device
int flashSem[DEVPERINT];

int procGoneSemaphore;

// -------------------------------

/* Pool statico di Support Structure, una per ogni possibile U-proc. */
support_t supportStructPool[UPROCMAX];

// Lista delle support struct libere
static support_t *freeList = NULL;


// ---------- Prototipi esportati ---------
void launchUProc(int asid); 
void deallocSupportStruct(support_t *sup);

// ----------- Prototipi locali ------------
static support_t *allocFromFreeList(void);
static support_t *allocSupportStruct(int asid);

void test() {
    initSwapStructures();

    // init semafori device
    for(int i = 0; i < DEVPERINT; i++) {
        flashSem[i] = 1;
    }
    //semafori per il terminale (read e write)
    readTerminalSemaphore = 1;
    writeTerminalSemaphore = 1;
    
    // processes
    masterSemaphore = 0;
    shellSemaphore = 0;
    procGoneSemaphore = 0;

    // Popola la free list con ASID 2-8
    for (int i = UPROCMAX - 1; i >= 1; i--) {
        deallocSupportStruct(&supportStructPool[i]);
    }

    /* 4. Lancia la shell (ASID 1 per convenzione)*/
    launchUProc(1);

    SYSCALL(PASSEREN, (int)&masterSemaphore,0,0);

    SYSCALL(TERMPROCESS,0,0,0);
}

// Aggiunge una support structure alla free list
void deallocSupportStruct(support_t *sup) {
    sup->s_list.next = (struct list_head *)freeList;
    freeList = sup;
}

// Preleva una support structure dalla free list
static support_t *allocFromFreeList(void) {
    if (freeList == NULL) return NULL;
    support_t *sup = freeList;
    freeList = (support_t *)sup->s_list.next;
    return sup;
}

/* -----------------------------------------------------------------------
* launchUProc() - Crea e dispatcha un U-proc tramite NSYS1 (CREATEPROCESS)
* ----------------------------------------------------------------------- */
void launchUProc(int asid) {
    //stato del processore iniziale dell'user process iniziale
    state_t uprocState;

    // AZZERAMENTO MANUALE PER RISOLVERE LA SPAZZATURA:
    char *ptr = (char *)&uprocState;
    for (int i = 0; i < sizeof(state_t); i++) {
        ptr[i] = 0;
    }
    
    support_t  *sup = allocSupportStruct(asid);
    
    /* ── Stato iniziale del U-proc ──────────────────────────────────────── */
    /* PC: inizio sezione .text degli U-proc (vedi linker script crtsi.o) */
    uprocState.pc_epc  = UPROCSTARTADDR;

    /* SP: cima della stack page nel kuseg */
    uprocState.reg_sp  = USERSTACKTOP;
    
    /* User mode, tutti gli interrupt abilitati, PLT abilitato */
    uprocState.status  = MSTATUS_MPIE_MASK          /* interrupt abilitati al ritorno */
    | MSTATUS_MIE_MASK;           /* interrupt globali on           */
    /* MPP = 00 => user mode al ritorno da eccezione */

    /* Abilita il Local Timer del processore */
    uprocState.mie = MIE_ALL;

    /* ASID nel campo EntryHi: identifica lo spazio di indirizzamento */
    uprocState.entry_hi = asid << ASIDSHIFT;

    /* ── Crea il processo ───────────────────────────────────────────────── */
    int pid = SYSCALL(CREATEPROCESS,(int)&uprocState, PROCESS_PRIO_LOW, (int)sup);

    if (pid < 0) {
        /* CREATEPROCESS fallito: situazione irrecuperabile */
        PANIC();
    }
}

/* -----------------------------------------------------------------------
 * allocSupportStruct() - Restituisce la Support Structure per l'ASID dato
 * e la inizializza completamente.
 * ----------------------------------------------------------------------- */
static support_t *allocSupportStruct(int asid) {
    support_t *sup;
    
    if (asid == SHELL_ASID) {
        // La shell usa sempre il primo slot — non passa per la free list
        sup = &supportStructPool[0];
    } else {
        sup = allocFromFreeList();
        if (sup == NULL) PANIC();  // non dovrebbe mai accadere
    }

    sup->sup_asid = asid;

    /* ── Page Table: SEMPRE reinizializzata a V=0, D=1 ─────────────────── */
    for (int i = 0; i < USERPGTBLSIZE - 1; i++) {
        sup->sup_privatePgTbl[i].pte_entryHI =
            ((0x80000 + i) << VPNSHIFT) | (asid << ASIDSHIFT);
        sup->sup_privatePgTbl[i].pte_entryLO = DIRTYON;   /* D=1, V=0 */
    }
    sup->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
        (0xBFFFF << VPNSHIFT) | (asid << ASIDSHIFT);
    sup->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;

    /* ── Calcolo dinamico degli Stack Pointer fisici in RAM ────────────── */
    volatile unsigned int ram_top;
    RAMTOP(ram_top);

    /*
     * L'ultimo frame (ram_top) è usato da test().
     * Assegniamo 2 frame fisici univoci per ogni ASID (da 1 a 8) partendo dal penultimo.
     * Poiché lo stack cresce verso il basso, il valore dello SP è la fine del frame.
     *
     * Esempio per ASID 1:
     * tlb_sp = ram_top - 1 * PAGESIZE  (penultimo frame)
     * gen_sp = ram_top - 2 * PAGESIZE  (terzultimo frame)
     */
    unsigned int tlb_sp = ram_top - ((asid * 2) - 1) * PAGESIZE;
    unsigned int gen_sp = ram_top - (asid * 2) * PAGESIZE;

    /* ── Contesti eccezione ─────────────────────────────────────────────── */
    sup->sup_exceptContext[PGFAULTEXCEPT].pc       = (memaddr)pager;
    sup->sup_exceptContext[PGFAULTEXCEPT].status   = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sup->sup_exceptContext[PGFAULTEXCEPT].stackPtr = tlb_sp;

    sup->sup_exceptContext[GENERALEXCEPT].pc       = (memaddr)generalExceptionHandler;
    sup->sup_exceptContext[GENERALEXCEPT].status   = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sup->sup_exceptContext[GENERALEXCEPT].stackPtr = gen_sp;

    return sup;
}