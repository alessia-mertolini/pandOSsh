/*
 * interrupts.c - Phase 2: Nucleus Interrupt Handlers
 *
 */

#include "./headers/interrupts.h"

// Restituisce il puntatore allo stato salvato dell'eccezione corrente
static inline state_t *getSavedExceptionState() {
    int cpuId = getPRID();
    return GET_EXCEPTION_STATE_PTR(cpuId);
}

// Restituisce l'indice del primo device attivo nel bitmap (o -1 se nessuno)
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

/*
 * Dopo aver gestito un interrupt, decide come proseguire:
 * - Se esiste un processo corrente, ripristina il suo stato (LDST) e lo riprende.
 * - Altrimenti invoca lo scheduler per selezionare il prossimo processo.
 */
static void returnToCurrentOrSchedule() {
    if (currentProcess != NULL) { // ritorno allo stato salvato
        LDST(getSavedExceptionState());
    } else { // non c'e' processo a cui tornare
        scheduler();
    }
}

// Versione della V (verhogen) dedicata ai device, che aggiorna il registro a0 del processo sbloccato con lo status code del device
static pcb_t *verhogenSemDevice(int *sem, unsigned int statusCode) {
    pcb_t *unblocked = NULL;
    (*sem)++;       // incremento del semaforo (V)
    if (*sem <= 0) {
        unblocked = removeBlocked(sem);             //se ci sono processi bloccati su di esso ne risveglia uno inserendolo nella readyQueue
        if (unblocked != NULL) {
            unblocked->p_s.reg_a0 = statusCode;     // comunica l'esito al processo sbloccato tramite a0
            unblocked->p_semAdd = NULL;
            insertProcQ(&readyQueue, unblocked);
            softBlockCount--;
        }
    }
    return unblocked;
}

// Gestore del PLT (Processor Local Timer)
static void handlePLT() {
    state_t *savedState = getSavedExceptionState();
    setTIMER(NEVER);  // disarma il PLT: ACK dell'interrupt

    // Se non c'e' processo corrente, non c'e' bisogno di salvare il contesto: basta chiamare lo scheduler
    if (currentProcess == NULL) {
        scheduler();
        return;
    }

    // Salva il contesto e rimette il processo in coda pronti
    currentProcess->p_s = *savedState;
    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;
    scheduler();
}

// Gestore dell'Interval Timer (pseudoclock)
static void handleIntervalTimer() {
    LDIT(PSECOND); // ricarica e ACK dell'Interval Timer

    // Sblocca tutti i processi in attesa del tick dello pseudoclock
    while (deviceSemaphores[PSEUDOCLOCK_SEM_IDX] < 0) {
        pcb_t *unblocked = verhogenSemDevice(&deviceSemaphores[PSEUDOCLOCK_SEM_IDX], 0);
        if (unblocked == NULL) break;
    }
    
    // Azzera il semaforo: i tick non si accumulano tra un periodo e l'altro
    deviceSemaphores[PSEUDOCLOCK_SEM_IDX] = 0;

    returnToCurrentOrSchedule();
}

// Gestore generico degli interrupt dei device (linee 3–7: disk, flash, ethernet, printer, terminal)
static void handleDeviceInterrupt(int intCode) {
    // Legge il bitmap dei device attivi sulla linea intCode
    unsigned int bitmap = *((volatile unsigned int *)CDEV_BITMAP_ADDR(intCode));

    // Determina quale device ha generato l'interrupt
    int devNo = getDeviceNumber(bitmap);
    if (devNo < 0) {
        returnToCurrentOrSchedule();
        return;
    }

    // Calcolo dell'indirizzo base del device register
    memaddr devRegAddr = DEV_REG_ADDR(intCode, devNo);

    if (intCode == IL_TERMINAL) {
        // Puntatori ai registri del terminale (TX e RX separati)
        volatile unsigned int *transmStatus  = (unsigned int *)(devRegAddr + 0x8);
        volatile unsigned int *transmCommand = (unsigned int *)(devRegAddr + 0xC);
        volatile unsigned int *recvStatus    = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *recvCommand   = (unsigned int *)(devRegAddr + 0x4);

        if ((*transmStatus & 0xFF) == OKCHARTRANS) {
            // Il trasmettitore ha completato l'invio di un carattere
            unsigned int statusCode = *transmStatus;
            *transmCommand = ACK;       // notifica al device che l'interrupt è stato gestito
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMTX_BASE + devNo], statusCode);
        } else if ((*recvStatus & 0xFF) == CHARRECV) {
            // Il ricevitore ha ricevuto un carattere
            unsigned int statusCode = *recvStatus;
            *recvCommand = ACK;
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMRX_BASE + devNo], statusCode);
        }
    } else {
        // Device generico (disk, flash, ethernet, printer)
        volatile unsigned int *devStatus  = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *devCommand = (unsigned int *)(devRegAddr + 0x4);
        unsigned int statusCode = *devStatus;
        *devCommand = ACK;
        // Calcola l'indice del semaforo: ogni linea ha DEVPERINT device
        int semIdx = (intCode - IL_DISK) * DEVPERINT + devNo;
        verhogenSemDevice(&deviceSemaphores[semIdx], statusCode);
    }

    returnToCurrentOrSchedule();
}

// Entry point principale del gestore degli interrupt
void interruptHandler() {
    unsigned int cause = getCAUSE();
    unsigned int intCode = cause & 0x7FFFFFFF;  // isola i bit della causa

    // catena di if consente la gestione in ordine decrescente di priorita' (priorita' piu' alta corrisponde a linea piu' bassa)
    if (intCode == IL_CPUTIMER) { // linea 1: PLT (timer locale della CPU)
        handlePLT();
    } else if (intCode == IL_TIMER) { // linea 2: Interval Timer (pseudoclock di sistema)
        handleIntervalTimer();
    } else if (intCode == IL_TERMINAL) { // linee 3-7: Device I/O
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
