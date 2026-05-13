# Documentation pandOSsh

## PHASE 2

Guida alla lettura:
Le costanti e le macro sono scritte in headers/const.h (quelle fornite in fase 1) e in phase2/headers/constants.h (quelle aggiunte in fase 2).

## Nucleus Initialization

Inizializza le variabili del nucleo secondo le specifiche fornite utilizzando le strutture dati precedentemente implementate in fase 1.
Variabili iniziallizate:
  - `processCount = 0`
  - `softBlockCount = 0`
  - `readyQueue` a lista vuota
  - `*currentProcess = NULL`
  - `deviceSemaphores[SEMDEVLEN]` -> tutti i semafori a 0


## The Scheduler
Implementazione di un preemptive round-robin scheduler con time slice di 5ms tramite Processor Local Timer (PLT).

**ALGORITMO DI DISPATCH**:
   1. Prende il PCB in testa alla coda ready (priorità massima).
   2. Carica il PLT con TIMESLICE (5ms).
   3. Salva il TOD corrente in `start_tod` (contabilità CPU).
   4. `LDST` sul processor state del PCB -> il processo riprende.

**READY QUEUE VUOTA**:
  - `processCount == 0`                -> HALT  (tutto terminato, ok)
  - `processCount > 0, softBlock > 0`  -> WAIT  (attesa interrupt)
  - `processCount > 0, softBlock == 0` -> PANIC (deadlock)

  Prima di WAIT occorre:

  - disabilitare il PLT (`MIE_MTIE_MASK`) per evitare che il primo interrupt sia un PLT scaduto mentre non c'è `currentProcess`

  - abilitare la gestione globale degli interrupt (`MSTATUS_MIE_MASK`)


NOTA su `start_tod`:
  prima di bloccare o terminare `currentProcess`, exceptions.c e
  interrupts.c aggiornano `p_time` così:
  ```C
    cpu_t now; STCK(now);
    currentProcess->p_time += (now - start_tod);
  ```
  `start_tod` e' una variabile globale del nucleo.

## Exception handling
Si considerano i codici come da specifiche effettuando uno smistamento delle cause di eccezione in :
- gestore degli interrupt -> `interruptHandler()`
- gestore delle syscall -> `syscallHandler()`
- gestore degli eventi tlb -> `tlbExceptionHandler()`
- gestore dei program trap -> `programTrapHandler()`

**Nota**: utilizziamo la macro `CAUSE_EXCCODE(cause)` per recuperare il codice di eccezione, facendo un and bit a bit con la mschera di eccezione come da specifiche

Dato che pandOSsh opera su un solo processore, in tutto il codice, quando recuperiamo lo stato salvato con `GET_EXCEPTION_STATE_PTR(cpuId)` consideriamo direttamente lo stato del processore con `cpuId = 0`, anziche' fare `cpuId = getPRID()`

## Syscall
Il gestore delle syscall (`syscallHandler()`) smista le varie system call da specifiche dopo aver aggiornate il program counter (`savedState->pc_epc += 4`). Controlla che la syscall venga chiamata in kernel mode tramite la macro `IS_KERNEL_MODE`: nel caso in cui non sia in kernel mode viene chiamato il `programTrapHandler()`. Altrimenti viene chiamata la funzione di gestione della syscall corrispondente, oppure `programTrapHandler()` nel caso in cui il valore passato nel registro `a0` non sia valido. Prima di chiamare il gestore delle trap si imposta il registro cause al valore `PRIVINSTR`, che corrisponde all'eccezione di istruzione privilegiata.
Se invece il valore passato in `a0` non e' negativo si chiama il `passUpOrDie` come da specifiche.

## Interrupt exception handling
Il gestore degli interrupt ottiene la causa e isola il valore di eccezione con la maschera `0x7FFFFFFF`, poi una catena di if chiama il corretto gestore tramite il *codice di eccezione, non tramite la linea di interrupt*: anche tutte le macro utilizzate in seguito, seguono questa logica (`CDEV_BITMAP_ADDR` da `arch.h`).

## PassUpOrDie
Se non esiste una `supportStruct` il processo viene terminato tramite `terminateProcessTree()` e si chiama lo scheduler, altrimenti si salva lo stato corrente dell'eccezione e si trasferisce il controllo al gestore definito nella struttura di supporto del processo.
`LDCXT(ctx->stackPtr, ctx->status, ctx->pc)` trasferisce atomicamente l'esecuzione al gestore, impostando stack pointer, status register e PC in un'unica operazione non interrompibile

## Accumulated CPU Time
Viene gestito tramite la funzione `updateCPUTime()`.

## Process Termination
La terminazione dei processi avviene tramite la funzione `terminateProcessTree()` in exceptions.c. E' una funzione ricorsiva che uccide tutti i nodi figli partendo dalle foglie e annulla tutte le relazioni del `pcb_t` passato. Se questo e' il processo corrente lo resetta a `NULL`. Altrimenti, se non e' il processo corrente, rimuove il processo da quelli bloccati (`WAIT`) e aggiorna il `softBlockCount` di conseguenza.

## Device registers
Gestiti come da specifiche, si vede una loro gestione in `getSemaphoreIndex` la quale serve a calcolare l'indice del semaforo corrispondente ad un certo indirizzo dato.

## PHASE 3
### exception.c - phase 3
`utlbRefillHandler` seppur si riferisce a fase 3 viene scritta in `exception.c` in quanto fa da ponte tra fase 2 e fase 3.  
Sostanzialmente cio' viene fatto in quanto funzione `utlbRefillHandler` necessita di conoscere il current process.

### initProc.c
In questo file .c viene implementata una delle ottimizzazioni consigliate. In particolare, la funzionalita' dell'allocazione e deallocazione della  
`supportStructPool`, invece del precedente array statico. `freeList` invece viene utilizato con lista della support struct libere.
Questo meccanismo viene completato tramite le funzioni `allocFromFreeList`, `allocSupportStruct` e `deallocSupportStruct`.

La modifica piu' interessante per questa parte e' l'allocazione degli spazi degli U-proc direttamente dalla RAM.
Dunque, invece di utilizzare i campi della support struct per assegnare i gestori delle eccezioni, vengono prelevati direttamente dalla RAM effettuando un calcolo con l'ASID del processo e la `PAGESIZE` (sia per il `generalExcept` che per il `page fault except`).  
Come da specifiche, gli spazi dedicati in RAM sono direttamente sotto a `RAMTOP`, evitando l'ultimo frame.

L'inzializzazione delle pagine avviene sempre con dirty bit on, quindi write-enabled.

### vmSupport.h
in `vmSupport.h` viene incluso `"../../phase2/headers/constants.h"`. Questo include viene fatto solo per utilizzare le costanti `STACK_PAGE_VPN` e `KUSEG_VPN` e per non ridefinirle un'altra volta.  
Tali constanti vengono utilizzate sia in fase 2 (`utlbRefillHandler`) che in fase 3, ma si riferiscono sempre alla gestione della memoria virtuale.

`SWAP_POOL_START_ADDR` e' una costante definita come da specifiche.

### vmSupport.c
Le funzioni `enterAtomicSession` e `exitAtomicSession` sono funzioni molto semplici che permettono reciprocamente di entrare in sessione atomica (disabilitando gli interrupt) ed uscire dalla sessione atomica (abilitando gli interrupt). Seppur molto semplici queste funzioni migliorano la leggibilita' del codice.

`releaseFrameOfProcess` e' una funzione che implementa l'ottimizzazione consigliata. Tale funzione viene utilizzata in `sysSupport.c` in `sys2Terminate` e `programTrapSupportHandler`. Prima di terminare un processo, tutti i frame occupati sono marcati come non occupati, eliminando cosi' scritture estranee e non autorizzate nel backing store.

`updateTLB` e' la prima ottimizzazione che prevede di migliorare la gestione del `TLB`, passando dall'utilizzo della macro `TLBCLR()` all'utilizzo combinato delle macro `TLBP()` e `TLBWI()` che permettono una gestione migliore e piu' accurata della TLB evitando di ripulire ad ogni comando la TLB rischiando di portare il sistema operativo in trashing.

`pageReplacementAlgorithm` e' stato significativamente migliorato. Senza ottimizzazione l'algoritmo non cerca un frame libero, ma ne estrae uno casuale usando l'algoritmo Round Robin. Con l'ottimizzazione prima si effettua un sanity check sul frame (evitando errori potenzialmente involuti), poi si ricerca un frame libero. Se non esistono frame liberi, allora si ritorna al vecchio algoritmo Round Robin evitando la sotituzione dei frame del processo corrente (evitando ancora una volta errori indesiderati).

`CAUSE_EXCODE` e' una macro definita sempre in `"../../phase2/headers/constants.h"` e serve per prelevare il codice di eccezione. Anche qui non viene ridefinita per questioni di pulizia del codice

### sysSupport.c
E' interessante notare che `sys2Terminate` e `programTrapSupportHandler` sono identiche. L'unica cosa che cambia e' l'utilizzo in parti diverse del programma. La decisione di non accorpare queste due funzioni sotto un'unica funzione deriva dal fatto che le specifiche sottolineano queste due funzioni come diverse semanticamente, seppur uguali nell'implementazione. Per chiarezza e coerenza rispetto alle specifiche si e' deciso di mantenere questa separazione.

Essenzialmente questo file implementa le syscall di fase 3 che vengono chiamate a livello utente. Queste syscall non sono altro che dei wrapper sulle syscall precedentemente definite in fase 2. Infine il `syscallExceptionHandler` e' l'analogo del `syscallHandler` definito in fase 2. 

`TERMREGADRR` e' una macro che calcola il register address dato il device number. Restituisce un termreg_t*

### shell.c
Questo e' il programma che permette di interagire all'utente con la shell.  
Ogni programma viene associato al suo asid tramite una mappa statica.
Viene letto il comando tramite la `SYSCALL` `READTERINAL`, se il nome del programma esiste, allora il programma viene eseguito tramite la syscall `EXECUTE`, altrimenti viene stampato a schermo un `command not found`.  
Con il comando `exit` la shell si chiude.

### calc.c
Questo e' un file tester che implementa una calcolatrice single digit che permette in input numeri negativi e positivi single digit e che permette le classiche operazioni, ovvero somma, sottrazione, prodotto e divisione.  
Vengono implementate due funzioni per la conversione da stringa ad intero e da intero a stringa.