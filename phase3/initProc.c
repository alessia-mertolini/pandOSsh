// File coordinatore
// Risiede la funzione TEST che funge da instantiator Process
// E' compito di questa funzione:
// inizializzare: SWAP POOL TABLE, SEMAPHORE POOL TABLE -> lo fa chimando initSwapStructs
// oltre a fare anche altre cose

#include "../headers/const.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>

#include "./headers/vmSupport.h"  /* initSwapStructs()                   */
#include "./headers/sysSupport.h" /* indirizzo handler generali          */

#include <uriscv/types.h>

//Array dei semafori dei device - 8 flash device
int flashSem[DEVPERINT];

int readTerminalSemaphore;
int writeTerminalSemaphore;

int  masterSemaphore;   /* shell container */
int  shellSemaphore;   /* semaforo per i processi iniziati dalla shell */

/* Pool statico di Support Structure, una per ogni possibile U-proc. */
support_t supportStructPool[UPROCMAX];

/* -----------------------------------------------------------------------
 * Prototipi locali
 * ----------------------------------------------------------------------- */
void launchUProc(int asid);
static support_t *allocSupportStruct(int asid);


// --- debug
extern void print(char *msg);

void test() {
    initSwapStructures();

    // init semafori
    // devices
    for(int i = 0; i < DEVPERINT; i++) {
        flashSem[i] = 1;
    }
    readTerminalSemaphore = 1;
    writeTerminalSemaphore = 1;

    // processes
    masterSemaphore = 0;
    shellSemaphore = 0;

    /* ── 4. Lancia la shell (ASID 1 per convenzione) ────────────────────── */
    launchUProc(1 /* SHELL_ASID */);

    SYSCALL(PASSEREN, (int)&masterSemaphore,0,0);

    print("\n\nse mi vedi sei fritto!!\n\n");

    SYSCALL(TERMPROCESS,0,0,0);
}


/* -----------------------------------------------------------------------
 * launchUProc() - Crea e dispatcha un U-proc tramite NSYS1 (CREATEPROCESS)
 * ----------------------------------------------------------------------- */
void launchUProc(int asid) {
    state_t     uprocState;
    support_t  *sup = allocSupportStruct(asid);

    /* ── Stato iniziale del U-proc ──────────────────────────────────────── */
    /* PC: inizio sezione .text degli U-proc (vedi linker script crtsi.o) */
    uprocState.pc_epc  = UPROCSTARTADDR;

    /* SP: cima della stack page nel kuseg */
    uprocState.reg_sp  = 0xC0000000;

    /* User mode, tutti gli interrupt abilitati, PLT abilitato */
    uprocState.status  = MSTATUS_MPIE_MASK          /* interrupt abilitati al ritorno */
                       | MSTATUS_MIE_MASK;           /* interrupt globali on           */
    /* MPP = 00 => user mode al ritorno da eccezione */

    /* ASID nel campo EntryHi: identifica lo spazio di indirizzamento */
    uprocState.entry_hi = asid << ASIDSHIFT;

    /* Abilita il Local Timer del processore */
    uprocState.mie = MIE_ALL;

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
    /* Gli ASID vanno da 1 a UPROCMAX: usiamo asid-1 come indice. */
    support_t *sup = &supportStructPool[asid - 1];

    /* ── sup_asid ───────────────────────────────────────────────────────── */
    sup->sup_asid = asid;

    /* ── Pagina Table (sup_privatePgTbl) ────────────────────────────────── */
    /* Prime 31 entry: pagine .text/.data a partire da 0x80000000 */
    for (int i = 0; i < USERPGTBLSIZE - 1; i++) {
        sup->sup_privatePgTbl[i].pte_entryHI =
            (0x80000 + i) << VPNSHIFT | (asid << ASIDSHIFT);
        sup->sup_privatePgTbl[i].pte_entryLO =
            DIRTYON;        /* D=1, V=0, G=0 */
    }
    /* Entry 31: pagina stack (VPN 0xBFFFF -> top a 0xC0000000) */
    sup->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
        0xBFFFF << VPNSHIFT | (asid << ASIDSHIFT);
    sup->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO =
        DIRTYON;

    /* ── Contesti eccezione ─────────────────────────────────────────────── */
    /* PGFAULTEXCEPT (0): handler TLB → vmSupport.c */
    sup->sup_exceptContext[PGFAULTEXCEPT].pc     = (memaddr)pager;
    sup->sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sup->sup_exceptContext[PGFAULTEXCEPT].stackPtr =
        (memaddr)&sup->sup_stackTLB[499];   /* stack cresce verso il basso */

    /* GENERALEXCEPT (1): handler generale → sysSupport.c */
    sup->sup_exceptContext[GENERALEXCEPT].pc     = (memaddr)generalExceptionHandler;
    sup->sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sup->sup_exceptContext[GENERALEXCEPT].stackPtr =
        (memaddr)&sup->sup_stackGen[499];

    return sup;
}