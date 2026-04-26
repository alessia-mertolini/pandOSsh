#include "./headers/vmSupport.h"
#include <uriscv/liburiscv.h>
#include "exceptions.h"

//TODO controllare se 'swapPoolTable' e 'swapPoolSemaphore' devono essere static o no
//TODO capire se sono estetiche le variabili qui
//definizione della swapPoolTable utilizzando le strutture definite in types.h
//impostiamo la swapPoolTable come da specifiche
static swap_t swapPoolTable[POOLSIZE];
//definizione del semaforo
static int swapPoolSemaphore;
//frame corrente della page table da sostituire
static int currentFrame = 0;

void enterAtomicSession() {
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
}
void exitAtomicSession() {
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}

void initSwapStructures() {
    //semaforo pronto per accettare una P
    swapPoolSemaphore = 1;
    for (int i = 0; i < POOLSIZE; i++) {
        swapPoolTable[i].sw_asid = -1;
        swapPoolTable[i].sw_pageNo = -1;
        swapPoolTable[i].sw_pte = NULL;
    }
}

void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int bitFlags) {
    enterAtomicSession();
    pteEntry_t *pageTable = &(s->sup_privatePgTbl[pageNo]);
    pageTable->pte_entryLO = physicalAddr | bitFlags;
    TLBCLR(); //ancora versione debug, poi qui deve venire inserita la versione potente
}

//algoritmo Round Robin per la sostituzione delle pagine
void pageReplacementAlgorithm(int *i) {
    *i = currentFrame;
    currentFrame = (currentFrame + 1) % POOLSIZE;
}
//vpn = p e frameNumber = i
int writeToFlash(int asid, unsigned int vpn, int frameNumber) {
    return 0;
}
int readToFlash(int asid, unsigned int vpn, int frameNumber) {
    return 0;
}

//sezione 4.2 delle specifiche
void pager() {
    //otteniamo il puntatore alla support structure del processo
    support_t *support = (support_t *) SYSCALL(GETSUPPORTPTR, 0,0,0);
    state_t *exceptionState = &(support -> sup_exceptState[0]);
    unsigned int cause = exceptionState -> cause;

    //TODO capire se la maschera applicata è corretta - cosa da debug
    //ricaviamo il codice di eccezione
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
    //TLBINVLDL := Load - TLBINVLDs := Store
    if (excCode == TLBINVLDL || excCode == TLBINVLDS) {
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0,0);

        //punto cinque, preleviamo la "missing page number", ovvero la pagina che è appena stata richiesta
        unsigned int p = (((exceptionState->entry_hi) & GETPAGENO) >> VPNSHIFT); //p="virtual page number 'vpn'"

        //algoritmo di prelevamento pagine round-robin
        int i; //i="Indice del frame nella swap pool". E' lo spazio fisico scelto per ospitare la pagina "p"
        pageReplacementAlgorithm(&i);

        //punto sette - determiniamo se il frame i-esimo è occupato
        if(swapPoolTable[i].sw_asid != -1) { //abbiamo definito con il valore di "-1" un frame vuoto
            //dunque qui il frame "i-esimo" è attualmente occupato
            int oldProcAsid = swapPoolTable[i].sw_asid;
            int oldProcVpn = swapPoolTable[i].sw_pageNo; //la pagina che verrà sostituita da "p"
            pteEntry_t *oldProcPte = swapPoolTable[i].sw_pte;
            enterAtomicSession();
            oldProcPte->pte_entryLO &= ~VALIDON; //marchiamo il bit del frame contenuto nella page table come invalido 8a)

            //TODO raffinare da specifiche
            //versione DEBUG, facciamo un ripulisti della TLB, questa parte di codice dovrà essere raffinata cercando la pagina
            //invece di ripulire, dobbiamo chiedere se la pagina è in TLB e ne caso rimuoverla perché "out of date"
            TLBCLR(); //8b)

            exitAtomicSession();

            //8c)
            int writeStatus = writeToFlash(oldProcAsid, oldProcVpn, i);
            if (writeStatus != 0) {
                programTrapHandler();
                return;
            }
            //9
            int readStatus = readToFlash(support->sup_asid, p, i);
            if (readStatus != 0) {
                programTrapHandler();
                return;
            }

            //10
            swapPoolTable[i].sw_asid = support -> sup_asid;
            swapPoolTable[i].sw_pageNo = p; //memorizzazione del vpn
            //salvataggio di un puntatore diretto alla specifica Page Table Entry di quella pagina "p"
            //all'interno della pageTable del processo corrente
            swapPoolTable[i].sw_pte = &(support -> sup_privatePgTbl[p]);

            //11 + 12
            unsigned int physicalAddr = (SWAP_POOL_START_ADDR + (i * PAGESIZE));
            updatePTE(support, p, physicalAddr, VALIDON|DIRTYON);

            SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0);
            LDST(exceptionState);
        }
    }
    else {
        programTrapHandler();
        return;
    }
}
