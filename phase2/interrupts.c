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
    termprint_direct("[ITIMER] handleIntervalTimer\n");
    pcb_t *unblocked;
    LDIT(PSECOND);
    while (deviceSemaphores[SEMDEV_PSEUDOCLK] <= 0) {
        unblocked = verhogenSemDevice(&deviceSemaphores[SEMDEV_PSEUDOCLK], 0);
        if (unblocked == NULL) {
            termprint_direct("[ITIMER] spurious break\n");
            break;
        }
    }
    termprint_direct("[ITIMER] done\n");
    returnToCurrentOrSchedule();
}

static void handleDeviceInterrupt(int intLineNo) {
    //convertiamo un numero tra 17 e 21 nell'indirizzo giusto?
    unsigned int bitmap = CDEV_BITMAP_ADDR(intLineNo);    
    termprint_direct("[DEV] line=");  print_hex((unsigned int)intLineNo);
    termprint_direct(" bitmap=");     print_hex(bitmap);   // ← AGGIUNGI QUESTA
    termprint_direct("\n");
    
    int devNo = getDeviceNumber(bitmap);
    termprint_direct("[GOOOL?] line=");  print_hex(devNo);
    if (devNo < 0 && intLineNo == INTLINE_TERM) {
        for (int i = 0; i < DEVPERINT; i++) {
            int actualLine = intLineNo + (DEV_IL_START - 3);
            memaddr devRegAddr = DEV_REG_ADDR(actualLine, i);
            volatile unsigned int *transmStatus = (unsigned int *)(devRegAddr + 0x8);
            volatile unsigned int *recvStatus   = (unsigned int *)(devRegAddr + 0x0);
            if ((*transmStatus & 0xFF) == OKCHARTRANS || (*recvStatus & 0xFF) == CHARRECV) {
                devNo = i;
                break;
            }
        }
    }

    if (devNo < 0) {
        termprint_direct("[DEV] no devNo found\n");
        returnToCurrentOrSchedule();
        return;
    }

    termprint_direct("[DEV] devNo=");
    print_hex((unsigned int)devNo);
    termprint_direct("\n");

    intLineNo -= 14;
    int actualLine = intLineNo + (DEV_IL_START - 3);
    memaddr devRegAddr = DEV_REG_ADDR(actualLine, devNo);

    if (intLineNo == INTLINE_TERM) {
        volatile unsigned int transmStatus  = (unsigned int)(devRegAddr + 0x8);
        volatile unsigned int transmCommand = (unsigned int)(devRegAddr + 0xC);
        volatile unsigned int recvStatus    = (unsigned int)(devRegAddr + 0x0);
        volatile unsigned int recvCommand   = (unsigned int)(devRegAddr + 0x4);
        
        termprint_direct("\n");
        termprint_direct("[AAAAA] transmStatus= ");  print_hex(transmStatus);
        termprint_direct("\n");
        termprint_direct("[BITBIT] transmStatus= ");  print_hex(transmStatus & 0xFF);
        termprint_direct("\n");

        if ((transmStatus) == OKCHARTRANS) {
            termprint_direct("[DEV] TERM TX complete\n");
            unsigned int statusCode = transmStatus;
            transmCommand = ACK;
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMTX_BASE + devNo], statusCode);
        } else if ((recvStatus) == CHARRECV) {
            termprint_direct("[DEV] TERM RX complete\n");
            unsigned int statusCode = recvStatus;
            recvCommand = ACK;
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMRX_BASE + devNo], statusCode);
        } else {
            termprint_direct("[DEV] TERM spurious transmStatus=");
            print_hex(transmStatus);
            termprint_direct("\n");
        }
    } else {
        volatile unsigned int *devStatus  = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *devCommand = (unsigned int *)(devRegAddr + 0x4);
        unsigned int statusCode = *devStatus;
        *devCommand = ACK;
        int semIdx = (intLineNo - 3) * DEVPERINT + devNo;
        termprint_direct("[DEV] non-term semIdx=");
        print_hex((unsigned int)semIdx);
        termprint_direct("\n");
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
    /* Legge il registro CAUSE per sapere quali interrupt sono pendenti */
    unsigned int cause = getCAUSE();
    cause = cause - PRESENTFLAG;
    termprint_direct("[DEBUG] cause= "); print_hex(cause); termprint_direct("\n");

    /*
     * Controlla le interrupt line in ordine di priorità decrescente.
     *
     * CAUSE_IP_GET(cause, il_no) restituisce non-zero se l'interrupt line
     * il_no è pendente (controlla il bit il_no+8 del registro cause).
     *
     * I costanti IL_* sono definiti in const.h:
     *   IL_CPUTIMER = 7  → bit 15 del cause = PLT
     *   IL_TIMER    = 3  → bit 11 del cause = Interval Timer
     *   IL_DISK     = 17 → bit 25 ... ma in µRISC-V si usa il mapping
     *
     * Nota: in µRISC-V la codifica degli interrupt nel registro cause segue
     * lo standard RISC-V con il bit più significativo = 1 per interrupt.
     * I bit IP del cause indicano le linee di interrupt pendenti.
     */

    if (cause==0x00000007) {
        termprint_direct("[CAUSEIPGET-A] cause= ");
        /*
         * Interrupt line 1: PLT (Processor Local Timer).
         * Il time slice del Current Process è scaduto.
         * Massima priorità: gestisci subito.
         */
        handlePLT();
        /* non ritorna */

    } else if (cause==0x00000003) {
        termprint_direct("[CAUSEIPGET-B] cause= "); print_hex(CAUSE_IP_GET(cause, IL_TIMER));
        /*
         * Interrupt line 2: Interval Timer.
         * Pseudo-clock tick: sblocca tutti i processi in attesa del clock.
         */
        handleIntervalTimer();
        /* non ritorna */

    } else if (cause==0x00000011) {
        termprint_direct("[CAUSEIPGET-C] cause= "); print_hex(CAUSE_IP_GET(cause, IL_DISK));
        /*
         * Interrupt line 3: Disk devices.
         * Un'operazione su disco è completata.
         */
        handleDeviceInterrupt(INTLINE_DISK);
        /* non ritorna */

    } else if (cause==0x00000012) {
        termprint_direct("[CAUSEIPGET-D] cause= "); print_hex(CAUSE_IP_GET(cause, IL_FLASH));
        /*
         * Interrupt line 4: Flash devices.
         * Un'operazione su flash è completata.
         */
        handleDeviceInterrupt(INTLINE_FLASH);
        /* non ritorna */

    } else if (cause==0x00000013) {
        termprint_direct("[CAUSEIPGET-E] cause= "); print_hex(CAUSE_IP_GET(cause, IL_ETHERNET));
        /*
         * Interrupt line 5: Ethernet (Network) devices.
         * Un'operazione di rete è completata.
         */
        handleDeviceInterrupt(INTLINE_ETH);
        /* non ritorna */

    } else if (cause==0x00000014) {
        termprint_direct("[CAUSEIPGET-F] cause= "); print_hex(CAUSE_IP_GET(cause, IL_PRINTER));
        /*
         * Interrupt line 6: Printer devices.
         * Un'operazione di stampa è completata.
         */
        handleDeviceInterrupt(INTLINE_PRINT);
        /* non ritorna */

    } else if (cause==0x00000015) {
        termprint_direct("[CAUSEIPGET-G] cause= "); print_hex(CAUSE_IP_GET(cause, IL_TERMINAL));
        /*
         * Interrupt line 7: Terminal devices.
         * Un'operazione su terminale (TX o RX) è completata.
         * handleDeviceInterrupt gestirà internamente la distinzione TX/RX.
         */
        handleDeviceInterrupt(IL_TERMINAL);
        /* non ritorna */

    } else {
        termprint_direct("[CAUSEIPGET-H] LAST ");
        /*
         * Nessun interrupt riconosciuto: situazione anomala (spurious interrupt).
         * Restituiamo semplicemente il controllo al Current Process.
         * In un sistema robusto si potrebbe loggare questo evento.
         */
        returnToCurrentOrSchedule();
    }
}