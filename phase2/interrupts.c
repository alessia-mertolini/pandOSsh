/*
 * interrupts.c  [DEBUG VERSION]
 */

#include "./headers/interrupts.h"
#include "./print.h"

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
        termprint_direct("[INT] returnToCurrent\n");
        LDST(getSavedExceptionState());
    } else {
        termprint_direct("[INT] -> scheduler\n");
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
            termprint_direct("[verhogenSemDev] unblocked pid=");
            print_hex((unsigned int)unblocked->p_pid);
            termprint_direct("\n");
        }
    }
    return unblocked;
}

static void handlePLT() {
    termprint_direct("[PLT] handlePLT\n");
    state_t *savedState = getSavedExceptionState();
    setTIMER(MIE_ALL);
    currentProcess->p_s = *savedState;
    updateProcessTime();
    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;
    termprint_direct("[PLT] -> scheduler\n");
    scheduler();
}

static void handleIntervalTimer() {
    termprint_direct("[ITIMER] sono dentro interval timer\n");
    LDIT(PSECOND);
    while (deviceSemaphores[SEMDEV_PSEUDOCLK] <= 0) {
        pcb_t *unblocked = verhogenSemDevice(&deviceSemaphores[SEMDEV_PSEUDOCLK], 0);
        if (unblocked == NULL) break;
    }
    termprint_direct("sono uscito da interval timer\n");
    /* Non tornare subito — ricontrolla la cause per altri interrupt */
    returnToCurrentOrSchedule();
}

static void handleDeviceInterrupt(int intLineNo) {
    /* 1. Leggi la bitmap dei device che hanno interrotto su questa linea */
    unsigned int bitmap = *((volatile unsigned int *)CDEV_BITMAP_ADDR(intLineNo));

    termprint_direct("[DEV] line="); print_hex((unsigned int)intLineNo);
    termprint_direct(" bitmap=");    print_hex(bitmap);
    termprint_direct("\n");

    /* 2. Trova quale device ha interrotto */
    int devNo = getDeviceNumber(bitmap);
    if (devNo < 0) {
        termprint_direct("[DEV] no devNo found\n");
        returnToCurrentOrSchedule();
        return;
    }

    termprint_direct("[DEV] devNo="); print_hex((unsigned int)devNo); termprint_direct("\n");

    /* 3. Calcola l'indirizzo del device register - intLineNo è già 17-21 */
    memaddr devRegAddr = DEV_REG_ADDR(intLineNo, devNo);

    termprint_direct("[DEV] devRegAddr="); print_hex(devRegAddr); termprint_direct("\n");

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
        } else {
            termprint_direct("[DEV] TERM spurious\n");
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

// void interruptHandler() {
//     unsigned int cause = getCAUSE();
//     unsigned int intCode = CAUSE_EXCCODE(cause);

//     termprint_direct("[INT] cause="); print_hex(cause); termprint_direct("\n");
//     termprint_direct("[INT] code="); print_hex(intCode); termprint_direct("\n");

//     if (intCode == 1) {
//         handlePLT();
//     } else if (intCode == 2) {
//         handleIntervalTimer();
//     } else if (intCode == 3) {
//         handleDeviceInterrupt(IL_DISK);
//     } else if (intCode == 4) {
//         handleDeviceInterrupt(IL_FLASH);
//     } else if (intCode == 5) {
//         handleDeviceInterrupt(IL_ETHERNET);
//     } else if (intCode == 6) {
//         handleDeviceInterrupt(IL_PRINTER);
//     } else if (intCode == 7) {
//         handleDeviceInterrupt(IL_TERMINAL);
//     } else {
//         termprint_direct("[INT] spurious\n");
//         returnToCurrentOrSchedule();
//     }
// }


void interruptHandler() {
    unsigned int cause = getCAUSE();
    unsigned int intCode = cause & 0x7FFFFFFF;  // rimuovi bit 31

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