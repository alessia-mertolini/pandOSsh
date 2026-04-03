# Documentation pandOSsh

Guida alla lettura:
Le costanti e le macro sono scritte in headers/const.h (quelle fornite in fase 1) e in phase2/headers/constants.h (quelle aggiunte in fase 2).

## Nucleus Initialization

Inizializza le variabili del nucleo secondo le specifiche fornite utilizzando le strutture dati precedentemente implementate in fase 1.
Variabili iniziallizate:
  - processCount = 0
  - softBlockCount = 0
  - readyQueue a lista vuota
  - *currentProcess = NULL
  - deviceSemaphores[SEMDEVLEN] -> tutti i semafori a 0


## The Scheduler
Implementazione di un preemptive round-robin scheduler con time slice di 5ms tramite Processor Local Timer (PLT).

**ALGORITMO DI DISPATCH**:
   1. Prende il PCB in testa alla readyQueue (priorità massima).
   2. Carica il PLT con TIMESLICE (5ms).
   3. Salva il TOD corrente in start_tod (contabilità CPU).
   4. LDST sul processor state del PCB -> il processo riprende.

**READY QUEUE VUOTA**:
  processCount == 0                -> HALT  (tutto terminato, ok)
  processCount > 0, softBlock > 0  -> WAIT  (attesa interrupt)
  processCount > 0, softBlock == 0 -> PANIC (deadlock)

  Prima di WAIT occorre:

  - disabilitare il PLT (MIE_MTIE_MASK) per evitare che il primo interrupt sia un PLT scaduto mentre non c'è currentProcess

  - abilitare la gestione globale degli interrupt (MSTATUS_MIE_MASK)


NOTA su start_tod:
  Prima di bloccare o terminare currentProcess, exceptions.c e
  interrupts.c aggiornano p_time così:
  `start_tod` e' una variabile globale del nucleo
  ```C
    cpu_t now; STCK(now);
    currentProcess->p_time += (now - start_tod);
  ```

  `start_tod` e' una variabile globale del nucleo

## Exception handling
Si considerano i codici come da specifiche effettuando uno smistamento delle cause di eccezione in :
- gestore degli interrupt -> `interruptHandler()`
- gestore delle syscall -> `syscallHandler()`
- gestore degli eventi tlb -> `tlbExceptionHandler()`
- gestore dei program trap -> `programTrapHandler()`

**Nota**: utilizziamo la macro `CAUSE_EXCCODE(cause)` per recuperare il codice di eccezione, facendo un and bit a bit con la mschera di eccezione come da specifiche

Dato che pandOSsh opera su un solo processore, in tutto il codice, quando recuperiamo lo stato salvato con `GET_EXCEPTION_STATE_PTR(cpuId)` consideriamo direttamente lo stato del processore con `cpuId = 0`, anziche' fare `cpuId = getPRID()`

## Syscall
Il gestore delle syscall (`syscallHandler()`) smista le varie system call da specifiche dopo aver aggiornate il program counter (`savedState->pc_epc += 4`). Controlla che la syscall venga chiamata in kernel mode tramite la macro `IS_KERNEL_MODE`: nel caso in cui non sia in kernel mode viene chiamato il `programTrapHandler()`. Altrimenti viene chiamata la funzione di gestione della syscall corrispondente, oppure `programTrapHandler` nel caso in cui il valore passato nel registro `a0` non sia valido. Prima di chiamare il gestore delle trap si imposta il registro cause al valore PRIVINSTR, che corrisponde all'eccezione di istruzione privilegiata.
Se invece il valore passato in `a0` non e' negativo si chiama il `passUpOrDie` come da specifiche.

## Interrupt exception handling
Il gestore degli interrupt ottiene la causa e isola il valore di eccezione con la maschera `0x7FFFFFFF`, poi una catena di if chiama il corretto gestore tramite il *codice di eccezione, non tramite la linea di interrupt*: anche tutte le macro utilizzate in seguito, seguono questa logica (`CDEV_BITMAP_ADDR` da `arch.h`).

## PassUpOrDie
Se non esiste una supportStruct il processo viene terminato tramite `terminateProcessTree` e si chiama lo scheduler, altrimenti si salva lo stato corrente dell'eccezione e si trasferisce il controllo al gestore definito nella struttura di supporto del processo.
`LDCXT(ctx->stackPtr, ctx->status, ctx->pc)` trasferisce atomicamente l'esecuzione al gestore, impostando stack pointer, status register e PC in un'unica operazione non interrompibile

## Accumulated CPU Time
Viene gestito tramite la funzione `updateCPUTime`.

## Process Termination
La terminazione dei processi avviene tramite la funzione `terminateProcessTree()` in exceptions.c. E' una funzione ricorsiva che uccide tutti i nodi figli partendo dalle foglie e annulla tutte le relazioni del `pcb_t` passato. Se questo e' il processo corrente lo resetta a NULL. Altrimenti, se non e' il processo corrente, rimuove il processo da quelli bloccati (`WAIT`) e aggiorna il `softBlockCount` di conseguenza.

## Device registers
Gestiti come da specifiche, si vede una loro gestione in `getSemaphoreIndex` la quale serve a calcolare l'indice del semaforo corrispondente ad un certo indirizzo dato.