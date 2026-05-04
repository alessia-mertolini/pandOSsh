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
static void initSupportStruct(int asid);
void launchUProc(int asid);
static support_t *allocSupportStruct(int asid);


// ---------------- DEBUG: print di fase 2 ----------------
#include <uriscv/liburiscv.h>

typedef unsigned int devregtr;

#define TERM0ADDR 0x10000254
#define TERMSTATMASK 0xFF
#define MAXSEM   20
#define NOLEAVES 4 /* number of leaves of p8 process tree */

extern int processCount;

int sem_term_mut = 1,              /* for mutual exclusion on terminal */
    s[MAXSEM + 1],                 /* semaphore array */
    sem_testsem             = 0,   /* for a simple test */
    sem_startp2             = 0,   /* used to start p2 */
    sem_endp2               = 0,   /* used to signal p2's demise */
    sem_endp3               = 0,   /* used to signal p3's demise */
    sem_blkp4               = 1,   /* used to block second incaration of p4 */
    sem_synp4               = 0,   /* used to allow p4 incarnations to synhronize */
    sem_endp4               = 0,   /* to signal demise of p4 */
    sem_endp5               = 0,   /* to signal demise of p5 */
    sem_endp8               = 0,   /* to signal demise of p8 */
    sem_endcreate[NOLEAVES] = {0}, /* for a p8 leaf to signal its creation */
    sem_blkp8               = 0,   /* to block p8 */
    sem_blkp9               = 0;   /* to block p9 */

void print(char *msg) {

    char     *s       = msg;
    devregtr *base    = (devregtr *)(TERM0ADDR);
    devregtr *command = base + 3;
    devregtr  status;

    SYSCALL(PASSEREN, (int)&sem_term_mut, 0, 0); /* P(sem_term_mut) */
    while (*s != EOS) {
        devregtr value = PRINTCHR | (((devregtr)*s) << 8);
        status         = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & TERMSTATMASK) != RECVD) {
            PANIC();
        }
        s++;
    }
    SYSCALL(VERHOGEN, (int)&sem_term_mut, 0, 0); /* V(sem_term_mut) */
}

// ---------------------------

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
    launchUProc(0 /* SHELL_ASID */);

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
    uprocState.pc_epc  = 0x800000B0;

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