#include "./headers/sysSupport.h"

/* ----- PROTOTIPI ----- */

/*
 * Trasmette il carattere c sul sotto-dispositivo di trasmissione del
 * terminale devNo tramite NSYS5 (DoIO sincrono).
 *
 * Layout device register terminale (Phase 2 Sezione 12.1):
 *   +0x0  RECV_STATUS
 *   +0x4  RECV_COMMAND
 *   +0x8  TRANSM_STATUS
 *   +0xC  TRANSM_COMMAND
 *
 * Valore scritto in TRANSM_COMMAND: (char << 8) | TRANSMITCHAR(2).
 * Restituisce la status word: byte basso = stato, byte 1 = carattere eco.
 */
static int transmitChar(int devNo, char c);


/*
 * Riceve un carattere dal sotto-dispositivo di ricezione del terminale
 * devNo tramite NSYS5 (DoIO sincrono).
 *
 * Scrive RECEIVECHAR(2) in RECV_COMMAND per avviare la ricezione.
 * Restituisce la status word: byte basso = stato, byte 1 = carattere ricevuto.
 */
static int receiveChar(int devNo);


/* -----------------------------------------------------------------------
* SYS2 - Terminate (Sezione 7.1)
* ----------------------------------------------------------------------- */
/*
 * Termina l'U-proc chiamante. Non ritorna mai.
 *
 * sys2Terminate
 *
 * Operazioni comuni di terminazione (Spec Sezione 7.1 e 9):
 *   1. V sul semaforo giusto:
 *        - shell  -> V(masterSemaphore)  sblocca InstantiatorProcess
 *        - figlio -> V(shellSemaphore)   sblocca la shell in attesa su SYS6
 *   2. NSYS2 (TERMPROCESS): elimina il processo e tutti i discendenti.
 *
 * Non ritorna mai.
 */
static void sys2Terminate(support_t *sup, int holdingMutex);


/* -----------------------------------------------------------------------
* SYS4 - WriteTerminal (Sezione 7.2)
* ----------------------------------------------------------------------- */
/*
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = virtAddr   indirizzo virtuale della stringa
 *   a2 = len        numero di caratteri da trasmettere
 *
 * Errori che causano terminazione dell'U-proc (Spec Sezione 7.2):
 *   - len < 0 o len > MAXSTRLENG
 *   - virtAddr o virtAddr+len fuori da [KUSEG, USERSTACKTOP)
 *
 * Ritorno (via LDST):
 *   successo:    a0 = numero caratteri trasmessi
 *   errore I/O:  a0 = -(status word del device)
 */
static void sys4WriteTerminal(support_t *sup);


/* -----------------------------------------------------------------------
* SYS5 - ReadTerminal (Sezione 7.3)
* ----------------------------------------------------------------------- */
/*
* Parametri da sup_exceptState[GENERALEXCEPT]:
*   a1 = virtAddr   indirizzo virtuale del buffer di destinazione
*
* Legge caratteri finché non arriva '\n' o si raggiunge MAXSTRLENG.
 *
 * Errori che causano terminazione dell'U-proc (Spec Sezione 7.3):
 *   - virtAddr fuori da [KUSEG, USERSTACKTOP)
 *
 * Ritorno (via LDST):
 *   successo:    a0 = numero caratteri ricevuti
 *   errore I/O:  a0 = -(status word del device)
 */
static void sys5ReadTerminal(support_t *sup);


/* -----------------------------------------------------------------------
 * SYS6 - Execute (Sezione 7.4)
----------------------------------------------------------------------- */
/*
 * Parametri da sup_exceptState[GENERALEXCEPT]:
 *   a1 = asid   ASID dell'U-proc da avviare
 *
 * Solo la shell può chiamare SYS6; altrimenti si tratta come Program Trap.
 *
 * Flusso:
 *   1. launchUProc(asid): crea il processo tramite NSYS1 (in initProc.c).
 *   2. P(shellSemaphore): blocca la shell.
 *   3. Il figlio al termine chiama SYS2 -> V(shellSemaphore): risveglia la shell.
 *   4. Ritorna con a0 = 0, PC += 4.
 */
static void sys6Execute(support_t *sup);


/* -----------------------------------------------------------------------
 * Sezione 7 - SYSCALL Exception Handler
 * ----------------------------------------------------------------------- */
/**
 * Gestisce le eccezioni SYSCALL (identificatori > 0) delegate dal Nucleus[cite: 227, 234].
 * Fornisce i servizi di Support Level (SYS2, SYS4, SYS5, SYS6) per l'U-proc corrente 
 * smistando la richiesta in base al codice operativo presente nel registro a0[cite: 238, 241].
*/
static void syscallExceptionHandler(support_t *sup);


/* ----- IMPLEMENTAZIONI ----- */
extern void releaseFramesOfProcess(int asid); // importata da vmSupport.h
extern support_t supportStructPool[];


// Ritorna 1 se il processo è la shell (ASID == SHELL_ASID).
static int isShellProcess(support_t *sup) {
    return (sup->sup_asid == SHELL_ASID);
}


static int transmitChar(int devNo, char c) {
    unsigned int baseAddr = TERM0ADDR + (devNo * 0x10);
    termreg_t *termReg = (termreg_t *)baseAddr;
    
    /* Costruisce il comando: byte alto = carattere, byte basso = TRANSMITCHAR. 
    *  Esempio: c -> 97 -> 0x61 -> 0x00000061
    *   Shift: 0x00006100
    *   Or: 0x00006102    
    */
    unsigned int command = ((unsigned int)(unsigned char)c << TERM_CHAR_SHIFT) | TRANSMITCHAR;

    return SYSCALL(DOIO, (int)&(termReg->transm_command), (int)command, 0);
}


static int receiveChar(int devNo) {
    unsigned int baseAddr = TERM0ADDR + (devNo * 0x10);
    termreg_t *termReg = (termreg_t *)baseAddr;
     
    return SYSCALL(DOIO, (int)&(termReg->recv_command), RECEIVECHAR, 0);
}


static void sys2Terminate(support_t *sup, int holdingMutex) {
     if (holdingMutex) {
        /* Il chiamante (pager) tiene già il mutex: rilascia i frame e poi V. */
        releaseFramesOfProcess(sup->sup_asid);
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
        } else {
        /* Nessun mutex tenuto: acquisiscilo, rilascia i frame, poi V. */
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0, 0);
        releaseFramesOfProcess(sup->sup_asid);
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
    }
    
    if (isShellProcess(sup)) { // process is shell
        SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
    } else { // user process was started by shell
        SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
    }
    
    SYSCALL(TERMPROCESS, 0, 0, 0);
}


static void sys4WriteTerminal(support_t *sup) {
    state_t *savedState = &(sup->sup_exceptState[GENERALEXCEPT]);
    unsigned int virtAddr   = savedState->reg_a1; // primo carattere trasmesso
    int len = (int) savedState->reg_a2;
    
    /* Il terminale della shell è il numero 0. */
    int devNo = SHELL_TERMINAL;
    
    /* Validazione lunghezza. */
    if (len < 0 || len > MAXSTRLENG) {
        sys2Terminate(sup, 0);
        /* non ritorna */
    }
    
    /* Validazione indirizzo: la stringa deve stare interamente in kuseg. */
    if (virtAddr < KUSEG || virtAddr + len > USERSTACKTOP) {
        sys2Terminate(sup, 0);
        /* non ritorna */
    }

    /* Trasmissione carattere per carattere. */
    char *str = (char *)virtAddr;
    
    int transmitted = 0;
    int i;

    SYSCALL(PASSEREN, (int)&writeTerminalSemaphore, 0, 0);
    for (i = 0; i < len; i++) {
        char c = str[i]; 
        
        int status = transmitChar(devNo, c);
        
        /* Controlla solo il byte di stato. */
        if ((status & TERM_STATUS_MASK) != OKCHARTRANS) {
            /* Errore device: ritorna il negativo dello status word. */
            savedState->reg_a0 = (unsigned int)(-(status));
            savedState->pc_epc += WORDLEN;
            SYSCALL(VERHOGEN,(int)&writeTerminalSemaphore, 0,0);
            LDST(savedState);
            /* non ritorna */
        }
        transmitted++;
    }
    
    /* Successo. */
    savedState->reg_a0 = (unsigned int)transmitted;
    savedState->pc_epc += WORDLEN;

    SYSCALL(VERHOGEN, (int)&writeTerminalSemaphore, 0, 0);
    LDST(savedState);
}


static void sys5ReadTerminal(support_t *sup) {
    state_t *savedState = &sup->sup_exceptState[GENERALEXCEPT];
    char    *virtAddr   = (char *)savedState->reg_a1;

    if (savedState->pc_epc == 0) {
        PANIC();  // si ferma qui invece di crashare l'emulatore
    }

    int devNo = SHELL_TERMINAL;
    
    /* Validazione indirizzo. */
    if ((unsigned int)virtAddr < KUSEG ||(unsigned int)virtAddr >= USERSTACKTOP) {
        sys2Terminate(sup, 0);
        /* non ritorna */
    }
    
    int received = 0;
    SYSCALL(PASSEREN, (int)&readTerminalSemaphore, 0, 0);
    while (1) {
        int  status = receiveChar(devNo);
        int  stCode = status & TERM_STATUS_MASK;
        char c      = (char)((status & TERM_CHAR_MASK) >> TERM_CHAR_SHIFT);
        
        if (stCode != OKCHARTRANS) {
            /* OKCHARTRANS == 5 vale sia per trasmissione che ricezione
            * (CHARRECV == 5 è lo stesso valore in pandos_const.h). */
            savedState->reg_a0 = (unsigned int)(-(status));
            savedState->pc_epc += WORDLEN;
            SYSCALL(VERHOGEN,(int)&readTerminalSemaphore, 0,0);
            LDST(savedState);
            /* non ritorna */
        }

        virtAddr[received] = c;
        received++;

        /* Fine riga o buffer pieno. */
        if (c == '\n' || received >= MAXSTRLENG) {
            break;
        }
    }
    
    savedState->reg_a0 = (unsigned int)received;
    savedState->pc_epc += WORDLEN;

    SYSCALL(VERHOGEN, (int)&readTerminalSemaphore, 0, 0);
    LDST(savedState);
}


static void sys6Execute(support_t *sup) {
    state_t *savedState = &sup->sup_exceptState[GENERALEXCEPT];
    int requested_asid = (int)savedState->reg_a1;
    
    /* Solo la shell può eseguire SYS6 (Spec Sezione 7.4). */
    if (!isShellProcess(sup)) {
        programTrapSupportHandler(sup, 0);
        /* non ritorna */
    }
    
    int safe_asid = 2 + ((requested_asid - 2) % (UPROCMAX - 1));
    
    /* Crea e lancia l'U-proc (initProc.c). */
    support_t *childSup = launchUProc(safe_asid);
    
    /* Blocca la shell fino alla terminazione del figlio. */
    SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);
    
    deallocSupportStruct(childSup);

    /* Figlio terminato: riprende la shell. */
    savedState->reg_a0 = 0;
    savedState->pc_epc += WORDLEN;
    
    LDST(savedState);
}


void programTrapSupportHandler(support_t *sup, int holdingMutex) {
    if (holdingMutex) {
        // Il pager (chiamante) tiene già il mutex: rilascia i frame e poi fa V
        releaseFramesOfProcess(sup->sup_asid);
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
    } else {
        // Nessun mutex tenuto: acquisiscilo, rilascia i frame, poi fai V
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0, 0);
        releaseFramesOfProcess(sup->sup_asid);
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
    }
    
    if (isShellProcess(sup)) { // process is shell
        SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
    } else { // user process was started by shell
        // deallocSupportStruct(sup);  // ← restituisce alla free list
        SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
    }
    
    SYSCALL(TERMPROCESS, 0, 0, 0);
    /* UNREACHABLE */
}
 

void generalExceptionHandler(void) {
    /* Recupera la Support Structure tramite NSYS8. */
    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Caso anomalo: il Nucleus non avrebbe dovuto passare qui senza sup. */
    if (sup == NULL) {
        SYSCALL(TERMPROCESS, 0, 0, 0);
        /* non ritorna */
    }

    /* Legge l'exception code dal Cause register dello stato salvato.
     * GETEXECCODE (0x0000007C) maschera i bit 2-6; poi shift di CAUSESHIFT(2)
     * per ottenere il valore numerico del codice. */
    unsigned int cause   = sup->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int excCode = CAUSE_EXCCODE(cause);

    if (excCode == SYSEXCEPTION) {
        /* SYSCALL con a0 > 0 (Sezione 7). */
        syscallExceptionHandler(sup);
    } else {
        /* Program Trap (Sezione 8). */
        programTrapSupportHandler(sup, 0);
    }

    /* non ritorna mai */
}


static void syscallExceptionHandler(support_t *sup) {
    state_t *savedState = &(sup->sup_exceptState[GENERALEXCEPT]);
    int sysNum = (int)savedState->reg_a0;

    switch (sysNum) {
        case TERMINATE:
            sys2Terminate(sup, 0);
            break;
        case WRITETERMINAL:
            sys4WriteTerminal(sup);
            break;
        case READTERMINAL:
            sys5ReadTerminal(sup);
            break;
        case EXECUTE:
            sys6Execute(sup);
            break;
        default:
            /* SYSCALL non supportata: trattato come Program Trap. */
            programTrapSupportHandler(sup, 0);
            break;
    }
}
