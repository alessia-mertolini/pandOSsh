/* =========================================================================
 * exceptions.c  –  Phase 2: Nucleus Exception & SYSCALL Handlers
 * [DEBUG VERSION]
 * ========================================================================= */

#include "./headers/exceptions.h"
#include "./print.h"
#include <uriscv/const.h>
extern unsigned int *p5MemLocation;

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
        } else if (excCode == 7 && (unsigned int)p5MemLocation == 0x80000000) {
            /* WORKAROUND URISCV: dopo un TLB-Refill con entry invalida,
             * uRISCV genera Store Access Fault (excCode 7) invece di
             * Store Page Fault (excCode 15). Usiamo entry_hi per
             * riconoscere l'indirizzo e deviare al gestore TLB. */
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

    savedState->pc_epc += WORDLEN;

    if (syscallNum < 0) {
        if (!kernelMode) {
            savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
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
            default:
                savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
                programTrapHandler();
                break;
        }
    } else {
        passUpOrDie(GENERALEXCEPT, savedState);
    }
}

void programTrapHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    /* Azzera MPP: il processo riprenderà in user mode nel Support Level.
     * Necessario per il corretto flusso p5→p5a (ADDRERROR su kseg). */
    passUpOrDie(GENERALEXCEPT, savedState);
}

void tlbExceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(PGFAULTEXCEPT, savedState);
}

static void createProcess(state_t *savedState) {
    state_t *childState = (state_t *) savedState->reg_a1;
    int priority = (int) savedState->reg_a2;
    support_t *supportPtr = (support_t *) savedState->reg_a3;
    pcb_t *newPcb = allocPcb();
    if (newPcb == NULL) {
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
    savedState->reg_a0 = (unsigned int) newPcb->p_pid;
    LDST(savedState);
}

static void terminateProcess(state_t *savedState) {
    int targetPid = (int) savedState->reg_a1;

    if (targetPid == 0) {
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
    } else {
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
        if (target != NULL) {
            if (target == currentProcess) {
                terminateProcessTree(currentProcess);
                currentProcess = NULL;
            } else {
                terminateProcessTree(target);
            }
        }
    }
    scheduler();
}

static void passeren(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;
    (*semAddr)--;
    if (*semAddr < 0) {
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
        LDST(savedState);
    }
}

static void verhogen(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;
    (*semAddr)++;
    if (*semAddr <= 0) {
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

static void doIO(state_t *savedState) {
    int *commandAddr = (int *) savedState->reg_a1;
    int commandValue = (int) savedState->reg_a2;
    int semIdx = getSemaphoreIndex(commandAddr);

    /* 1. Salva stato e tempo del processo corrente */
    updateCPUTime();
    currentProcess->p_s = *savedState;

    /* 2. Blocca il processo sul semaforo del device */
    deviceSemaphores[semIdx]--;
    insertBlocked(&deviceSemaphores[semIdx], currentProcess);
    softBlockCount++;


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
    for(volatile int i = 0; i < 500; i++); //CICLO BUSYWAIT NECESSARIO PER COMPILAZIONE SU LINUX
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
    if (root->p_parent != NULL) {
        outChild(root);
    }
    if (root == currentProcess) {
        /* nop */
    } else if (root->p_semAdd != NULL) {
        int *sem = root->p_semAdd;
        outBlocked(root);
        int *base = &deviceSemaphores[0];
        int *end = &deviceSemaphores[SEMDEVLEN];
        if (sem >= base && sem < end) {
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