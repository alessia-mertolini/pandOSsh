/* =========================================================================
 * exceptions.c  –  Phase 2: Nucleus Exception & SYSCALL Handlers

   NOTA:
    - LDST -> usato per operazioni di get (casi NON bloccanti)
    - scheduler -> usato per operazioni bloccanti 
 * ========================================================================= */

#include "./headers/exceptions.h"
#include "headers/constants.h"
#include "headers/scheduler.h"
#include <uriscv/const.h>
#include <uriscv/liburiscv.h>

//--- Prototipi static delle SYSCALL

/* Crea un processo in modo tale da essere un processo progenito del chimante
 * Se il processo non può essere creato per mancanza di risorse
 * si titorna il valore -1 nel reg a0 del chiamante, altrimenti
 * si ritorna nel reg a0 il pd del processo appena creato */
static void createProcess(state_t *savedState);
/* Questo servizio causa la morte del processo corrente, o di un altro processo
 * Inoltre tutta la progenie del processo è terminata.
 * L'esecuzione di questa istruzione non termina finché tutta la progenie
 * non è terminata, successivamente, lo scheduler viene chiamato */
static void terminateProcess(state_t *savedState);
/* Questa syscall quando richiesta esegue una P su un semaforo 
 * l'indirizzo del semaforo è posto nel registro a1*/
static void passeren(state_t *savedState);
/* Questa syscall quando richiesta esegue una V su un semaforo 
 * l'indirizzo del semaforo è posto nel registro a1*/
static void verhogen(state_t *savedState);
/* Questo servizio, quando chiamato permette si posizionare il valore
 * contenuto nel reg a3 all'indirizzo di memoria del reg a2
 * permettendo quindi un Input/Output. 
 * Il processo chiamante la DoIO è bloccato fino a quando l'I/O non è completato */
static void doIO(state_t *savedState);
/* Richiede il processor time accumulato in microsecondi dal processore*/
static void getCPUTime(state_t *savedState);
/* Questa syscall esegue una P sul semafor pseudo-clock del nucleo
 * su questo semaforo viene eseguita una V ogni 100 millisecondi 
 * da parte del nucelo*/
static void waitForClock(state_t *savedState);
/* Questo servizio richiede il puntatore alla support struct del processo corrente*/
static void getSupportData(state_t *savedState);
static void getProcessID(state_t *savedState);
/* Qusto servizio causa il processo chiamante a lasciare volontariamente la CPU*/
static void yield(state_t *savedState);

// Funzione ausiliaria per la corretta compilazione da parte di GCC
void *memcpy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* -------------
    Gestione degli eventi
   --------------- */

// Gestore delle eccezioni
void exceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    unsigned int cause = savedState->cause;

    if (CAUSE_IS_INT(cause)) { // l'eccezione corrente e' un interrupt
        interruptHandler();
    } else {
        unsigned int excCode = CAUSE_EXCCODE(cause);
        if (excCode == SYSEXCEPTION || excCode == EXC_SYSCALL_U) { // l'eccezione e' una syscall
            syscallHandler();
        } else if (excCode >= EXC_TLB_MIN && excCode <= EXC_TLB_MAX) { // l'eccezione e' un page fault
            tlbExceptionHandler();
        }
        else { // Trap per i codici 0-7, 9, 10, 12-23
            programTrapHandler();
        }
    }
}

// Gestore delle system call
void syscallHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    int syscallNum = (int)savedState->reg_a0; // il registro a0 contiene il valore della syscall
    int kernelMode = IS_KERNEL_MODE(savedState->status);
    savedState->pc_epc += WORDLEN; // le eccezioni volontarie hanno l'incremento del PC

    if (syscallNum < 0) {
        if (!kernelMode) { // tentativo di performare una syscall in modalita' user
            savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT); // mantiene le informazioni del registro aggiungendo PRIVINSTR come excCode
            programTrapHandler();
            return;
        }
        switch (syscallNum) {
            case CREATEPROCESS:
                createProcess(savedState);
                break;
            case TERMPROCESS:
                terminateProcess(savedState);
                break;
            case PASSEREN:
                passeren(savedState);
                break;
            case VERHOGEN:
                verhogen(savedState);
                break;
            case DOIO:
                doIO(savedState);
                break;
            case GETTIME:
                getCPUTime(savedState);
                break;
            case CLOCKWAIT:
                waitForClock(savedState);
                break;
            case GETSUPPORTPTR:
                getSupportData(savedState);
                break;
            case GETPROCESSID:
                getProcessID(savedState);
                break;
            case YIELD:
                yield(savedState);
                break;
            default: // caso default - syscall non esistente
                savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
                programTrapHandler();
                break;
        }
    } else {
        passUpOrDie(GENERALEXCEPT, savedState);
    }
}

// Gestore dei trap
void programTrapHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0); 
    passUpOrDie(GENERALEXCEPT, savedState);
}

// Gestore dei page fault
void tlbExceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(PGFAULTEXCEPT, savedState);
}

/* ----------------
    System calls
   ---------------*/
// NSYS1
static void createProcess(state_t *savedState) {
    state_t *childState = (state_t *) savedState->reg_a1; // il registro a1 contiene lo stato del nuovo processo
    int priority = (int) savedState->reg_a2;
    support_t *supportPtr = (support_t *) savedState->reg_a3;
    pcb_t *newPcb = allocPcb();

    if (newPcb == NULL) { // non più free PCB disponibili
        savedState->reg_a0 = (unsigned int) -1;
        LDST(savedState); // LDST riprende un processo sospeso
        return;
    }

    newPcb->p_s = *childState;
    newPcb->p_prio = priority;
    newPcb->p_supportStruct = supportPtr;
    newPcb->p_time = 0;
    newPcb->p_semAdd = NULL; // stato ready

    //inserimento del processo nelle varie strutture dati
    insertChild(currentProcess, newPcb);
    insertProcQ(&readyQueue, newPcb);
    processCount++;
    savedState->reg_a0 = (unsigned int) newPcb->p_pid;
    LDST(savedState);
}

// NSYS2
static void terminateProcess(state_t *savedState) {
    int targetPid = (int) savedState->reg_a1;

    if (targetPid == 0) { // suicidio del processo corrente
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
    } else {
        pcb_t *target = NULL;
        pcb_t *iter;

        // cerco il processo nella readyQueue
        list_for_each_entry(iter, &readyQueue, p_list) {
            if (iter->p_pid == targetPid) {
                target = iter;
                break;
            }
        }
        
        if (target == NULL) { // il processo non era tra quelli ready
            if (currentProcess != NULL && currentProcess->p_pid == targetPid) { // il processo cercato e' quello in esecuzione
                target = currentProcess;
            } else { // non e' ne' pronto, ne' in esecuzione, vediamo se era fra quelli bloccati
                target = searchBlockedPcb(targetPid);
            }
        } 
        
        if (target != NULL){ // ho trovato il pcb con pid = targetPid
            terminateProcessTree(target);
        }
    }

    if (currentProcess == NULL) { // Il processo corrente è morto (ha ucciso se stesso o un suo antenato)
        scheduler(); // Il processore è vuoto, chiamiamo lo scheduler per far partire un altro processo
    } else {
        // Il processo corrente è sopravvisuto (es. ha ucciso un processo non correlato o target era NULL)
        // Dobbiamo restituirgli il controllo immediatamente
        LDST(savedState);
    }
}

// NSYS3 -- blocking
// -> approccio Dijkstra
static void passeren(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;

    (*semAddr)--; // P

    if (*semAddr < 0) { // il semaforo era <= 0, facendo una P blocco il processo -> caso bloccante
        currentProcess->p_s = *savedState;
        updateCPUTime();
        insertBlocked(semAddr, currentProcess);
        int *base = &deviceSemaphores[0];
        int *end  = &deviceSemaphores[SEMDEVLEN];
        if (semAddr >= base && semAddr < end) { // controllo di sicurezza per semaforo valido
            softBlockCount++;
        }
        currentProcess = NULL;
        scheduler();
    } else { // la P era lecita, quindi recupero il savedState -> caso NON bloccante
        LDST(savedState);
    }
}

// NSYS4
static void verhogen(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;

    (*semAddr)++; // V

    if (*semAddr <= 0) { // se c'erano processi bloccati aggiorno il softBlockCount
        pcb_t *unblocked = removeBlocked(semAddr);
        if (unblocked != NULL) {
            unblocked->p_semAdd = NULL;
            int *base = &deviceSemaphores[0];
            int *end  = &deviceSemaphores[SEMDEVLEN];
            if (semAddr >= base && semAddr < end) {
                softBlockCount--;
            }
            insertProcQ(&readyQueue, unblocked);
        }
    }

    LDST(savedState);
}

// NSYS5 -- blocking
static void doIO(state_t *savedState) {
    int *commandAddr = (int *) savedState->reg_a1;
    int commandValue = (int) savedState->reg_a2;
    int semIdx = getSemaphoreIndex(commandAddr);

    //Salviamo stato e tempo del processo corrente
    currentProcess->p_s = *savedState;
    updateCPUTime();

    //Blocchiamo il processo sul semaforo del device
    deviceSemaphores[semIdx]--;
    insertBlocked(&deviceSemaphores[semIdx], currentProcess);
    softBlockCount++;
    currentProcess = NULL;

    //critical session - genera un interrupt
    *commandAddr = commandValue; //posizionamento del contenuto da scrivere
    scheduler();
}

// NSYS6
static void getCPUTime(state_t *savedState) {
    cpu_t current_time;
    STCK(current_time);
    // Tempo passato (p_time) + Tempo del quanto corrente (current_time - start_tod)
    savedState->reg_a0 = (unsigned int) (currentProcess->p_time + (current_time - start_tod));
    LDST(savedState);
}

// NSYS7
static void waitForClock(state_t *savedState) {
    currentProcess->p_s = *savedState;
    updateCPUTime(); 

    deviceSemaphores[PSEUDOCLOCK_SEM_IDX]--; // P su semaforo dello pseudo-clock
    insertBlocked(&deviceSemaphores[PSEUDOCLOCK_SEM_IDX], currentProcess); // blocco sempre -> V ogni 100ms dal nucleo
    softBlockCount++;
    currentProcess = NULL;

    scheduler();
}

// NSYS8
static void getSupportData(state_t *savedState) {
    savedState->reg_a0 = (unsigned int) currentProcess->p_supportStruct;
    LDST(savedState);
}

// NSYS9
static void getProcessID(state_t *savedState) {
    int parent = (int) savedState->reg_a1;
    if (parent == 0) {
        savedState->reg_a0 = (unsigned int) currentProcess->p_pid;
    } else {
        if (currentProcess->p_parent != NULL) {
            savedState->reg_a0 = (unsigned int) currentProcess->p_parent->p_pid;
        } else {
            savedState->reg_a0 = 0;
        }
    }
    LDST(savedState);
}

// NSYS10
static void yield(state_t *savedState) {
    currentProcess->p_s = *savedState;
    updateCPUTime();

    if (!emptyProcQ(&readyQueue)) {
        // Estraiamo il processo attualmente con priorita' massima
        pcb_t *nextToRun = removeProcQ(&readyQueue);
        
        // Ora inseriamo il processo corrente, dopo aver estratto quello in testa per essere sicuri di 
        // non estrarlo anche se ha la priorita' massima
        insertProcQ(&readyQueue, currentProcess);
        
        // Settiamo il processo corrente a quello precedentemente estratto
        currentProcess = nextToRun;
        setTIMER(TIMESLICE * *((unsigned int *)TIMESCALEADDR));
        STCK(start_tod);
        LDST(&(currentProcess->p_s));
    } else {
        // Se la coda è vuota, il processo è l'unico vivo nel sistema
        insertProcQ(&readyQueue, currentProcess);
        currentProcess = NULL;
        scheduler();
    }
}

static void passUpOrDie(int exceptionType, state_t *savedState) {
    if (currentProcess->p_supportStruct == NULL) { //caso 1 - no support struct
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
        scheduler();
    } else { // caso 2 - support struct valida ed utilizzabile
        currentProcess->p_supportStruct->sup_exceptState[exceptionType] = *savedState;
        context_t *ctx = &currentProcess->p_supportStruct->sup_exceptContext[exceptionType];
        LDCXT(ctx->stackPtr, ctx->status, ctx->pc);
    }
}

static void terminateProcessTree(pcb_t *root) {
    while (!emptyChild(root)) {
        pcb_t *child = removeChild(root);
        terminateProcessTree(child);
    }

    if (root->p_parent != NULL) { // rendiamo il processo orfano
        outChild(root);
    }

    if (root == currentProcess) { // e' il processo in esecuzone
        currentProcess = NULL;
    } else if (root->p_semAdd != NULL) { // e' un processo bloccato
        int *sem = root->p_semAdd;
        outBlocked(root);
        int *base = &deviceSemaphores[0];
        int *end = &deviceSemaphores[SEMDEVLEN];
        if (sem >= base && sem < end) {
            softBlockCount--;
        } else {
            (*sem)++; /* restore user semaphore: killed process no longer occupies a P slot */
        }
    } else { // e' un processo ready
        outProcQ(&readyQueue, root);
    }
    processCount--;
    freePcb(root);
}

static void updateCPUTime(void) {
    cpu_t now;
    STCK(now);
    currentProcess->p_time += (now - start_tod);
}

static int getSemaphoreIndex(int *commandAddr) {
    unsigned int addr = (unsigned int) commandAddr;
    unsigned int offset = addr - START_DEVREG;
    //viene estrapolato il Device Number
    int DevNo = (int)((offset % 0x80) / 0x10); //0x80 byte per ogni linea - 0x10 byte per ogni device register
    //estrapolato l'interrupt delle linee
    int intLineNo = (int)(offset / 0x80) + 3;
    int semIdx = -1; //retVal
    if (intLineNo < 7) { //linee 3-6
        semIdx = (intLineNo - 3) * 8 + DevNo;
    } else { //Linea 7 - Terminale
        //inizio device register
        unsigned int devRegStart = START_DEVREG + ((intLineNo - 3) * 0x80) + (DevNo * 0x10);
        int internalOffset = addr - devRegStart;
        if (internalOffset == 0xC) {
            semIdx = 32 + DevNo; //Trasmissione
        } else {
            semIdx = 40 + DevNo; //Ricezione
        }
    }
    return semIdx;
}

// ------------ phase 3 ------------------------
// will locate the correct page table entry in some support level data structure
void uTLB_RefillHandler() { // -> cache miss
    state_t *savedState = (state_t *) BIOSDATAPAGE;
    unsigned int faultEntryHi = savedState->entry_hi;

    // if (savedState->pc_epc == 0) HALT();  // ← già ce l'avete
    // if (savedState->reg_sp == 0) HALT();  // ← aggiungete questo
    // if (savedState->reg_ra == 0) HALT();  // ← e questo

    // con la maschera isoliamo il numero della pagina (prime 5 posizioni esadecimali)
    // per ottenere il corretto vpn shiftiamo poi a dx di 12 bit (ovvero 3 posizioni esadecimali)
    unsigned int vpn = (faultEntryHi & VPN_MASK) >> VPNSHIFT; // top 20 bit = numero di pagina -> otteniamo 0x80000...0xBFFFF

    if (currentProcess == NULL || currentProcess->p_supportStruct == NULL) {
        PANIC();
    }

    // Determina l'indice nella Page Table
    int p;
    if (vpn == STACK_PAGE_VPN) { // 0xBFFFF -> entry 31
        p = USERPGTBLSIZE - 1;   // 31
    } else {
        p = (int)(vpn - KUSEG_VPN); // 0x80000..0x8001E -> 0..30
    }

    // Controllo di sicurezza: l'indirizzo era fuori dai range validi di memoria utente?
    if (p < 0 || p >= USERPGTBLSIZE) {
        // Un processo ha tentato di accedere a un'area di memoria inesistente.
        // Possiamo fermare tutto per sicurezza.
        PANIC(); 
    }
    
    pteEntry_t *pageTable = &(currentProcess->p_supportStruct->sup_privatePgTbl[p]);

    // 3. Write this Page Table entry into the TLB
    setENTRYHI(pageTable->pte_entryHI);
    setENTRYLO(pageTable->pte_entryLO);
    TLBWR(); // scrive in uno slot random del TLB
    // 4. Return control to the Current Process
    LDST((state_t*) BIOSDATAPAGE);
} 