/* =========================================================================
 * vmSupport.c  –  Phase 3: TLB exception handler (Pager) + Flash I/O
 * ========================================================================= */

#include "headers/vmSupport.h"
#include "headers/sysSupport.h"
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>

/* --- Costanti ------------------------------------------------------------ */
#define SWAP_POOL_SIZE   (2 * UPROCMAX)
#define SWAP_POOL_START  0x20020000          /* dopo 32 frame di OS code    */
#define FLASH_READ       3
#define FLASH_WRITE      4
#define FLASH_OK         1                   /* status code operazione OK   */

/* --- Struttura di una entry della Swap Pool table ------------------------ */
typedef struct {
    int       asid;    /* ASID del processo occupante; -1 = libero          */
    int       vpn;     /* VPN della pagina logica occupante                 */
    pteEntry_t *pte;   /* puntatore alla Page Table entry del proprietario  */
} swapEntry_t;

/* --- Dati di modulo ------------------------------------------------------- */
static swapEntry_t swapPool[SWAP_POOL_SIZE];  /* Swap Pool table            */
static int         frameIndex = -1;           /* cursore FIFO               */

/* Semaforo di mutua esclusione sulla Swap Pool table (init a 1 in initProc) */
int swapPoolSem;

/* Semafori di mutua esclusione per i device I/O (un sem per flash device)  */
/* Dichiarati qui ed esportati; inizializzati a 1 in initProc.c             */
int flashSem[DEVPERINT];   /* 8 flash device                               */
int termReadSem;           /* terminale – sotto-device RX                  */
int termWriteSem;          /* terminale – sotto-device TX                  */

/* =========================================================================
 * initSwapStructs
 *   Inizializza la Swap Pool table (chiamata da test/initProc).
 * ========================================================================= */
void initSwapStructs(void) {
    for (int i = 0; i < SWAP_POOL_SIZE; i++)
        swapPool[i].asid = -1;   /* frame libero                           */
}

/* =========================================================================
 * pickFrame  [Section 5.4]
 *   Algoritmo FIFO: restituisce l'indice del frame da usare.
 *   Semplice incremento modulo dimensione della pool.
 * ========================================================================= */
static int pickFrame(void) {
    frameIndex = (frameIndex + 1) % SWAP_POOL_SIZE;
    return frameIndex;
}

/* =========================================================================
 * frameAddress
 *   Restituisce l'indirizzo fisico di un frame della Swap Pool.
 * ========================================================================= */
static memaddr frameAddress(int frameIdx) {
    return (memaddr)(SWAP_POOL_START + frameIdx * PAGESIZE);
}

/* =========================================================================
 * flashIO  [Section 5.1]
 *   Esegue un'operazione di lettura o scrittura su un flash device.
 *   asid    : ASID del processo [1..8] → device index [0..7]
 *   pageNo  : numero di blocco (0..31) sul flash
 *   physAddr: indirizzo fisico del frame RAM sorgente/destinazione
 *   command : FLASH_READ o FLASH_WRITE
 *   Ritorna 0 se OK, -1 in caso di errore.
 * ========================================================================= */
static int flashIO(int asid, int pageNo, memaddr physAddr, int command) {
    /* Il flash device del processo ha index (asid - 1)                      */
    int devNo = asid - 1;

    /* Indirizzo base del device register del flash [Section 5.1]            */
    dtpreg_t *devReg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, devNo);

    /* Acquisisce il semaforo di mutua esclusione sul device                  */
    SYSCALL(PASSEREN, (int)&flashSem[devNo], 0, 0);

    /* Passo 1: scrive DATA0 con l'indirizzo fisico del frame                 */
    devReg->data0 = physAddr;

    /* Passo 2: avvia l'operazione tramite NSYS5 (DoIO)                       */
    /* Il campo COMMAND contiene: blocco (byte alti) | comando (byte basso)   */
    unsigned int cmd = (pageNo << 8) | command;
    unsigned int status = SYSCALL(DOIO, (int)&devReg->command, cmd, 0);

    /* Rilascia il semaforo del device                                        */
    SYSCALL(VERHOGEN, (int)&flashSem[devNo], 0, 0);

    /* Controlla lo status: FLASH_OK (1) = operazione completata con successo */
    return ((status & 0xFF) == FLASH_OK) ? 0 : -1;
}

/* =========================================================================
 * updateTLB  [Section 5.2]
 *   Aggiorna la cache TLB in modo atomico dopo aver modificato una PTE.
 *   Usa TLBP per verificare se l'entry è in cache; se sì, la sovrascrive
 *   con TLBWI.  Atomicità garantita disabilitando gli interrupt [Section 5.3].
 * ========================================================================= */
static void updateTLB(pteEntry_t *pte) {
    /* Disabilita gli interrupt per garantire atomicità [Section 5.3]        */
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);

    setENTRYHI(pte->entryHI);
    TLBP();   /* cerca l'entry nel TLB; se trovata, Index.P = 0             */

    if ((getINDEX() & PRESENTFLAG) == 0) {
        /* L'entry è in TLB: aggiornala con TLBWI                           */
        setENTRYHI(pte->entryHI);
        setENTRYLO(pte->entryLO);
        TLBWI();
    }

    /* Riabilita gli interrupt                                               */
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}

/* =========================================================================
 * pager  [Section 4.2]
 *   TLB exception handler del Support Level.
 *   Gestisce i page fault (TLB-Invalid) per i U-proc.
 *   Esegue: eviction opzionale, lettura dal backing store, aggiornamento
 *   della Page Table e del TLB.
 * ========================================================================= */
void pager(void) {
    /* Passo 1: ottiene il puntatore alla Support Structure [Section 4.2 #1] */
    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Passo 2: recupera lo stato salvato al momento dell'eccezione TLB      */
    state_t *excState = &sup->sup_exceptState[PGFAULTEXCEPT];
    unsigned int cause = excState->cause & GETEXECCODE;

    /* Passo 3: TLB-Modification → tratta come program trap [Section 4.2 #3]*/
    unsigned int excCode = (excState->cause & GETEXECCODE) >> CAUSESHIFT;
    if (excCode == 1) {   // 1 = TLB-Modification (Mod)
        programTrapSupportHandler(sup);
        return;
    }

    /* Passo 4: acquisisce la mutua esclusione sulla Swap Pool               */
    SYSCALL(PASSEREN, (int)&swapPoolSem, 0, 0);

    /* Passo 5: determina il numero di pagina mancante dall'EntryHi salvato  */
    int missingVPN = (excState->entry_hi & GETPAGENO) >> VPNSHIFT;

    /* Individua l'indice nella Page Table del processo                      */
    /* Le prime 31 entry sono .text/.data (VPN 0x80000..0x8001E);
       l'ultima (entry 31) è la stack page (VPN 0xBFFFF)                    */
    int pageIdx;
    if (missingVPN == 0xBFFFF)
        pageIdx = 31;       /* stack page                                   */
    else
        pageIdx = missingVPN - 0x80000;  /* pagine .text/.data              */

    /* Passo 6: sceglie un frame con l'algoritmo FIFO [Section 5.4]          */
    int frameIdx = pickFrame();
    memaddr frameAddr = frameAddress(frameIdx);

    /* Passo 7–8: se il frame è occupato, esegue l'eviction                 */
    if (swapPool[frameIdx].asid != -1) {
        swapEntry_t *victim = &swapPool[frameIdx];

        /* Passo 8a: marca la PTE della vittima come non valida (V=0)        */
        /* + aggiorna il TLB in modo atomico [Section 5.3]                  */
        setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
        victim->pte->entryLO &= ~VALIDON;   /* spegne il bit V              */
        updateTLB(victim->pte);             /* invalida l'entry in TLB      */
        setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

        /* Passo 8c: scrive il frame sul backing store della vittima         */
        int writeErr = flashIO(victim->asid,
                               victim->vpn == 0xBFFFF ? 31 : victim->vpn - 0x80000,
                               frameAddr,
                               FLASH_WRITE);
        if (writeErr != 0) {
            /* Errore di I/O: rilascia il mutex e termina il processo        */
            SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
            programTrapSupportHandler(sup);
            return;
        }
    }

    /* Passo 9: legge la pagina p dal backing store del processo corrente    */
    int readErr = flashIO(sup->sup_asid, pageIdx, frameAddr, FLASH_READ);
    if (readErr != 0) {
        SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
        programTrapSupportHandler(sup);
        return;
    }

    /* Passo 10: aggiorna la Swap Pool table                                 */
    swapPool[frameIdx].asid = sup->sup_asid;
    swapPool[frameIdx].vpn  = missingVPN;
    swapPool[frameIdx].pte  = &sup->sup_privatePgTbl[pageIdx];

    /* Passo 11–12: aggiorna la PTE del processo e il TLB in modo atomico   */
    pteEntry_t *pte = &sup->sup_privatePgTbl[pageIdx];
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
    pte->entryLO = (frameAddr & 0xFFFFF000) | VALIDON | DIRTYON;  // PFN + V + D
    updateTLB(pte);
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

    /* Passo 13: rilascia la mutua esclusione sulla Swap Pool                */
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);

    /* Passo 14: restituisce il controllo al U-proc per ripetere l'istruzione*/
    LDST(excState);
}