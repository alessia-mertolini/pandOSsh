/* =========================================================================
 * exceptions.c  –  Phase 2: Nucleus Exception & SYSCALL Handlers
 * [DEBUG VERSION]
 * ========================================================================= */

#include "./headers/exceptions.h"
#include "./print.h"
<<<<<<< HEAD
#include "headers/constants.h"
=======
>>>>>>> gabriele
#include <uriscv/const.h>

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

void *memcpy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void exceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    unsigned int cause = savedState->cause;

    if (CAUSE_IS_INT(cause)) {
        interruptHandler();
    } else {
        unsigned int excCode = CAUSE_EXCCODE(cause);
        if (excCode == SYSEXCEPTION || excCode == EXC_SYSCALL_U) {
            syscallHandler();
        } else if (excCode >= EXC_TLB_MIN && excCode <= EXC_TLB_MAX) {
            tlbExceptionHandler();
        } else {
            programTrapHandler();
        }
    }
}

void syscallHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    int syscallNum = (int)savedState->reg_a0;
    int kernelMode = IS_KERNEL_MODE(savedState->status);

<<<<<<< HEAD
    // termprint_direct("[SYS] num=");
    // print_hex((unsigned int)syscallNum);
    // termprint_direct(" kern=");
    // print_hex((unsigned int)kernelMode);
    // termprint_direct("\n");

=======
>>>>>>> gabriele
    savedState->pc_epc += WORDLEN;

    if (syscallNum < 0) {
        if (!kernelMode) {
<<<<<<< HEAD
            // termprint_direct("[SYS] -> PRIVINSTR (user mode)\n");
=======
>>>>>>> gabriele
            savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
            programTrapHandler();
            return;
        }
        switch (syscallNum) {
            case CREATEPROCESS:
<<<<<<< HEAD
                // termprint_direct("[SYS] -> createProcess\n");
=======
>>>>>>> gabriele
                createProcess(savedState);
                break;
            case TERMPROCESS:
                // termprint_direct("[SYS] -> terminateProcess\n");
                terminateProcess(savedState);
                break;
            case PASSEREN:
                // termprint_direct("\t\t[SYS] -> passeren\n");
                passeren(savedState);
                break;
            case VERHOGEN:
                // termprint_direct("[SYS] -> verhogen\n");
                verhogen(savedState);
                break;
            case DOIO:
                // termprint_direct("[SYS] -> doIO\n");
                doIO(savedState);
                break;
            case GETTIME:
                // termprint_direct("[SYS] -> getCPUTime\n");
                getCPUTime(savedState);
                break;
            case CLOCKWAIT:
                // termprint_direct("[SYS] -> waitForClock\n");
                waitForClock(savedState);
                break;
            case GETSUPPORTPTR:
                // termprint_direct("[SYS] -> getSupportData\n");
                getSupportData(savedState);
                break;
            case GETPROCESSID:
                // termprint_direct("[SYS] -> getProcessID\n");
                getProcessID(savedState);
                break;
            case YIELD:
                // termprint_direct("[SYS] -> yield\n");
                yield(savedState);
                break;
            default:
<<<<<<< HEAD
                // termprint_direct("[SYS] -> default PRIVINSTR num=");
                // print_hex((unsigned int)syscallNum);
                // termprint_direct("\n");
=======
>>>>>>> gabriele
                savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
                programTrapHandler();
                break;
        }
    } else {
<<<<<<< HEAD
        // termprint_direct("[SYS] -> passUpOrDie (num>=1)\n");
=======
>>>>>>> gabriele
        passUpOrDie(GENERALEXCEPT, savedState);
    }
}

void programTrapHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    /* Azzera MPP: il processo riprenderà in user mode nel Support Level.
     * Necessario per il corretto flusso p5→p5a (ADDRERROR su kseg). */
    savedState->status &= ~MSTATUS_MPP_MASK;
    passUpOrDie(GENERALEXCEPT, savedState);
}

void tlbExceptionHandler(void) {
    // termprint_direct("[TLB] tlbExceptionHandler\n");
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);

    unsigned int excCode = CAUSE_EXCCODE(savedState->cause);
    if (excCode == 7 && getBADVADDR() == 0x80000000) {
        passUpOrDie(PGFAULTEXCEPT, savedState);
        return;
    }

    passUpOrDie(PGFAULTEXCEPT, savedState);
}

static void createProcess(state_t *savedState) {
    state_t *childState = (state_t *) savedState->reg_a1;
    int priority = (int) savedState->reg_a2;
    support_t *supportPtr = (support_t *) savedState->reg_a3;
    pcb_t *newPcb = allocPcb();
    if (newPcb == NULL) {
<<<<<<< HEAD
        // termprint_direct("[createProcess] FAIL no PCB\n");
=======
>>>>>>> gabriele
        savedState->reg_a0 = (unsigned int) -1;
        LDST(savedState);
        return;
    }
    newPcb->p_s = *childState;
    newPcb->p_prio = priority;
    newPcb->p_supportStruct = supportPtr;
    newPcb->p_time = 0;
    newPcb->p_semAdd = NULL;
    insertChild(currentProcess, newPcb);
    insertProcQ(&readyQueue, newPcb);
    processCount++;
<<<<<<< HEAD
    // termprint_direct("[createProcess] OK pid=");
    // print_hex((unsigned int)newPcb->p_pid);
    // termprint_direct("\n");
=======
>>>>>>> gabriele
    savedState->reg_a0 = (unsigned int) newPcb->p_pid;
    LDST(savedState);
}

<<<<<<< HEAD
/* Ricerca ricorsiva di un processo per PID all'interno dell'intero albero */
static pcb_t* findProcessByPid(pcb_t *node, int pid) {
    if (node == NULL) return NULL;
    if (node->p_pid == pid) return node;

    pcb_t *child;
    list_for_each_entry(child, &node->p_child, p_sib) {
        pcb_t *found = findProcessByPid(child, pid);
        if (found != NULL) return found;
    }
    return NULL;
}

=======
>>>>>>> gabriele
static void terminateProcess(state_t *savedState) {
    int targetPid = (int) savedState->reg_a1;

    if (targetPid == 0) {
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
    } else {
<<<<<<< HEAD
        // 1. Risali fino alla radice dell'albero dei processi
        pcb_t *root = currentProcess;
        if (root != NULL) {
            while (root->p_parent != NULL) {
                root = root->p_parent;
            }
        }
        
        // 2. Cerca il processo target esplorando tutto l'albero
        pcb_t *target = findProcessByPid(root, targetPid);

=======
        pcb_t *target = NULL;
        pcb_t *iter;
        list_for_each_entry(iter, &readyQueue, p_list) {
            if (iter->p_pid == targetPid) {
                target = iter;
                break;
            }
        }
        if (target == NULL && currentProcess != NULL && currentProcess->p_pid == targetPid) {
            target = currentProcess;
        }
        if (target == NULL) {
            /* Process not in readyQueue or currentProcess: search blocked processes in ASL */
            target = searchBlockedPcb(targetPid);
        }
>>>>>>> gabriele
        if (target != NULL) {
            if (target == currentProcess) {
                terminateProcessTree(currentProcess);
                currentProcess = NULL;
            } else {
                terminateProcessTree(target);
                // Nota: se il currentProcess fa parte del sottoalbero eliminato 
                // (es. p10 che uccide p9), il suo PCB verrà distrutto.
                // Fortunatamente lo scheduler() si occuperà in automatico 
                // di resettare currentProcess = NULL.
            }
        }
    }
<<<<<<< HEAD
    
=======
>>>>>>> gabriele
    scheduler();
}

static void passeren(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;
    (*semAddr)--;
<<<<<<< HEAD

    if (*semAddr < 0) {
        // termprint_direct("[passeren] BLOCK -> scheduler\n");
=======
    if (*semAddr < 0) {
>>>>>>> gabriele
        updateCPUTime();
        currentProcess->p_s = *savedState;
        insertBlocked(semAddr, currentProcess);
        int *base = &deviceSemaphores[0];
        int *end  = &deviceSemaphores[SEMDEVLEN];
        if (semAddr >= base && semAddr < end) {
            softBlockCount++;
        }
        currentProcess = NULL;
        scheduler();
    } else {
<<<<<<< HEAD
        // termprint_direct("[passeren] PASS\n");
=======
>>>>>>> gabriele
        LDST(savedState);
    }
}

static void verhogen(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;
    (*semAddr)++;
<<<<<<< HEAD
    // termprint_direct("[verhogen] sem=");
    // print_hex((unsigned int)*semAddr);
    // termprint_direct("\n");
=======
>>>>>>> gabriele
    if (*semAddr <= 0) {
        pcb_t *unblocked = removeBlocked(semAddr);
        if (unblocked != NULL) {
            // termprint_direct("[verhogen] unblocked pid=");
            // print_hex((unsigned int)unblocked->p_pid);
            // termprint_direct("\n");
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

static void doIO(state_t *savedState) {
    int *commandAddr = (int *) savedState->reg_a1;
    int commandValue = (int) savedState->reg_a2;
    int semIdx = getSemaphoreIndex(commandAddr);

<<<<<<< HEAD
    // termprint_direct("[doIO] semIdx=");
    // print_hex((unsigned int)semIdx);
    // termprint_direct(" cmdAddr=");
    // print_hex((unsigned int)commandAddr);
    // termprint_direct(" cmdVal=");
    // print_hex((unsigned int)commandValue);
    // termprint_direct("\n");

=======
>>>>>>> gabriele
    /* 1. Salva stato e tempo del processo corrente */
    updateCPUTime();
    currentProcess->p_s = *savedState;

    /* 2. Blocca il processo sul semaforo del device */
    deviceSemaphores[semIdx]--;
    insertBlocked(&deviceSemaphores[semIdx], currentProcess);
    softBlockCount++;

<<<<<<< HEAD
    // termprint_direct("[doIO] blocked pid=");
    // print_hex((unsigned int)currentProcess->p_pid);
    // termprint_direct(" sem=");
    // print_hex((unsigned int)deviceSemaphores[semIdx]);
    // termprint_direct("\n");
=======
>>>>>>> gabriele

    /* 3. Solo ORA togli il currentProcess */
    currentProcess = NULL;

    /* 4. Scrivi il comando nel device register per avviare l'operazione.
     *    IMPORTANTE: deve essere l'ultima istruzione prima di scheduler().
     *    Qualsiasi termprint_direct() dopo questa scrittura userebbe lo
     *    stesso registro TX del terminale, consumando l'interrupt pendente
     *    prima che WAIT() lo possa ricevere. */
    *commandAddr = commandValue;

    /* 5. Chiama lo scheduler */
    scheduler();
}

static void getCPUTime(state_t *savedState) {
    cpu_t currentTOD;
    STCK(currentTOD);
    cpu_t totalTime = currentProcess->p_time + (currentTOD - start_tod);
    savedState->reg_a0 = (unsigned int) totalTime;
    LDST(savedState);
}

static void waitForClock(state_t *savedState) {
<<<<<<< HEAD
    // termprint_direct("[waitForClock] BLOCK -> scheduler\n");
=======
    for(volatile int i = 0; i < 500; i++); //CICLO BUSYWAIT NECESSARIO PER COMPILAZIONE SU LINUX
>>>>>>> gabriele
    updateCPUTime();
    currentProcess->p_s = *savedState;
    deviceSemaphores[PSEUDOCLOCK_SEM_IDX]--;
    insertBlocked(&deviceSemaphores[PSEUDOCLOCK_SEM_IDX], currentProcess);
    softBlockCount++;
    currentProcess = NULL;
    scheduler();
}

static void getSupportData(state_t *savedState) {
    savedState->reg_a0 = (unsigned int) currentProcess->p_supportStruct;
    LDST(savedState);
}

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

static void yield(state_t *savedState) {
    // termprint_direct("[yield] -> scheduler\n");
    updateCPUTime();
    currentProcess->p_s = *savedState;
    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;
    scheduler();
}

static void passUpOrDie(int exceptionType, state_t *savedState) {
    if (currentProcess->p_supportStruct == NULL) {
<<<<<<< HEAD
        termprint_direct("[PUOD] verso lo scheduler\n");
=======
>>>>>>> gabriele
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
        scheduler();
    } else {
<<<<<<< HEAD
        termprint_direct("[PUOD] verso LDCXT\n");
=======
>>>>>>> gabriele
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
    if (root->p_parent != NULL) {
        outChild(root);
    }
    if (root == currentProcess) {
        /* nop */
    } else if (root->p_semAdd != NULL) {
<<<<<<< HEAD
        int *sem = root->p_semAdd; // Salva il puntatore al semaforo
        
        outBlocked(root); // Rimuove il processo dalla coda
        
        (*sem)++; // <-- RIGA CRITICA: Ripristina il valore del semaforo poiché il processo non è più in attesa!

        // Distinzione tra semafori dei dispositivi e semafori utente
        int *base = &deviceSemaphores[0];
        int *end = &deviceSemaphores[SEMDEVLEN];
        if (sem >= base && sem < end) { 
=======
        int *sem = root->p_semAdd;
        outBlocked(root);
        int *base = &deviceSemaphores[0];
        int *end = &deviceSemaphores[SEMDEVLEN];
        if (sem >= base && sem < end) {
>>>>>>> gabriele
            softBlockCount--;
        } else {
            (*sem)++; /* restore user semaphore: killed process no longer occupies a P slot */
        }
    } else {
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