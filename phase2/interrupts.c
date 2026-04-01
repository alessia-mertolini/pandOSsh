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

// in ogni caso isoliamo il bit i che identifica il device
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
    if (currentProcess != NULL) { // ritorno allo stato salvato
        LDST(getSavedExceptionState());
    } else { // non c'e' processo a cui tornare
        scheduler();
    }
}

// diversa dalla syscall perche' non scrivo nel registro a1 di invocazione dell'eccezione
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
    setTIMER(NEVER);  // ACK del PLT

    if (currentProcess == NULL) {
        /* PLT fired while no process was running (expired during exception handling) */
        scheduler();
        return; /* unreachable */
    }

    currentProcess->p_s = *savedState;
    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;
    scheduler();
}

static void handleIntervalTimer() {
    LDIT(PSECOND); // ACK dell'interrupt

    // Sblocca TUTTI i processi in attesa, poi forza il semaforo a 0
    while (deviceSemaphores[PSEUDOCLOCK_SEM_IDX] < 0) {
        pcb_t *unblocked = verhogenSemDevice(&deviceSemaphores[PSEUDOCLOCK_SEM_IDX], 0);
        if (unblocked == NULL) break;
    }
    
    // Forza il semaforo a 0: non deve accumularsi tra un tick e l'altro
    deviceSemaphores[PSEUDOCLOCK_SEM_IDX] = 0;

    returnToCurrentOrSchedule();
}

/* gestione tramite intCode anziche' IntLineNo - non usando quindi la macro `DEV_REG_ADDR` */
static void handleDeviceInterrupt(int intCode) {
    // Troviamo la linea di interrupt j (pag. 10 specifiche)
    unsigned int bitmap = *((volatile unsigned int *)CDEV_BITMAP_ADDR(intCode));

    /* Calcolo del devNo */
    int devNo = getDeviceNumber(bitmap);
    if (devNo < 0) {
        returnToCurrentOrSchedule();
        return;
    }

    /* Calcolo dell'indirizzo del device register */
    memaddr devRegAddr = DEV_REG_ADDR(intCode, devNo);

    if (intCode == IL_TERMINAL) {
        volatile unsigned int *transmStatus  = (unsigned int *)(devRegAddr + 0x8);
        volatile unsigned int *transmCommand = (unsigned int *)(devRegAddr + 0xC);
        volatile unsigned int *recvStatus    = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *recvCommand   = (unsigned int *)(devRegAddr + 0x4);

        if ((*transmStatus & 0xFF) == OKCHARTRANS) { // caso di trasmissione del carattere
            unsigned int statusCode = *transmStatus; // salvataggio stato
            *transmCommand = ACK; // ACK
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMTX_BASE + devNo], statusCode); // V
        } else if ((*recvStatus & 0xFF) == CHARRECV) { // caso di ricezione del carattere
            unsigned int statusCode = *recvStatus;
            *recvCommand = ACK;
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMRX_BASE + devNo], statusCode);
        }
    } else {
        volatile unsigned int *devStatus  = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *devCommand = (unsigned int *)(devRegAddr + 0x4);
        unsigned int statusCode = *devStatus;
        *devCommand = ACK;
        int semIdx = (intCode - IL_DISK) * DEVPERINT + devNo;
        verhogenSemDevice(&deviceSemaphores[semIdx], statusCode);
    }

    returnToCurrentOrSchedule();
}

void interruptHandler() {
    unsigned int cause = getCAUSE();
    unsigned int intCode = cause & 0x7FFFFFFF;

    // catena di if consente la gestione in ordine decrescente di priorita' (priorita' piu' alta corrisponde a linea piu' bassa)
    if (intCode == IL_CPUTIMER) { // linea 1: PLT
        handlePLT();
    } else if (intCode == IL_TIMER) { // linea 2: interval timer
        handleIntervalTimer();
    } else if (intCode == IL_TERMINAL) { // linee 3-7: devices
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
