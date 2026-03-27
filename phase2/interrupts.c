/*
 * interrupts.c
 *
 * NOTA: tutte le chiamate a termprint_direct/print_hex sono state rimosse
 * da questo file perché usano lo stesso registro TX del terminale 0.
 * Qualsiasi scrittura sul terminale mentre un interrupt del terminale è
 * pendente in MIP causa un ACK implicito, svuotando il bit nel CDEV bitmap
 * prima che handleDeviceInterrupt lo possa leggere.
 */

#include "./headers/interrupts.h"

static inline state_t *getSavedExceptionState() {
    int cpuId = getPRID();
    return GET_EXCEPTION_STATE_PTR(cpuId);
}

static void updateProcessTime() {
    if (currentProcess != NULL) {
        cpu_t now;
        STCK(now);
        currentProcess->p_time += (now - start_tod);
        STCK(start_tod);
    }
}

static int getDeviceNumber(unsigned int bitmap) {
    if (bitmap & DEV0ON) return 0;
    if (bitmap & DEV1ON) return 1;
    if (bitmap & DEV2ON) return 2;
    if (bitmap & DEV3ON) return 3;
    if (bitmap & DEV4ON) return 4;
    if (bitmap & DEV5ON) return 5;
    if (bitmap & DEV6ON) return 6;
    if (bitmap & DEV7ON) return 7;
    return -1;
}

static void returnToCurrentOrSchedule() {
    if (currentProcess != NULL) {
        LDST(getSavedExceptionState());
    } else {
        scheduler();
    }
}

static pcb_t *verhogenSemDevice(int *sem, unsigned int statusCode) {
    pcb_t *unblocked = NULL;
    (*sem)++;
    if (*sem <= 0) {
        unblocked = removeBlocked(sem);
        if (unblocked != NULL) {
            unblocked->p_s.reg_a0 = statusCode;
            unblocked->p_semAdd = NULL;
            insertProcQ(&readyQueue, unblocked);
            softBlockCount--;
        }
    }
    return unblocked;
}

static void handlePLT() {
    state_t *savedState = getSavedExceptionState();
    setTIMER(NEVER);  /* disable PLT unconditionally */
    if (currentProcess == NULL) {
        /* PLT fired while no process was running (expired during exception handling) */
        scheduler();
        return; /* unreachable */
    }
    updateProcessTime();
    currentProcess->p_s = *savedState;
    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;
    scheduler();
}

static void handleIntervalTimer() {
    LDIT(PSECOND);
    while (deviceSemaphores[SEMDEV_PSEUDOCLK] <= 0) {
        pcb_t *unblocked = verhogenSemDevice(&deviceSemaphores[SEMDEV_PSEUDOCLK], 0);
        if (unblocked == NULL) break;
    }
    returnToCurrentOrSchedule();
}

static void handleDeviceInterrupt(int intLineNo) {
    /* 1. Leggi la bitmap dei device che hanno interrotto su questa linea */
    unsigned int bitmap = *((volatile unsigned int *)CDEV_BITMAP_ADDR(intLineNo));

    /* 2. Trova quale device ha interrotto */
    int devNo = getDeviceNumber(bitmap);
    if (devNo < 0) {
        returnToCurrentOrSchedule();
        return;
    }

    /* 3. Calcola l'indirizzo del device register */
    memaddr devRegAddr = DEV_REG_ADDR(intLineNo, devNo);

    /* 4. Gestisci in base al tipo di device */
    if (intLineNo == IL_TERMINAL) {
        volatile unsigned int *transmStatus  = (unsigned int *)(devRegAddr + 0x8);
        volatile unsigned int *transmCommand = (unsigned int *)(devRegAddr + 0xC);
        volatile unsigned int *recvStatus    = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *recvCommand   = (unsigned int *)(devRegAddr + 0x4);

        if ((*transmStatus & 0xFF) == OKCHARTRANS) {
            unsigned int statusCode = *transmStatus;
            *transmCommand = ACK;
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMTX_BASE + devNo], statusCode);
        } else if ((*recvStatus & 0xFF) == CHARRECV) {
            unsigned int statusCode = *recvStatus;
            *recvCommand = ACK;
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMRX_BASE + devNo], statusCode);
        }
    } else {
        volatile unsigned int *devStatus  = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *devCommand = (unsigned int *)(devRegAddr + 0x4);
        unsigned int statusCode = *devStatus;
        *devCommand = ACK;
        int semIdx = (intLineNo - IL_DISK) * DEVPERINT + devNo;
        verhogenSemDevice(&deviceSemaphores[semIdx], statusCode);
    }

    returnToCurrentOrSchedule();
}


void interruptHandler() {
    unsigned int cause = getCAUSE();
    unsigned int intCode = cause & 0x7FFFFFFF;

    if (intCode == IL_CPUTIMER) {
        handlePLT();
    } else if (intCode == IL_TIMER) {
        handleIntervalTimer();
    } else if (intCode == IL_TERMINAL) {
        handleDeviceInterrupt(IL_TERMINAL);
    } else if (intCode == IL_PRINTER) {
        handleDeviceInterrupt(IL_PRINTER);
    } else if (intCode == IL_ETHERNET) {
        handleDeviceInterrupt(IL_ETHERNET);
    } else if (intCode == IL_FLASH) {
        handleDeviceInterrupt(IL_FLASH);
    } else if (intCode == IL_DISK) {
        handleDeviceInterrupt(IL_DISK);
    } else {
        returnToCurrentOrSchedule();
    }
}
