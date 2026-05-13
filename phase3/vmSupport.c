#include "./headers/vmSupport.h"

//definizione della swapPoolTable utilizzando le strutture definite in types.h
//impostiamo la swapPoolTable come da specifiche
static swap_t swapPoolTable[POOLSIZE];
//definizione del semaforo
int swapPoolSemaphore;
//frame corrente della page table da sostituire
static int currentFrame = 0;

static void updateTLB(pteEntry_t *pageTable) ;

extern int flashSem[DEVPERINT]; // importata da initProc.h

// Funzione che permette di entrare in sessione atomica disabilitando gli interrupt
static void enterAtomicSession() {
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
}
// Antidoto della funzione soprastante
static void exitAtomicSession() {
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}

void releaseFramesOfProcess(int asid) {
    for (int i = 0; i < POOLSIZE; i++) {
        if (swapPoolTable[i].sw_asid == asid) {
            if (swapPoolTable[i].sw_pte != NULL) {
                swapPoolTable[i].sw_pte->pte_entryLO = DIRTYON;
            }
            enterAtomicSession();
            updateTLB(swapPoolTable[i].sw_pte);
            exitAtomicSession();
            swapPoolTable[i].sw_asid  = -1;
            swapPoolTable[i].sw_pageNo = -1;
            swapPoolTable[i].sw_pte   = NULL;
        }
    }
}

// Restituisce l'indirizzo fisico di un frame della Swap Pool
memaddr frameAddressFunction(int frameIdx) {
    return (memaddr)(SWAP_POOL_START_ADDR + frameIdx * PAGESIZE);
}

void initSwapStructures() {
    // qui l'atomic session e' superflua perche' viene chiamata dal processo test
    // quando e' l'unico processo presente
    // tecnicamente si potrebbe inserire, ma non è necessaria

    //impostiamo il semaforo pronto per accettare una P
    swapPoolSemaphore = 1;
    for (int i = 0; i < POOLSIZE; i++) {
        swapPoolTable[i].sw_asid = -1;
        swapPoolTable[i].sw_pageNo = -1;
        swapPoolTable[i].sw_pte = NULL;
    }
}

// * va chiamato in atomic session
/*
  @param bitFlags: puo' essere VALIDON | DIRTYON | GLOBALON
  @param physicalAddr: indirizzo fisico del frame deve essere gia' nel formato
  corretto per pte_entryLO, cioe' PFN << PFNSHIFT
  @return: aggiorna la pagetable e il tlb con la pagina e i flag passati
*/
static void updateTLB(pteEntry_t *pageTable) {
    // mi salvo il processo che ha causato il page fault
    unsigned int oldEntryHi = getENTRYHI();
    setENTRYHI(pageTable->pte_entryHI); // pagina vittima passata come parametro

    //ricerca nel TLB se esiste la pagina vittima
    TLBP(); //consulta entryHI
    if ((getINDEX() & PRESENTFLAG) == 0) {
        // Se c'è, aggiorna subito il registro fisico con il nuovo EntryLo (V, D, G e PFN)
        setENTRYLO(pageTable->pte_entryLO);
        TLBWI();
    }

    //ripristino del processo corrente
    setENTRYHI(oldEntryHi);
}

// update PageTable Entry
// * physicalAddr deve essere in formato 0x00000000 (indirizzo completo) non 0x00000 (PFN)
static void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int bitFlags) {
    pteEntry_t *pageTable = &(s->sup_privatePgTbl[pageNo]);
    pageTable->pte_entryLO = physicalAddr | bitFlags;
    updateTLB(pageTable); 
}

//algoritmo Round Robin per la sostituzione delle pagine
static void pageReplacementAlgorithm(int *frameOut, int currentAsid) {
    // Sanity check
    if (currentFrame < 0 || currentFrame >= POOLSIZE) {
        currentFrame = 0;  // reset di sicurezza
    }
    // Prima cerca un frame libero - ottimizzazione
    for (int j = 0; j < POOLSIZE; j++) {
        if (swapPoolTable[j].sw_asid == -1) {
            *frameOut = j;
            currentFrame = (j + 1) % POOLSIZE;
            return;
        }
    }

    // Round-robin ma salta i frame del processo corrente
    int start = currentFrame;
    do {
        int candidate = currentFrame;
        currentFrame = (currentFrame + 1) % POOLSIZE;
        if (swapPoolTable[candidate].sw_asid != currentAsid) {
            *frameOut = candidate;
            return;
        }
    } while (currentFrame != start);
    
    // Caso: tutti i frame appartengono al processo corrente viene usata FIFO normale
    *frameOut = currentFrame;
    currentFrame = (currentFrame + 1) % POOLSIZE;
}

/*
    Commento per Gabbo: ho cambiato vpn in pageNo perche' si trattano effettivamente di due cose diverse
    vpn e' un numero del tipo 0x80000...0xbffff mentre qua vogliamo un pageNo ovvero un numero tipo 0...31
*/
//p = page number e frameNumber = i
int readOrWriteToFlash(int asid, int pageNo, int frameNumber, int command) {
    unsigned int frameAddress = frameAddressFunction(frameNumber);
    // Asid va da 1 - 8, mentre i dispositivi vanno da 0 - 7
    int devNo = asid - 1;
    // Indirizzo base del device register del flash
    dtpreg_t *devReg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, devNo);

    // Acquisisce il semaforo di mutua esclusione sul device
    SYSCALL(PASSEREN, (int)&flashSem[devNo], 0, 0);
    // Passo 1: scrive DATA0 con l'indirizzo fisico del frame
    devReg->data0 = frameAddress;

    // Passo 2: avvia l'operazione tramite NSYS5 (DoIO)
    // Il campo COMMAND contiene: blocco (byte alti) | comando (byte basso)

    // ! Sembra corretto ma teniamolo d'occhio
    unsigned int cmd = (pageNo << 8) | command;
    unsigned int status = SYSCALL(DOIO, (int)&devReg->command, cmd, 0);

    //Rilascia il semaforo del device
    SYSCALL(VERHOGEN, (int)&flashSem[devNo], 0, 0);

    if((status & 0xFF) == 1) {
        return OK; //operazione effettuata con successo
    } else {
        return -1;
    }
}

void pager() {
    // otteniamo il puntatore alla support structure del processo
    support_t *support = (support_t *) SYSCALL(GETSUPPORTPTR, 0,0,0);
    state_t *exceptionState = &(support -> sup_exceptState[PGFAULTEXCEPT]);
    unsigned int cause = exceptionState -> cause;

    //ricaviamo il codice tramite macro gia' precedentemente usata
    unsigned int excCode = CAUSE_EXCCODE(cause);

    if (excCode >= 24 && excCode <= 28) {
        // Mutua esclusione sul semaforo della swapPool
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0,0);

        //Estraiamo il VPN puro azzerando i 12 bit bassi
        unsigned int missing_vpn = (exceptionState->entry_hi & VPN_MASK) >> VPNSHIFT; // -> otteniamo qualcosa del tipo 0x80000...0xBFFFF

        //Traduciamo il VPN nell'indice logico 'p' (da 0 a 31)
        int p;
        if (missing_vpn == STACK_PAGE_VPN) {
            p = 31;
        } else if (missing_vpn >= 0x80000 && missing_vpn <= 0x8001E) { 
            p = (int)(missing_vpn - 0x80000); // Pagine di codice (0-30)
        } else {
            // ERRORE: Segmentation Fault!
            // Rilascia il semaforo prima di terminare
            SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0);
            programTrapSupportHandler(support, 0);
            return;
        }
        
        //algoritmo di prelevamento pagine round-robin
        int i; //i="Indice del frame nella swap pool". E' lo spazio fisico scelto per ospitare la pagina "p"
        pageReplacementAlgorithm(&i, support->sup_asid);

        //punto sette - determiniamo se il frame i-esimo è occupato
        if(swapPoolTable[i].sw_asid != -1) {
            //abbiamo definito con il valore di "-1" un frame vuoto
            //dunque qui il frame "i-esimo" è attualmente occupato
            int oldProcAsid = swapPoolTable[i].sw_asid;
            int oldProcVpn = swapPoolTable[i].sw_pageNo; //la pagina che verrà sostituita da "p"
            pteEntry_t *oldProcPte = swapPoolTable[i].sw_pte;

            enterAtomicSession();

            //8a)
            oldProcPte->pte_entryLO &= ~VALIDON; //marchiamo il bit del frame contenuto nella page table come invalido

            //8b)
            //dobbiamo chiedere se la pagina è in TLB e ne caso rimuoverla perché "out of date"
            updateTLB(oldProcPte);

            exitAtomicSession();

            //8c)
            int writeStatus = readOrWriteToFlash(oldProcAsid, oldProcVpn, i, FLASHWRITE);
            if (writeStatus != OK) {
                /* Tiene ancora swapPoolSemaphore: programTrapSupportHandler lo rilascia. */
                programTrapSupportHandler(support, 1);
                return;
            }
        }

        //9
        int readStatus = readOrWriteToFlash(support->sup_asid, p, i, FLASHREAD);
        if (readStatus != OK) {
            /* Tiene ancora swapPoolSemaphore: programTrapSupportHandler lo rilascia. */
            programTrapSupportHandler(support, 1);
            return;
        }

        //10
        swapPoolTable[i].sw_asid = support -> sup_asid;
        swapPoolTable[i].sw_pageNo = p; //memorizzazione del vpn
        //salvataggio di un puntatore diretto alla specifica Page Table Entry di quella pagina "p"
        //all'interno della pageTable del processo corrente
        swapPoolTable[i].sw_pte = &(support -> sup_privatePgTbl[p]);

        //11 + 12
        enterAtomicSession();
        unsigned int physicalAddr = frameAddressFunction(i);
        updatePTE(support, p, physicalAddr, VALIDON|DIRTYON);
        exitAtomicSession();

        //13
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0);
        LDST(exceptionState);
    }
    else { // TLB-Modification exception
        programTrapSupportHandler(support, 0);
        return;
    }
}