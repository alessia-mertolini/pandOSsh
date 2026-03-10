/* =========================================================================
 * exceptions.c  –  Phase 2: Nucleus Exception & SYSCALL Handlers
 *
 * Modulo responsabile di:
 *   1. uTLB_RefillHandler  : placeholder per il TLB-Refill (Fase 2)
 *   2. exceptionHandler    : dispatcher principale (entry point da Pass Up Vector)
 *   3. syscallHandler      : routing e implementazione delle NSYS1-NSYS10
 *   4. programTrapHandler  : Pass Up or Die per Program Trap
 *   5. tlbExceptionHandler : Pass Up or Die per eccezioni TLB
 *
 * Dipendenze Fase 2:
 *   - constants.h  : variabili globali extern, macro di utilità
 *   - scheduler.h  : funzione scheduler() chiamata dopo syscall bloccanti
 * ========================================================================= */

#include "./exceptions.h"
#include "./scheduler.h"
#include "./headers/constants.h"
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

//Prototipi delle SysCall visibili SOLO in questo file "exceptions.c"
//-------
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

/*-- INIZIO DELLA DEFINIZIONE DEI MODULI --*/

/*-- 1 uTLB_RefillHandler – Placeholder - Gestito in Fase 3 -- */
void uTLB_RefillHandler(void) {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *) BIOSDATAPAGE);
}

/* -- 2 exceptionHandler – Dispatcher principale delle eccezioni --
 * Entry point chiamato dall'hardware dopo ogni eccezione ad eccezzione di TLB-Refill
 * Lo stato del processore al momento dell'eccezione è già stato salvato
 * dal BIOS nella BIOS Data Page (BIOSDATAPAGE, processore 0)
 * Legge il campo cause dallo stato salvato per determinare il tipo di
 * eccezione e smista al gestore appropriato */
void exceptionHandler(void) {
    // Recupera lo stato salvato dal BIOS Data Page per il processore 0
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);

    unsigned int cause = savedState->cause;
    unsigned int excCode = CAUSE_EXCCODE(cause);

    if (CAUSE_IS_INT(cause)) { //L'eccezione in questione è un INTERRUPT
        interruptHandler();

    } else if (excCode == EXC_SYSCALL || excCode == EXC_SYSCALL_U) { //SYSCALL
        syscallHandler();

    } else if (excCode >= EXC_TLB_MIN && excCode <= EXC_TLB_MAX) { //Eccezioni TLB (codici 24-28)
        tlbExceptionHandler();

    } else { // Program Trap (codici 0-7, 9, 10, 12-23)
        programTrapHandler();
    }
}

/* -- 3 syscallHandler – Dispatcher e gestione delle NSYS1-NSYS10 --
 * si controlla:
 *   a) se il processo era in kernel-mode (MPP == M-mode in mstatus)
 *      se era in user-mode con numero negativo → simula PRIVINSTR (Istruzione Privilegiata)
 *   b) il numero di syscall nel registro a0 dello stato salvato
 *      1 - valori negativi => NSYS del Nucleus
 *      2 - valori >= 1 => Pass Up or Die (GENERALEXCEPT)
 *      3 - qualsiasi altro valore non gestito => simula PRIVINSTR */
void syscallHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);

    int syscallNum = (int)savedState->reg_a0;
    int kernelMode = IS_KERNEL_MODE(savedState->status);
    savedState->pc_epc += WORD_SIZE; //Avanza il PC di 4: evita il re-eseguimento della SYSCALL al ritorno

    //gestione delle systemcall
    if (syscallNum < 0) { //caso b1) - Syscall con numero < 0 vengono dal Nucleus => Pass Up or Die
        if (!kernelMode) { //caso b3) - Syscall con valore non gestito - simulata una istruzione privilegiata
            //provesso user-mode che chiede un servizio privilegiato => simulato una program trap priviledged
            savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
            programTrapHandler();
            return;
        }
        //Dispatch alle singole SYSCALL
        switch (syscallNum) {
            case CREATEPROCESS:
                createProcess(savedState);    
                break;
            case TERMINATEPROCESS:
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
            case GETCPUTIME:
                getCPUTime(savedState);
                break;
            case WAITCLOCK:
                waitForClock(savedState);
                break;
            case GETSUPPORTPTR:
                getSupportData(savedState);
                break;
            case GETPID:
                getProcessID(savedState);
                break;
            case YIELD:
                yield(savedState);
                break;

            default: //Numero di syscall privilegiata non riconosciuto => Program Trap
                savedState->cause = (savedState->cause & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
                programTrapHandler();
                break;
        }
    } else { //caso b2) - Syscall con numero >= 1: non gestite dal Nucleus => Pass Up or Die
        passUpOrDie(GENERALEXCEPT, savedState);
    }
}

/* -- 4 programTrapHandler – Pass Up or Die per Program Trap --
 * Eccezioni con codici 0-7, 9, 10, 12-23 e valori non gestiti da syscall
 * Usa l'indice GENERALEXCEPT per la Support Structure */
void programTrapHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(GENERALEXCEPT, savedState);
}

/* -- 5 tlbExceptionHandler – Pass Up or Die per eccezioni TLB --
 * Eccezioni con codici 24 - 28
 * Usa l'indice PGFAULTEXCEPT per la Support Structure */
void tlbExceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(PGFAULTEXCEPT, savedState);
}

/* -- Implementazione delle singole SYSCALL (funzioni static)-- 
 * SysCall bloccanti => richiamano scheduler
*/

/* -------------------------------------------------------------------------
 * NSYS1 – CreateProcess
 *
 * crea un nuovo processo figlio del processo corrente
 *   a1 => puntatore allo stato iniziale del nuovo processo (state_t *)
 *   a2 => priorità del nuovo processo (int)
 *   a3 => puntatore alla Support Structure (support_t *), può essere NULL
 *
 * Ritorna in a0 del processo chiamante:
 *   il PID del nuovo processo se ha successo
 *   -1 se non ci sono PCB liberi
 * ------------------------------------------------------------------------- */
static void createProcess(state_t *savedState) {
    state_t *childState = (state_t *) savedState->reg_a1;
    int priority = (int) savedState->reg_a2;
    support_t *supportPtr = (support_t *) savedState->reg_a3;
    pcb_t *newPcb = allocPcb();
    if (newPcb == NULL) {
        savedState->reg_a0 = (unsigned int) -1; //nessun PCB disponibile, restituisco errore
        LDST(savedState); //ritorno al processo chiamante
        return; 
    }
    //Inizializzazione del nuovo PCB 
    newPcb->p_s = *childState;
    newPcb->p_prio = priority;
    newPcb->p_supportStruct = supportPtr;
    newPcb->p_time = 0;
    newPcb->p_semAdd = NULL;
    //Inserimento nella gerarchia e nella ready queue
    insertChild(currentProcess, newPcb);
    insertProcQ(&readyQueue, newPcb);
    processCount++;
    // Restituisce il PID del nuovo processo
    savedState->reg_a0 = (unsigned int) newPcb->p_pid;
    //Ritorna al processo chiamante
    LDST(savedState);
}

/* -------------------------------------------------------------------------
 * NSYS2 – TerminateProcess
 *
 * Termina il processo indicato e tutti i suoi discendenti
 *   a1 => PID del processo da terminare (0 = processo corrente)
 *
 * Dopo la terminazione chiama lo Scheduler
 * ------------------------------------------------------------------------- */
static void terminateProcess(state_t *savedState) {
    int targetPid = (int) savedState->reg_a1;

    if (targetPid == 0) {
        //Termina il processo corrente
        terminateProcessTree(currentProcess);
        currentProcess = NULL;
    } else {
        //Si ricerca il target scansionando la ready Queue
        pcb_t *target = NULL;
        //Cerca nella ready queue
        pcb_t *iter; //iteratore necessario alla macro
        list_for_each_entry(iter, &readyQueue, p_list) { //macro di listx.h
            if (iter->p_pid == targetPid) {
                target = iter;
                break;
            }
        }
        //Se non trovato nella ready queue, controlla se è il processo corrente
        if (target == NULL && currentProcess != NULL && currentProcess->p_pid == targetPid) {
            target = currentProcess;
        }
        if (target != NULL) { //Se si è rinvenuto un target durante la ricerca lo si elimina
            if (target == currentProcess) {
                terminateProcessTree(currentProcess);
                currentProcess = NULL;
            } else {
                terminateProcessTree(target);
            }
        }
        //Se la ricerca non ha successo perché il PID non esisteva non si fa nulla
    }
    scheduler(); //si richiama lo scheduler
}

/* -------------------------------------------------------------------------
 * NSYS3 – Passeren (P)
 *
 * Operazione P (decrementa) su un semaforo
 *   a1 => indirizzo fisico del semaforo (int *)
 *
 * Se il valore del semaforo diventa < 0, blocca il processo corrente
 * ------------------------------------------------------------------------- */
static void passeren(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;
    (*semAddr)--; //decremento (P)
    if (*semAddr < 0) {
        //Blocca il processo corrente
        updateCPUTime();
        //Copia lo stato aggiornato (con PC già incrementato da syscallhandler) nel PCB
        currentProcess->p_s = *savedState;
        //Inserisce il processo nell'ASL sul semaforo indicato
        insertBlocked(semAddr, currentProcess);
        softBlockCount++;
        currentProcess = NULL;
        scheduler();
    } else {
        //Semaforo libero, ritorna immediatamente al processo
        LDST(savedState);
    }
}

/* -------------------------------------------------------------------------
 * NSYS4 – Verhogen (V)
 *
 * Operazione V (incrementa) su un semaforo.
 * a1 => indirizzo fisico del semaforo (int *)
 *
 * Se c'è un processo bloccato, lo sblocca e lo mette nella Ready Queue.
 * Non blocca mai il processo corrente.
 * ------------------------------------------------------------------------- */
static void verhogen(state_t *savedState) {
    int *semAddr = (int *) savedState->reg_a1;

    (*semAddr)++; //incremento (V)

    if (*semAddr <= 0) {
        //Se c'è almeno un processo in attesa lo sblocca
        pcb_t *unblocked = removeBlocked(semAddr);
        if (unblocked != NULL) {
            unblocked->p_semAdd = NULL;
            softBlockCount--;
            insertProcQ(&readyQueue, unblocked);
        }
    }
    //Ritorna al processo corrente
    LDST(savedState);
}

/* -------------------------------------------------------------------------
 * NSYS5 – DoIO
 *
 * Avvia un'operazione I/O sincrona e blocca il processo corrente.
 *   a1 → indirizzo del campo command del device register (int *)
 *   a2 → valore da scrivere nel campo command (int)
 *
 * Il processo rimane bloccato finché il dispositivo non genera un interrupt.
 * Al risveglio, il codice di stato del dispositivo è restituito in a0.
 * ------------------------------------------------------------------------- */
static void doIO(state_t *savedState) {
    int *commandAddr = (int *) savedState->reg_a1;
    int commandValue = (int) savedState->reg_a2;
    //Determina l'indice del semaforo per questo dispositivo
    int semIdx = getSemaphoreIndex(commandAddr);
    //Blocca il processo corrente prima di inviare il comando, il semaforo è init a 0, quindi P porta sempre a blocco
    updateCPUTime();
    currentProcess->p_s = *savedState;

    deviceSemaphores[semIdx]--;
    insertBlocked(&deviceSemaphores[semIdx], currentProcess);
    softBlockCount++;

    //Invia il comando al dispositivo (avvia l'I/O)
    *commandAddr = commandValue;

    currentProcess = NULL;

    scheduler();
}

/* -------------------------------------------------------------------------
 * NSYS6 – GetCPUTime
 *
 * Restituisce in a0 il tempo CPU accumulato dal processo corrente
 * Include anche il tempo del quanto corrente (non ancora contabilizzato)
 * ------------------------------------------------------------------------- */
static void getCPUTime(state_t *savedState) {
    cpu_t currentTOD;
    STCK(currentTOD);
    //tempo già accomulato (p_time) + tempo trascorso nel quanto corrente
    cpu_t totalTime = currentProcess->p_time + (currentTOD - start_tod);
    savedState->reg_a0 = (unsigned int) totalTime;

    //Ritorna al processo corrente
    LDST(savedState);
}

/* -------------------------------------------------------------------------
 * NSYS7 – WaitForClock
 *
 * Blocca il processo corrente sul semaforo del Pseudo-clock.
 * Il semaforo viene subisce una V ogni 100ms dal ProcessLocalTimer
 * ------------------------------------------------------------------------- */
static void waitForClock(state_t *savedState) {
    updateCPUTime();
    currentProcess->p_s = *savedState;

    deviceSemaphores[PSEUDOCLOCK_SEM_IDX]--;
    insertBlocked(&deviceSemaphores[PSEUDOCLOCK_SEM_IDX], currentProcess);
    softBlockCount++;

    currentProcess = NULL;
    scheduler();
}

/* ------------------------------MODULI-------------------------------------------
 * NSYS8 – GetSupportData
 *
 * Restituisce in a0 il puntatore alla Support Structure del processo corrente.
 * Se il processo non ha una Support Structure, restituisce NULL.
 * ------------------------------------------------------------------------- */
static void getSupportData(state_t *savedState) {
    savedState->reg_a0 = (unsigned int) currentProcess->p_supportStruct;
    //Ritorna al processo corrente
    LDST(savedState);
}

/* -------------------------------------------------------------------------
 * NSYS9 – GetProcessID
 *
 * Restituisce in a0 il PID del processo corrente o del suo padre
 *   a1 => 0: restituisce il PID del processo corrente
 *   a1 => 1: restituisce il PID del processo padre.
 *   se il processo non ha un padre si ritorna 0
 * ------------------------------------------------------------------------- */
static void getProcessID(state_t *savedState) {
    int parent = (int) savedState->reg_a1; //viene utilizzato come flag
    if (parent == 0) { //se il flag è zero si ritorna il pd del processo
        savedState->reg_a0 = (unsigned int) currentProcess->p_pid;
    } else {
        if (currentProcess->p_parent != NULL) { //altrimenti si ritorna il pd del processo padre
            savedState->reg_a0 = (unsigned int) currentProcess->p_parent->p_pid;
        } else { //ritorno 0 nel caso di processo radice
            savedState->reg_a0 = 0; //processo radice non ha nessun padre, ritorno 0
        }
    }
    //Ritorna al processo corrente
    LDST(savedState);
}

/* -------------------------------------------------------------------------
 * NSYS10 – Yield
 *
 * Il processo corrente cede volontariamente la CPU
 * Viene reinserito nella Ready Queue rispettando priorità
 * ------------------------------------------------------------------------- */
static void yield(state_t *savedState) {
    updateCPUTime();
    currentProcess->p_s = *savedState;
    insertProcQ(&readyQueue, currentProcess); //a parità di priorità, ha precedenza quello già inserito
    currentProcess = NULL;

    scheduler();
}

/*-- FINE DELLA DEFINIZIONE DEI MODULI --*/

/*-- INIZIO DELLA DEFINIZIONE DELLE FUNZIONI DI SUPPORTO --*/

/* -- Funzioni di supporto (static) -- */

static void passUpOrDie(int exceptionType, state_t *savedState) {
    if (currentProcess->p_supportStruct == NULL) { //caso die, non si ha una supportStruct
        terminateProcessTree(currentProcess); //termino il processo e il suo ramo
        currentProcess = NULL;
        scheduler();
    } else { //altrimenti il processo viene passato "verso l'alto"
        //copia lo stato dell'eccezione nella supportStruct
        currentProcess->p_supportStruct->sup_exceptState[exceptionType] = *savedState;
        //Trasferimento del controllo al supportLevel
        //context_t è un indirizzo di destinazione completo con ProgramCounter e StackPointer e con quali permessi
        context_t *ctx = &currentProcess->p_supportStruct->sup_exceptContext[exceptionType]; //recupero del contesto
        LDCXT(ctx->stackPtr, ctx->status, ctx->pc); //il salto, non viene usato LDST perché bisogna AVVIARE
        //un nuovo contesto e non RIPRENDERLO da dove era stato interrotto
    }
}

//termina un sottoalbero di processi
static void terminateProcessTree(pcb_t *root) {
    //Termina prima tutti i figli, poi termina il nodo radice
    while (!emptyChild(root)) {
        pcb_t *child = removeChild(root);
        terminateProcessTree(child);
    }
    //Distaccamento del nodo root dal suo genitore
    if (root->p_parent != NULL) {
        outChild(root);
    }

    //Rimuove root da qualsiasi struttura in cui si trova 
    if (root == currentProcess) {
        //sarà compito del chiamante impostare currentProcess = NULL
    } else if (root->p_semAdd != NULL) {
        outBlocked(root); //rimozione processo dalla coda del semaforo su cui è bloccato
        //distinzione tra semafori dei dispositivi e semafori utente
        int *base = &deviceSemaphores[0];
        int *end = &deviceSemaphores[SEMDEVLEN];
        if (root->p_semAdd >= base && root->p_semAdd < end) { //caso semaforo di dispositivo
            softBlockCount--;
        }
    } else {
        //Il processo è nella ready queue
        outProcQ(&readyQueue, root);
    }
    processCount--;
    freePcb(root);
}


//aggiorna p_time del processo corrente con il TOD clock
static void updateCPUTime(void) {
    cpu_t now;
    STCK(now);
    currentProcess->p_time += (now - start_tod);
    start_tod = now; //riferimento per il prossimo intervallo
}

//commandAddr punta al campo command dentro a un device register, non all'inizio del device register.
//Bisogna quindi trovare dove inizia il devide register che lo contiene
static int getSemaphoreIndex(int *commandAddr) {
    unsigned int addr = (unsigned int) commandAddr; //campo command dentro a un device register, non all'inizio del device register
    unsigned int regBase = addr & ~0xF; //inizio del device register. Prendo l'indirizzo e faccio una AND bit-a-bit trovando l'indirizzo corretto

    //START_DEVREG: inizio del primo device register in assoluto
    unsigned int offset = regBase - START_DEVREG; //otteniamo quindi posizione relativa di questo device register nell'area di memoria dedicata ai dispositivi
    int DevNo = (int)((offset % 0x80) / 0x10); //0x10:= dimensione di un device register. Così otteniamo il numero del device
    // int IntLineNo = ((offset - (DevNo * 0x10)) / 0x80) + 3; // formula inversa del del calcolo del devAddrBase
    int intLineNo = (int)(offset / 0x80) + 3; //la divisione intera per 0x80 da il blocco. Viene aggiunto 3 perché le interrupt line partono da 3
    //guardare constants.h per vedere correttamente i blocchi
    int semIdx = -1;
    if (intLineNo < 7) { //standard device delle linee 3-4-5-6
        //all'interno del deviceSemaphores saranno nell'ordine delle linee di interrupt
        //ogni linea continene 8 devices + sommiamo DevNo per il device correttol
        semIdx = (intLineNo - 3) * 8 + DevNo;
    } else{ //terminal device
        int internalOffset = offset - regBase; //identifica il sub-device corrispondente
        if(internalOffset == 0x4){ //RECV_COMMAND
            //32 perché siamo nella linea / e quindi quanto meno siamo dopo 4(linnee) * 8(devices)
            semIdx = 32 + DevNo;
        } else{ //TRANSM_COMMAND
            semIdx = 40 + DevNo;
        }
    }

    /* Dispositivo non-terminale (linee 3-6): indice diretto */
    return semIdx;
}

/*-- FINE DELLA DEFINIZIONE DELLE FUNZIONI DI SUPPORTO --*/