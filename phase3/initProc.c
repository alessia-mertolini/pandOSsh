#include "./headers/initProc.h"

/* ----- PROTOTIPI ----- */

static support_t *allocFromFreeList(void);
static support_t *allocSupportStruct(int asid);


extern void initSwapStructures(); // importata da vmSupport.h

/* ----- IMPLEMENTAZIONE ----- */
// Semaforo per I/O su terminale
int readTerminalSemaphore;
int writeTerminalSemaphore;

int  masterSemaphore;   /* shell container */
int  shellSemaphore;   /* semaforo per i processi iniziati dalla shell */

// Array dei semafori dei device - 8 flash device
int flashSem[DEVPERINT];


// Pool statico di Support Structure, una per ogni possibile U-proc.
support_t supportStructPool[UPROCMAX];

// Lista delle support struct libere
static support_t *freeList = NULL;


void test() {
    // Inizializzazione strutture di supporto
    initSwapStructures();

    // init semafori dei device
    for(int i = 0; i < DEVPERINT; i++) {
        flashSem[i] = 1;
    }
    //semafori per il terminale (read e write)
    readTerminalSemaphore = 1;
    writeTerminalSemaphore = 1;
    
    // semafori per la gestione dei processi
    masterSemaphore = 0;
    shellSemaphore = 0;

    // Popolazione della free list con ASID 2-8
    for (int i = UPROCMAX - 1; i >= 1; i--) {
        deallocSupportStruct(&supportStructPool[i]);
    }

    // lancio della shell
    launchUProc(1);

    // P sul masterSemaphore; rimarra' bloccato qui fino a quando la shell non fara' exit
    SYSCALL(PASSEREN, (int)&masterSemaphore,0,0);

    SYSCALL(TERMPROCESS,0,0,0);
}


// Aggiunge una support structure alla free list
void deallocSupportStruct(support_t *sup) {
    sup->s_list.next = (struct list_head *)freeList;
    sup->sup_asid = -1;
    freeList = sup;
}


// Preleva una support structure dalla free list
static support_t *allocFromFreeList(void) {
    if (freeList == NULL) return NULL;
    support_t *sup = freeList;
    freeList = (support_t *)sup->s_list.next;
    return sup;
}


support_t* launchUProc(int asid) {
    //stato del processore iniziale dell'user process iniziale
    state_t uprocState;

    // azzeramento manuale per risolvere problemi di lettura di area di memoria garbage
    char *ptr = (char *)&uprocState;
    for (int i = 0; i < sizeof(state_t); i++) {
        ptr[i] = 0;
    }
    
    support_t  *sup = allocSupportStruct(asid);
    
    // Stato iniziale del U-proc
    // PC: inizio sezione .text degli U-proc
    uprocState.pc_epc  = UPROCSTARTADDR;

    // SP: cima della stack page nel kuseg
    uprocState.reg_sp  = USERSTACKTOP;
    
    // User mode, tutti gli interrupt abilitati, PLT abilitato
    uprocState.status  = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK; // interrupt abilitati          

    // Abilita il Local Timer del processore
    uprocState.mie = MIE_ALL;

    // Set dell'ASID nel campo EntryHi
    uprocState.entry_hi = asid << ASIDSHIFT;

    // Creazione del processo
    int pid = SYSCALL(CREATEPROCESS,(int)&uprocState, PROCESS_PRIO_LOW, (int)sup);

    if (pid < 0) {
        /* 
            Situazione di errore che non dovrebbe essere raggiunta (caso di allocazione di piu' di 20 pcb)
            Si tratta solo di un controllo aggiuntivo
        */
        PANIC();
    }

    return sup;
}


//Funzione di inizializzazione delle strutture dati necessarie alla swap pool
//'swapPoolSemaphore' viene inizializzato come da specifiche pronto per accettare una 'P'
//inizializzazione dei campi della struct 'swap_t' utilizzata da 'swapPoolTable'
void initSwapStructures();
static void initPageTable(support_t *s) {
    for(int i = 0; i < USERPGTBLSIZE; i++) {
        volatile unsigned int vpn;


        if(i == USERPGTBLSIZE - 1) { // per l'ultima pagina vi e' invece un registro preposto
          vpn = STACK_PAGE_VPN;
        } else { // per le prime 31 pagine e un semplice contatore da 1 a 31
          vpn = KUSEG_VPN + i;
        }

        // ogni entryHI e' composta dai primi 20 bit per il numero vpn, che quindi shiftiamo a sx
        // e dagli ultimi 3 per l'ASID, che quindi sommiamo per avere in fondo a destra come
        // indirizzo del registro puntato dalla pageTable
        s->sup_privatePgTbl[i].pte_entryHI = (vpn << VPNSHIFT) | (s->sup_asid << ASIDSHIFT);

        // pagine sempre inizializzate in scrittura
        s->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
    }
}


/* 
 * allocSupportStruct() - Restituisce la Support Structure per l'ASID dato
 * e la inizializza completamente.
 */
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

    // Inizializzazione Page Table
    initPageTable(sup);

    // Calcolo dinamico degli Stack Pointer fisici in RAM
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

    // Gestione delle eccezioni
    // Page faults
    sup->sup_exceptContext[PGFAULTEXCEPT].pc       = (memaddr)pager;
    sup->sup_exceptContext[PGFAULTEXCEPT].status   = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sup->sup_exceptContext[PGFAULTEXCEPT].stackPtr = tlb_sp;

    // General exceptions (syscall + program trap)
    sup->sup_exceptContext[GENERALEXCEPT].pc       = (memaddr)generalExceptionHandler;
    sup->sup_exceptContext[GENERALEXCEPT].status   = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sup->sup_exceptContext[GENERALEXCEPT].stackPtr = gen_sp;

    TLBCLR();

    return sup;
}