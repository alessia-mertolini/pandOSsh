/* =========================================================================
 * exceptions.c  –  Phase 2: Nucleus Exception & SYSCALL Handlers

   NOTA:
    - LDST -> usato per operazioni di get (casi NON bloccanti)
    - scheduler -> usato per operazioni bloccanti 
 * ========================================================================= */

#include "./headers/exceptions.h"
#include "headers/scheduler.h"
#include <uriscv/const.h>
#include <uriscv/liburiscv.h>

static void createProcess(state_t *savedState);
static void terminateProcess(state_t *savedState);
static void passeren(state_t *savedState);
static void verhogen(state_t *savedState);
static void doIO(state_t *savedState);
static void getCPUTime(state_t *savedState);
static void waitForClock(state_t *savedState);
static void getSupportData(state_t *savedState);
static void getProcessID(state_t *savedState);
static void yield(state_t *savedState);

// Funzione ausiliaria per la corretta compilazione da parte di GCC
void *memcpy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* ------------------------------------------------------------------------
    Gestori di eventi
   ------------------------------------------------------------------------ */

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
            // } else if (excCode == 7 && getBADVADDR() >= 0x80000000U) {
        //     /* WORKAROUND URISCV: dopo un TLB-Refill con entry invalida,
        //      * uRISCV genera Store Access Fault (excCode 7) invece di
        //      * Store Page Fault (excCode 15). Usiamo entry_hi per
        //      * riconoscere l'indirizzo e deviare al gestore TLB. */
        //     tlbExceptionHandler();
        // } 
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

    savedState->pc_epc += WORDLEN; // solo le eccezioni volontarie (syscall) hanno l'incremento del PC

    if (syscallNum < 0) {
        if (!kernelMode) { // attempt to perform syscall in user mode
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
            default: // non-existent syscall
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
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0); //qui non si può fare savedState->status &= ~MSTATUS_MPP_MASK; 
    //perché sarebbe come mentire al PassUpOrDie, alterando la natura del processo. Il nucleo non decide queste cose, le decide il 
    //livello di supporto, ovvero il p2test
    unsigned int cause = savedState->cause;
    unsigned int excCode = CAUSE_EXCCODE(cause);
    if (excCode == 7 && getBADVADDR() >= 0x80000000U) {
            /* WORKAROUND URISCV: dopo un TLB-Refill con entry invalida,
             * uRISCV genera Store Access Fault (excCode 7) invece di
             * Store Page Fault (excCode 15). Usiamo entry_hi per
             * riconoscere l'indirizzo e deviare al gestore TLB. */
            tlbExceptionHandler();
        } 
    passUpOrDie(GENERALEXCEPT, savedState);
}

// Gestore dei page fault
void tlbExceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(PGFAULTEXCEPT, savedState);
}

/* ------------------------------------------------------------------------
    System calls
   ------------------------------------------------------------------------ */

// NSYS1
static void createProcess(state_t *savedState) {
    state_t *childState = (state_t *) savedState->reg_a1; // il registro a1 contiene lo stato del nuovo processo
    int priority = (int) savedState->reg_a2;
    support_t *supportPtr = (support_t *) savedState->reg_a3;
    pcb_t *newPcb = allocPcb();

    if (newPcb == NULL) { // e.g. no more free PCBs
        savedState->reg_a0 = (unsigned int) -1;
        LDST(savedState); // LDST restarts an interrupted process
        return;
    }

    newPcb->p_s = *childState;
    newPcb->p_prio = priority;
    newPcb->p_supportStruct = supportPtr;
    newPcb->p_time = 0;
    newPcb->p_semAdd = NULL; // ready state

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

            if (target == currentProcess) currentProcess = NULL; // caso in cui ho terminato il processo in esecuzione
        }
    }

    if (currentProcess == NULL) {
        // Il processo corrente è morto (ha ucciso se stesso o un suo antenato).
        // Il processore è vuoto, chiamiamo lo scheduler per far partire un altro processo.
        scheduler();
    } else {
        // Il processo corrente è SOPRAVVISSUTO (es. ha ucciso un processo non correlato o target era NULL).
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
        updateCPUTime();
//         cpu_t now;
// STCK(now);
// currentProcess->p_time += (now - start_tod);
        currentProcess->p_s = *savedState;
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

//     cpu_t now;
// STCK(now);
// currentProcess->p_time += (now - start_tod);

    /* Salviamo stato e tempo del processo corrente */
    updateCPUTime();
    currentProcess->p_s = *savedState;

    /* Blocchiamo il processo sul semaforo del device */
    deviceSemaphores[semIdx]--;
    insertBlocked(&deviceSemaphores[semIdx], currentProcess);
    softBlockCount++;

    /* Togliamo currentProcess */
    currentProcess = NULL;

    /* 4. Scrivi il comando nel device register per avviare l'operazione.
     *    IMPORTANTE: deve essere l'ultima istruzione prima di scheduler().
     *    Qualsiasi termprint_direct() dopo questa scrittura userebbe lo
     *    stesso registro TX del terminale, consumando l'interrupt pendente
     *    prima che WAIT() lo possa ricevere. */
    *commandAddr = commandValue; // critical section

    /* 5. Chiama lo scheduler */
    scheduler();
}

// NSYS6 - DA SPECIFICHE
static void getCPUTime(state_t *savedState) {
    cpu_t current_time;
    STCK(current_time);
    // Tempo passato (p_time) + Tempo del quanto corrente (current_time - start_tod)
    savedState->reg_a0 = (unsigned int) (currentProcess->p_time + (current_time - start_tod));

    LDST(savedState);
}

// NSYS7
static void waitForClock(state_t *savedState) {
    // cpu_t now;
    // STCK(now);
    // currentProcess->p_time += (now - start_tod);
    //for(volatile int i = 0; i < 2; i++); //CICLO BUSYWAIT NECESSARIO PER COMPILAZIONE SU LINUX
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
//     cpu_t now;
// STCK(now);
// currentProcess->p_time += (now - start_tod);
    updateCPUTime();
    currentProcess->p_s = *savedState;

    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;

    scheduler();
}

static void passUpOrDie(int exceptionType, state_t *savedState) {
    if (currentProcess->p_supportStruct == NULL) {
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
        scheduler();
    } else {
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
    start_tod = now;
}

static int getSemaphoreIndex(int *commandAddr) {
    unsigned int addr = (unsigned int) commandAddr;
    unsigned int offset = addr - START_DEVREG;
    int DevNo = (int)((offset % 0x80) / 0x10);
    int intLineNo = (int)(offset / 0x80) + 3;
    int semIdx = -1;
    if (intLineNo < 7) {
        semIdx = (intLineNo - 3) * 8 + DevNo;
    } else {
        unsigned int devRegStart = START_DEVREG + ((intLineNo - 3) * 0x80) + (DevNo * 0x10);
        int internalOffset = addr - devRegStart;
        if (internalOffset == 0xC) {
            semIdx = 32 + DevNo;
        } else {
            semIdx = 40 + DevNo;
        }
    }
    return semIdx;
}