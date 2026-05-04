#include "./headers/vmSupport.h"

//TODO controllare se 'swapPoolTable' e 'swapPoolSemaphore' devono essere static o no
//TODO capire se sono estetiche le variabili qui
//definizione della swapPoolTable utilizzando le strutture definite in types.h
//impostiamo la swapPoolTable come da specifiche
static swap_t swapPoolTable[POOLSIZE];
//definizione del semaforo
static int swapPoolSemaphore;
//frame corrente della page table da sostituire
static int currentFrame = 0;
//Array dei semafori dei device - 8 flash device
int flashSem[DEVPERINT];

void enterAtomicSession() {
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
}
void exitAtomicSession() {
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}

//Restituisce l'indirizzo fisico di un frame della Swap Pool
static memaddr frameAddressFunction(int frameIdx) {
    return (memaddr)(SWAP_POOL_START_ADDR + frameIdx * PAGESIZE);
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

/*
  @param bitFlags: puo' essere VALIDON | DIRTYON | GLOBALON
  @param physicalAddr: indirizzo fisico del frame deve essere gia' nel formato corretto per
                       pte_entryLO, cioe' PFN << PFNSHIFT
  @return: aggiorna la pagetable e il tlb con la pagina e i flag passati
*/
void updateTLB(support_t *s, int pageNo, unsigned int physicalAddr, int bitFlags) {
    enterAtomicSession();

    // * 2 - poi aggiorniamo il TLB
    // unsigned int oldEntryHi = getENTRYHI();
    // setENTRYHI(pageTable->pte_entryHI);

    // ricerca nel TLB
    // TLBP();
    // if(getINDEX() >= 0) {
    //   // Se c'è, aggiorna subito il registro fisico con il nuovo EntryLo (V, D, G e PFN)
    //   setENTRYLO(s->sup_privatePgTbl[pageNo].pte_entryLO);
    //   TLBWI();
    // }

    // ripristino
    // setENTRYHI(oldEntryHi);

    //TODO: sostituire la riga seguente con il frammento di codice precedente
    TLBCLR();
    exitAtomicSession();
}

void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int bitFlags) {
    enterAtomicSession();
    pteEntry_t *pageTable = &(s->sup_privatePgTbl[pageNo]);
    pageTable->pte_entryLO = physicalAddr | bitFlags;
    //TODO DOPO DEBUG USARE updateTLB()
    TLBCLR(); //ancora versione debug, poi qui deve venire inserita la versione potente
    exitAtomicSession();
}

//algoritmo Round Robin per la sostituzione delle pagine
void pageReplacementAlgorithm(int *i) {
    *i = currentFrame;
    currentFrame = (currentFrame + 1) % POOLSIZE;
}
//vpn = p = page number e frameNumber = i
int readOrWriteToFlash(int asid, int vpn, int frameNumber, int command) {
    memaddr frameAddress = frameAddressFunction(frameNumber);
    //Asid va da 1 - 8, mentre i dispositivi vanno da 0 - 7
    int devNo = asid - 1;
    //Indirizzo base del device register del flash
    dtpreg_t *devReg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, devNo);

    //Acquisisce il semaforo di mutua esclusione sul device
    SYSCALL(PASSEREN, (int)&flashSem[devNo], 0, 0);
    //Passo 1: scrive DATA0 con l'indirizzo fisico del frame
    devReg->data0 = frameAddress;

    //Passo 2: avvia l'operazione tramite NSYS5 (DoIO)
    //Il campo COMMAND contiene: blocco (byte alti) | comando (byte basso)
    unsigned int cmd = (vpn << 8) | command;
    unsigned int status = SYSCALL(DOIO, (int)&devReg->command, cmd, 0);

    //Rilascia il semaforo del device
    SYSCALL(VERHOGEN, (int)&flashSem[devNo], 0, 0);

    if((status & 0xFF) == 1) {
        return OK; //operazione effettuata con successo
    } else {
        return -1;
    }
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
        if(swapPoolTable[i].sw_asid != -1) {
            //abbiamo definito con il valore di "-1" un frame vuoto
            //dunque qui il frame "i-esimo" è attualmente occupato
            int oldProcAsid = swapPoolTable[i].sw_asid;
            int oldProcVpn = swapPoolTable[i].sw_pageNo; //la pagina che verrà sostituita da "p"
            pteEntry_t *oldProcPte = swapPoolTable[i].sw_pte;
            enterAtomicSession();
            oldProcPte->pte_entryLO &= ~VALIDON; //marchiamo il bit del frame contenuto nella page table come invalido 8a)

            //TODO DOPO DEBUG USARE updateTLB()
            //versione DEBUG, facciamo un ripulisti della TLB, questa parte di codice dovrà essere raffinata cercando la pagina
            //invece di ripulire, dobbiamo chiedere se la pagina è in TLB e ne caso rimuoverla perché "out of date"
            TLBCLR(); //8b)

            exitAtomicSession();

            //8c)

            //int readOrWriteToFlash(int asid, int vpn, int frameNumber, int command);

            int writeStatus = readOrWriteToFlash(oldProcAsid, oldProcVpn, i, FLASHWRITE);
            //int writeStatus = writeToFlash(oldProcAsid, oldProcVpn, i);
            if (writeStatus != OK) {
                programTrapSupportHandler(support);
                return;
            }
        }

        //9
        int readStatus = readOrWriteToFlash(support->sup_asid, p, i, FLASHREAD);
        if (readStatus != OK) {
            programTrapSupportHandler(support);
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
    else {
        programTrapSupportHandler(support);
        return;
    }
}

//TODO assegnare ASID values
void initPageTable(support_t *s) {
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

        // attiviamo solo D e G
        s->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
    }
}