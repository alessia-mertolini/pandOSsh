#include "./headers/vmSupport.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>
#include "./print.h"

//TODO controllare se 'swapPoolTable' e 'swapPoolSemaphore' devono essere static o no
//TODO capire se sono estetiche le variabili qui
//definizione della swapPoolTable utilizzando le strutture definite in types.h
//impostiamo la swapPoolTable come da specifiche
static swap_t swapPoolTable[POOLSIZE];
//definizione del semaforo
int swapPoolSemaphore;
//frame corrente della page table da sostituire
int currentFrame = 0;


void enterAtomicSession() {
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
}
void exitAtomicSession() {
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}

void releaseFramesOfProcess(int asid) {
    enterAtomicSession();
    for (int i = 0; i < POOLSIZE; i++) {
        if (swapPoolTable[i].sw_asid == asid) {
            //print("[RELEASE] frame=");
            char buf[2];
            buf[0] = i + '0';
            buf[1] = '\0';
            //print(buf);
            //print(" asid=");
            buf[0] = swapPoolTable[i].sw_asid + '0';
            buf[1] = '\0';
            //print(buf);
            //print("\n");
            if (swapPoolTable[i].sw_pte != NULL) {
                swapPoolTable[i].sw_pte->pte_entryLO &= ~VALIDON;
            }
            swapPoolTable[i].sw_asid  = -1;
            swapPoolTable[i].sw_pageNo = -1;
            swapPoolTable[i].sw_pte   = NULL;
        }
    }
    TLBCLR();
    exitAtomicSession();
}

// Restituisce l'indirizzo fisico di un frame della Swap Pool
memaddr frameAddressFunction(int frameIdx) {
    return (memaddr)(SWAP_POOL_START_ADDR + frameIdx * PAGESIZE);
}

void initSwapStructures() {
    // qui l'atomic session e' superflua perche' viene chiamata dal processo test
    // quando e' l'unico processo presente (si puo' mettere comunque, non fa danno)

    //semaforo pronto per accettare una P
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
void updateTLB(pteEntry_t *pageTable) {
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
void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int bitFlags) {
    ////print("[UPDATE_TLB] iniziata\n");
    pteEntry_t *pageTable = &(s->sup_privatePgTbl[pageNo]);
    pageTable->pte_entryLO = physicalAddr | bitFlags;
    updateTLB(pageTable); 
}

//algoritmo Round Robin per la sostituzione delle pagine
void pageReplacementAlgorithm(int *frameOut, int currentAsid) {
    // Prima cerca un frame libero
    for (int j = 0; j < POOLSIZE; j++) {
        if (swapPoolTable[j].sw_asid == -1) {
            *frameOut = j;
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
    
    // Tutti i frame appartengono al processo corrente — usa FIFO normale
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
        ////print("[PROBLEMI]\n");
        return -1;
    }
}

// page fault handler -> sezione 4.2 delle specifiche
void pager() {
    // otteniamo il puntatore alla support structure del processo
    support_t *support = (support_t *) SYSCALL(GETSUPPORTPTR, 0,0,0);
    state_t *exceptionState = &(support -> sup_exceptState[PGFAULTEXCEPT]);
    unsigned int cause = exceptionState -> cause;

    // ------ debug -------------
    // ESTREMIAMO IL PC PER VEDERE DOVE CRASHA
    unsigned int fault_pc = exceptionState->pc_epc;
    char buf[2];
    buf[0] = support->sup_asid + '0';
    buf[1] = '\0';
    //print("\n[CRITICAL-PAGER] Pager invocato da ASID=");
    //print(buf);
    //print("\n[CRITICAL-PAGER] Indirizzo istruzione (PC) che ha causato il fault = 0x");
    // ... stampa fault_pc in esadecimale (puoi usare la stessa logica che hai usato per pte_entryLO) ...
    char buf3[12];
    // stampa in esadecimale manuale
    for (int k = 7; k >= 0; k--) {
        int nibble = (fault_pc >> (k * 4)) & 0xF;
        buf3[7 - k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    buf3[8] = '\n';
    buf3[9] = '\0';
    //print(buf3);
    //print("\n");

    //TODO capire se la maschera applicata è corretta - cosa da debug
    //ricaviamo il codice di eccezione
    //unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
    unsigned int excCode = CAUSE_EXCCODE(cause);

    //TLBINVLDL := Load - TLBINVLDs := Store
    if (excCode >= 24 && excCode <= 28) {
        // Mutua esclusione sul semaforo della swapPool
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0,0);

        /* 1. Estraiamo il VPN puro azzerando i 12 bit bassi */
        unsigned int missing_vpn = (exceptionState->entry_hi & VPN_MASK) >> VPNSHIFT; // -> otteniamo qualcosa del tipo 0x80000...0xBFFFF

        /* 2. Traduciamo il VPN nell'indice logico 'p' (da 0 a 31) */
        int p;
        if (missing_vpn == STACK_PAGE_VPN) {
            p = 31;
        } else if (missing_vpn >= 0x80000 && missing_vpn <= 0x8001E) { 
            p = (int)(missing_vpn - 0x80000); // Pagine di codice (0-30)
        } else {
            // ERRORE: Segmentation Fault!
            // Dobbiamo rilasciare il semaforo prima di ucciderla!
            SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0);
            programTrapSupportHandler(support);
            return;
        }
        
        //algoritmo di prelevamento pagine round-robin
        int i; //i="Indice del frame nella swap pool". E' lo spazio fisico scelto per ospitare la pagina "p"
        pageReplacementAlgorithm(&i, support->sup_asid);

        // dopo aver calcolato p
        //print("[DEBUG] Pager: pageNo=");
        char buf[3];
        buf[0] = '0' + p / 10;
        buf[1] = '0' + p % 10;
        buf[2] = '\0';
        //print(buf);
        //print(" frame=");
        buf[0] = '0' + i / 10;
        buf[1] = '0' + i % 10;
        //print(buf);
        //print("\n");

        //punto sette - determiniamo se il frame i-esimo è occupato
        if(swapPoolTable[i].sw_asid != -1) {
            //abbiamo definito con il valore di "-1" un frame vuoto
            //dunque qui il frame "i-esimo" è attualmente occupato
            int oldProcAsid = swapPoolTable[i].sw_asid;
            int oldProcVpn = swapPoolTable[i].sw_pageNo; //la pagina che verrà sostituita da "p"
            pteEntry_t *oldProcPte = swapPoolTable[i].sw_pte;
            
            enterAtomicSession();

            oldProcPte->pte_entryLO &= ~VALIDON; //marchiamo il bit del frame contenuto nella page table come invalido 8a)

            //versione DEBUG, facciamo un ripulisti della TLB, questa parte di codice dovrà essere raffinata cercando la pagina
            //invece di ripulire, dobbiamo chiedere se la pagina è in TLB e ne caso rimuoverla perché "out of date"
            updateTLB(oldProcPte); //8b)

            exitAtomicSession();

            //8c)
            int writeStatus = readOrWriteToFlash(oldProcAsid, oldProcVpn, i, FLASHWRITE);
            if (writeStatus != OK) {
                SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0); // RILASCIA IL SEMAFORO!
                programTrapSupportHandler(support);
                return;
            }
        }

        //9
        //print("[DEBUG] Pager: Leggo dal Flash Device: ");
        char devNo[2];
        devNo[0] = (support->sup_asid - 1) + '0';
        devNo[1] = '\0';
        //print(devNo);
        //print("\n");
        int readStatus = readOrWriteToFlash(support->sup_asid, p, i, FLASHREAD);
        //print("[DEBUG] Pager: Flash Device letto con successo!\n");
        if (readStatus != OK) {
            SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0); // RILASCIA IL SEMAFORO!
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
        enterAtomicSession();
        unsigned int physicalAddr = frameAddressFunction(i);
        updatePTE(support, p, physicalAddr, VALIDON|DIRTYON);
        exitAtomicSession();
        unsigned int entryLO = support->sup_privatePgTbl[p].pte_entryLO;
        char buf3[12];
        // stampa in esadecimale manuale
        for (int k = 7; k >= 0; k--) {
            int nibble = (entryLO >> (k * 4)) & 0xF;
            buf3[7 - k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        }
        buf3[8] = '\n';
        buf3[9] = '\0';
        //print("[PTE] pte_entryLO=0x");
        //print(buf3);

        //13
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0,0);

        LDST(exceptionState);
    }
    else { // TLB-Modification exception
        programTrapSupportHandler(support);
        return;
    }
}

// * NON UTILIZZATA
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