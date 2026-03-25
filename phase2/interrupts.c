/*
 * interrupts.c
 *
 * Modulo di gestione delle interruzioni hardware per PandOSsh Phase 2.
 *
 * Implementa la sezione 7:
 *   - Sezione 7.1 : Interrupt da device periferici (disk, flash, ethernet, printer, terminal)
 *   - Sezione 7.2 : Interrupt del Processor Local Timer (PLT) → fine time slice
 *   - Sezione 7.3 : Interrupt dell'Interval Timer → Pseudo-clock tick (ogni 100ms)
 *
 * Flusso generale:
 *   exceptionHandler() [funzione in exceptions.c che deve chiamare interruptHandler() da questo modulo]
 *       - interruptHandler()   <-- entry point di questo modulo
 *               - handlePLT()           se interrupt line 1 (PLT)
 *               - handleIntervalTimer() se interrupt line 2 (Interval Timer)
 *               - handleDeviceInterrupt(intLineNo) per line 3-7
 */

#include "../headers/types.h"
#include "../headers/const.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

#include "interrupts.h"
#include "scheduler.h"

// VARIABILI GLOBALI ESTERNE --> variabili DA FARE in initial.c


// FUNZIONI INTERNE

/*
 * getSavedExceptionState()
 *
 * Restituisce un puntatore al processor state salvato al momento dell'eccezione
 * per la CPU corrente.
 */
static inline state_t *getSavedExceptionState() {
    int cpuId = getPRID(); /* ID del processore corrente (0 in Phase 2) */
    return GET_EXCEPTION_STATE_PTR(cpuId);  // = BIOSDATAPAGE + id * STATE_T_SIZE_IN_BYTES (in const.h)
}

/*
 * updateProcessTime()
 *
 * Aggiunge al campo p_time del Current Process il tempo CPU trascorso
 * dall'inizio del suo time slice corrente (misurato con il TOD clock).
 *
 * Il TOD clock conta verso l'alto in microsecondi dal boot.
 * start_tod viene impostato dallo scheduler ogni volta che dispatcha un processo.
 *
 * Questa funzione va chiamata prima di bloccare o prelazionare il processo.
 */
static void updateProcessTime() {
    if (currentProcess != NULL) {       // currentProcess = puntatore al PCB del processo in esecuzione
        cpu_t now;
        STCK(now); /* legge il valore corrente del TOD clock */
        /* Accumula il tempo trascorso dall'inizio del quantum corrente */
        currentProcess->p_time += (now - start_tod);
        /* Aggiorna start_tod per il prossimo intervallo */
        STCK(start_tod);
    }
}

/*
 * getDeviceNumber()
 *
 * Data la bitmap degli interrupt pendenti su una interrupt line,
 * restituisce il numero del device con priorità più alta (numero più basso).
 *
 * La priorità è: device 0 > device 1 > ... > device 7.
 * Si usa una serie di AND con le costanti DEV0ON..DEV7ON (da const.h).
 *
 * bitmap : valore letto dalla Interrupting Devices Bit Map per la line corrente
 * return : numero del device (0-7) con interrupt pending a priorità massima,
 *          oppure -1 se nessun bit è settato (non dovrebbe accadere).
 */
static int getDeviceNumber(unsigned int bitmap) {
    /* Controlla i device in ordine di priorità crescente del numero */
    if (bitmap & DEV0ON) return 0;
    if (bitmap & DEV1ON) return 1;
    if (bitmap & DEV2ON) return 2;
    if (bitmap & DEV3ON) return 3;
    if (bitmap & DEV4ON) return 4;
    if (bitmap & DEV5ON) return 5;
    if (bitmap & DEV6ON) return 6;
    if (bitmap & DEV7ON) return 7;
    return -1; /* nessun interrupt pending (situazione anomala) */
}

/*
 * returnToCurrentOrSchedule()
 *
 * Decisione finale dopo aver gestito un interrupt:
 *  - Se esiste un Current Process (non eravamo in wait state), esegui LDST
 *    sul saved exception state per restituirgli il controllo.
 *  - Altrimenti (lo scheduler aveva eseguito WAIT perché non c'era nulla da
 *    eseguire), chiama lo scheduler per scegliere il prossimo processo.
 *
 * Nota: questa funzione non ritorna mai al chiamante.
 */
static void returnToCurrentOrSchedule() {
    if (currentProcess != NULL) {
        /* Ripristina il processo corrente dal saved exception state */
        LDST(getSavedExceptionState());
        /* LDST non ritorna */
    } else {
        /* Nessun processo era in esecuzione: lascia decidere allo scheduler */
        scheduler();
        /* scheduler() non ritorna */
    }
}

/*
 * verhogenSemDevice()
 *
 * Esegue una operazione V (Verhogen) sul semaforo di un device.
 * Se il semaforo è ≤ 0, c'è un processo bloccato in attesa: lo sblocca,
 * inserisce il suo PCB nella ready queue e decrementa softBlockCount.
 *
 * sem      : puntatore al semaforo del device
 * statusCode : valore di stato del device da restituire al processo sbloccato
 *              (viene messo nel registro a0 del PCB sbloccato, sezione 7.1 step 5)
 *
 * return   : puntatore al PCB sbloccato, oppure NULL se nessuno era bloccato
 *            (caso "no PCB waiting", sezione 7.1 nota importante)
 */
static pcb_t *verhogenSemDevice(int *sem, unsigned int statusCode) {
    pcb_t *unblocked = NULL;

    (*sem)++; /* V: incrementa il semaforo */

    if (*sem <= 0) {
        /*
         * Il semaforo era ≤ -1 prima dell'incremento, quindi era ≤ 0 ora:
         * c'è almeno un processo bloccato su questo semaforo → sblocca il primo.
         */
        unblocked = removeBlocked(sem); /* rimuove dalla ASL il primo PCB bloccato */

        if (unblocked != NULL) {
            /* Deposita il codice di stato nel registro a0 del processo sbloccato.
             * Quando il processo riprenderà l'esecuzione, troverà ioStatus in a0
             * (spec sezione 7.1 step 5 e sezione 6.5). */
            unblocked->p_s.reg_a0 = statusCode;

            /* Azzera il semaforo su cui era bloccato (non è più bloccato) */
            unblocked->p_semAdd = NULL;

            /* Inserisce il PCB nella ready queue (transizione blocked → ready) */
            insertProcQ(&readyQueue, unblocked);

            /* Decrementa il contatore dei processi soft-blocked */
            softBlockCount--;
        }
    }

    return unblocked;
}

// GESTIONE PLT (Processor Local Timer) - Sezione 7.2

/*
 * handlePLT()
 *
 * Gestisce l'interrupt del Processor Local Timer.
 * Il PLT genera un interrupt quando il time slice da 5ms del processo corrente
 * è scaduto. Il processo deve essere prelazionato (running → ready).
 */
static void handlePLT() {
    state_t *savedState = getSavedExceptionState();

    /*
     * Step 1: Acknowledge del PLT.
     * Si carica un valore non-zero nel timer per silenziare l'interrupt pendente.
     * Usiamo il valore massimo (0xFFFFFFFF) per non generare subito un altro interrupt.
     * Lo scheduler caricherà il valore corretto (TIMESLICE) al prossimo dispatch.
     */
    setTIMER(PLT_ACK_VALUE);        // PLT_ACK_VALUE = 0xFFFFFFFF

    /*
     * Step 2: Copia il processor state al momento dell'interrupt nel PCB.
     * Il savedState contiene i registri del processo esattamente prima dell'interrupt.
     * Questo è lo stato da cui il processo riprenderà l'esecuzione in futuro.
     */
    currentProcess->p_s = *savedState;

    /*
     * Step 3: Aggiorna il tempo CPU accumulato.
     * Il processo ha usato tutto il suo time slice: contabilizziamo il tempo.
     */
    updateProcessTime();

    /*
     * Step 4: Sposta il Current Process dalla running queue alla ready queue
     * (transizione running → ready). Lo scheduler sceglierà il prossimo processo.
     */
    insertProcQ(&readyQueue, currentProcess);
    currentProcess = NULL;      // nessun processo in esecuzione ora

    /*
     * Step 5: Chiama lo scheduler per il prossimo processo
     */
    scheduler();
}

// GESTIONE INTERVAL TIMER (Pseudo-clock) - Sezione 7.3

/*
 * handleIntervalTimer()
 *
 * Gestisce l'interrupt dell'Interval Timer (ogni 100ms = PSECOND microsecondi).
 * Questo timer è riservato esclusivamente per i Pseudo-clock tick:
 * sblocca TUTTI i processi in attesa del pseudo-clock.
 *
 * Passi (sezione 7.3):
 *  1. Acknowledge: ricarica l'Interval Timer con 100ms.
 *  2. Sblocca tutti i PCB bloccati sul semaforo pseudo-clock.
 *  3. Restituisce il controllo al Current Process (o chiama lo scheduler).
 */
static void handleIntervalTimer() {
    pcb_t *unblocked;

    /*
     * Step 1: Acknowledge dell'interrupt.
     * Ricarica l'Interval Timer con PSECOND per il prossimo tick.
     * LDIT è la macro predefinita per caricare l'Interval Timer (sezione 7.3).
     */
    LDIT(PSECOND);      // PSECOND = 100000 = 100 ms

    /*
     * Step 2: Sblocca TUTTI i processi in attesa del pseudo-clock.
     *
     * A differenza dei device, qui non sblocchiamo solo il primo processo:
     * il pseudo-clock è un evento broadcast, tutti i processi in attesa
     * vengono risvegliati contemporaneamente (sezione 7.3 step 2).
     *
     * Eseguiamo V sul semaforo pseudo-clock finché ci sono processi bloccati.
     * Nota: il semaforo pseudo-clock parte da 0 (sincrono), quindi dopo NSYS7
     * vale -N dove N è il numero di processi in attesa.
     */
    while (deviceSemaphores[SEMDEV_PSEUDOCLK] <= 0) {
        /*
         * verhogenSemDevice gestisce anche l'aggiornamento di softBlockCount,
         * il movimento del PCB nella ready queue, e il deposito dello status.
         * Per il pseudo-clock lo status è 0 (nessun device).
         */
        unblocked = verhogenSemDevice(&deviceSemaphores[SEMDEV_PSEUDOCLK], 0);

        if (unblocked == NULL) {
            /*
             * Nessun processo bloccato trovato nonostante il semaforo ≤ 0:
             * situazione anomala, usciamo per sicurezza per evitare loop infiniti.
             */
            break;
        }
    }

    /*
     * Step 3: Restituisce il controllo al Current Process se esiste,
     * altrimenti chiama lo scheduler.
     * Non ritorna.
     */
    returnToCurrentOrSchedule();
}

// GESTIONE DEVICE PERIFERICI - Sezione 7.1

/*
 * handleDeviceInterrupt()
 *
 * Gestisce un interrupt da un device periferico su una delle interrupt line 3-7.
 * I device supportati sono: disk (3), flash (4), ethernet (5), printer (6),
 * terminal TX e RX (7).
 *
 * intLineNo : numero logico della interrupt line (3-7, vedi Table 1 spec)
 *
 * Passi (spec sezione 7.1):
 *  1. Legge la bitmap per trovare il device con priorità massima.
 *  2. Calcola l'indirizzo del device register.
 *  3. Salva il codice di stato dal device register.
 *  4. Acknowledge: scrive ACK nel command register del device.
 *  5. V sul semaforo del device → sblocca il processo che stava attendendo l'I/O.
 *  6. Deposita il codice di stato in a0 del processo sbloccato.
 *  7. Restituisce il controllo al Current Process (o chiama lo scheduler).
 */
static void handleDeviceInterrupt(int intLineNo) {
    /* Legge la bitmap degli interrupt pendenti per questa interrupt line */
    unsigned int bitmap = INT_BITMAP_WORD(intLineNo);

    /* Trova il numero del device con la priorità più alta (numero più basso) */
    int devNo = getDeviceNumber(bitmap);
    if (devNo < 0) {
        /*
         * Nessun device con interrupt pendente trovato: situazione anomala.
         * Restituiamo il controllo al Current Process senza fare nulla.
         */
        returnToCurrentOrSchedule();
        return; /* non raggiunto */
    }

    /*
     * Calcola l'indirizzo base del device register per questo device.
     * Formula dalla spec sezione 7.1:
     *   devAddrBase = 0x10000054 + ((IntLineNo - 3) * 0x80) + (DevNo * 0x10)
     */
    memaddr devRegAddr = DEV_REG_ADDR(intLineNo, devNo);

    if (intLineNo == INTLINE_TERM) {
        /*
         * === CASO SPECIALE: TERMINAL (interrupt line 7) ===
         *
         * I terminali hanno DUE sub-device indipendenti:
         *   - Transmitter (TX): scrittura sul terminale
         *   - Receiver (RX):   lettura dal terminale
         *
         * Struttura del terminal device register (spec sezione 12.1, Table 5):
         *   offset 0x0 → RECV_STATUS    (ricezione: stato)
         *   offset 0x4 → RECV_COMMAND   (ricezione: comando)
         *   offset 0x8 → TRANSM_STATUS  (trasmissione: stato)
         *   offset 0xC → TRANSM_COMMAND (trasmissione: comando)
         *
         * Priorità: TX > RX (spec sezione 7, ultimo paragrafo prima di 7.1).
         * Quindi controlliamo prima il trasmettitore.
         *
         * OKCHARTRANS (=5) è il codice di stato che indica trasmissione completata.
         * CHARRECV    (=5) è il codice di stato che indica ricezione completata.
         */

        /* Puntatori ai campi del terminal device register */
        volatile unsigned int *transmStatus  = (unsigned int *)(devRegAddr + 0x8);
        volatile unsigned int *transmCommand = (unsigned int *)(devRegAddr + 0xC);
        volatile unsigned int *recvStatus    = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *recvCommand   = (unsigned int *)(devRegAddr + 0x4);

        /*
         * Controlla prima il trasmettitore (priorità alta).
         * Lo stato "pronto / operazione completata" per TX è OKCHARTRANS (=5).
         * Leggiamo solo i bit bassi dello status (device completion code).
         */
        if ((*transmStatus & 0xFF) == OKCHARTRANS) {
            /* --- TX completata --- */

            /* Step 3: salva il codice di stato della trasmissione */
            unsigned int statusCode = *transmStatus;

            /* Step 4: acknowledge → scrive ACK nel command register TX */
            *transmCommand = ACK;

            /* Step 5-6: V sul semaforo TX del terminale + sblocca processo */
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMTX_BASE + devNo], statusCode);

        } else if ((*recvStatus & 0xFF) == CHARRECV) {
            /* --- RX completata (priorità bassa rispetto a TX) --- */

            /* Step 3: salva il codice di stato della ricezione */
            unsigned int statusCode = *recvStatus;

            /* Step 4: acknowledge → scrive ACK nel command register RX */
            *recvCommand = ACK;

            /* Step 5-6: V sul semaforo RX del terminale + sblocca processo */
            verhogenSemDevice(&deviceSemaphores[SEMDEV_TERMRX_BASE + devNo], statusCode);
        }
        /*
         * Nota: se né TX né RX è in stato completed, potrebbe essere un interrupt
         * spurio o il device è in un altro stato. Non facciamo nulla e ritorniamo.
         */

    } else {
        /*
         * === CASO NORMALE: device non-terminal (disk, flash, ethernet, printer) ===
         *
         * Struttura generica del device register (spec sezione 12, Table 4):
         *   offset 0x0 → STATUS
         *   offset 0x4 → COMMAND
         *   offset 0x8 → DATA0
         *   offset 0xC → DATA1
         */

        volatile unsigned int *devStatus  = (unsigned int *)(devRegAddr + 0x0);
        volatile unsigned int *devCommand = (unsigned int *)(devRegAddr + 0x4);

        /* Step 3: salva il codice di stato dal campo STATUS del device register */
        unsigned int statusCode = *devStatus;

        /*
         * Step 4: acknowledge dell'interrupt.
         * Scriviamo ACK nel campo COMMAND del device register.
         * Questo dice al controller del device che l'interrupt è stato ricevuto.
         */
        *devCommand = ACK;

        /*
         * Step 5-6: V sul semaforo del device e sblocca il processo in attesa.
         *
         * Calcola l'indice del semaforo nell'array deviceSemaphores[]:
         *   (intLineNo - 3) * DEVPERINT + devNo
         * Esempio: disk device 2 → (3-3)*8 + 2 = 2
         *          printer device 5 → (6-3)*8 + 5 = 29
         */
        int semIdx = (intLineNo - 3) * DEVPERINT + devNo;
        verhogenSemDevice(&deviceSemaphores[semIdx], statusCode);
    }

    /*
     * Step 7: restituisce il controllo al Current Process se esiste,
     * altrimenti chiama lo scheduler.
     * (spec sezione 7.1, step 7 e nota "no Current Process")
     * Non ritorna.
     */
    returnToCurrentOrSchedule();
}

// ENTRY POINT: interruptHandler() - Sezione 7

/*
 * interruptHandler()
 *
 * Entry point del modulo. Chiamata da exceptionHandler() in exceptions.c
 * quando l'eccezione corrente è un interrupt (CAUSE_IS_INT è vero).
 *
 * La funzione determina QUALE interrupt ha la priorità massima e delega
 * al gestore appropriato. La priorità è:
 *
 *   1. PLT (line 1) - priorità massima: fine time slice del processo corrente
 *   2. Interval Timer (line 2): pseudo-clock tick
 *   3. Disk (line 3) device 0-7
 *   4. Flash (line 4) device 0-7
 *   5. Ethernet (line 5) device 0-7
 *   6. Printer (line 6) device 0-7
 *   7. Terminal TX poi RX (line 7) device 0-7 - priorità minima
 *
 * Per determinare quale interrupt line è attiva usiamo getCAUSE() e i bit
 * del campo MIP (Machine Interrupt Pending), tramite i costanti IL_* che
 * indicano il bit della causa corrispondente all'interrupt line.
 */
void interruptHandler() {
    /* Legge il registro CAUSE per sapere quali interrupt sono pendenti */
    unsigned int cause = getCAUSE();

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

    if (CAUSE_IP_GET(cause, IL_CPUTIMER)) {
        /*
         * Interrupt line 1: PLT (Processor Local Timer).
         * Il time slice del Current Process è scaduto.
         * Massima priorità: gestisci subito.
         */
        handlePLT();
        /* non ritorna */

    } else if (CAUSE_IP_GET(cause, IL_TIMER)) {
        /*
         * Interrupt line 2: Interval Timer.
         * Pseudo-clock tick: sblocca tutti i processi in attesa del clock.
         */
        handleIntervalTimer();
        /* non ritorna */

    } else if (CAUSE_IP_GET(cause, IL_DISK)) {
        /*
         * Interrupt line 3: Disk devices.
         * Un'operazione su disco è completata.
         */
        handleDeviceInterrupt(INTLINE_DISK);
        /* non ritorna */

    } else if (CAUSE_IP_GET(cause, IL_FLASH)) {
        /*
         * Interrupt line 4: Flash devices.
         * Un'operazione su flash è completata.
         */
        handleDeviceInterrupt(INTLINE_FLASH);
        /* non ritorna */

    } else if (CAUSE_IP_GET(cause, IL_ETHERNET)) {
        /*
         * Interrupt line 5: Ethernet (Network) devices.
         * Un'operazione di rete è completata.
         */
        handleDeviceInterrupt(INTLINE_ETH);
        /* non ritorna */

    } else if (CAUSE_IP_GET(cause, IL_PRINTER)) {
        /*
         * Interrupt line 6: Printer devices.
         * Un'operazione di stampa è completata.
         */
        handleDeviceInterrupt(INTLINE_PRINT);
        /* non ritorna */

    } else if (CAUSE_IP_GET(cause, IL_TERMINAL)) {
        /*
         * Interrupt line 7: Terminal devices.
         * Un'operazione su terminale (TX o RX) è completata.
         * handleDeviceInterrupt gestirà internamente la distinzione TX/RX.
         */
        handleDeviceInterrupt(INTLINE_TERM);
        /* non ritorna */

    } else {
        /*
         * Nessun interrupt riconosciuto: situazione anomala (spurious interrupt).
         * Restituiamo semplicemente il controllo al Current Process.
         * In un sistema robusto si potrebbe loggare questo evento.
         */
        returnToCurrentOrSchedule();
    }
}