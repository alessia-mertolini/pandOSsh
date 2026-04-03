#ifndef SCHEDULER_H
#define SCHEDULER_H

/* -----------------------------------------------------------------------
* READY QUEUE VUOTA:
*   processCount == 0                -> HALT  (tutto terminato, ok)
*   processCount > 0, softBlock > 0  -> WAIT  (attesa interrupt)
*   processCount > 0, softBlock == 0 -> PANIC (deadlock)
*
* Questo header viene incluso in:
*   - scheduler.c  (implementazione)
*   - initial.c    (chiama scheduler() alla fine del main)
*   - exceptions.c (chiama scheduler() dopo syscall bloccanti)
*   - interrupts.c (chiama scheduler() dopo PLT interrupt)
* ----------------------------------------------------------------------- */

#include "../../headers/const.h"
#include "./constants.h"
#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include <uriscv/liburiscv.h>

/* -----------------------------------------------------------------------
 * Variabili globali del kernel
 * Definite in initial.c, usate da tutti i moduli.
 * ----------------------------------------------------------------------- */

/* Numero di processi avviati e non ancora terminati */
extern int processCount;

/* Numero di processi bloccati per I/O o Pseudo-clock */
extern int softBlockCount;

/* Coda dei processi pronti, ordinata per priorità */
extern struct list_head readyQueue;

/* Puntatore al PCB del processo attualmente in esecuzione (NULL = nessuno) */
extern pcb_t *currentProcess;

/* Array di semafori per i device (indicizzato, vedere SEMDEVLEN in const.h)
 * Ultimo elemento (indice PSEUDOCLOCK_SEM_IDX) = semaforo Pseudo-clock */
extern int deviceSemaphores[SEMDEVLEN];

/* -----------------------------------------------------------------------
 * Timestamp TOD all'inizio del quanto del processo corrente.
 * Settato dallo scheduler ad ogni dispatch.
 * Letto da exceptions.c e interrupts.c per aggiornare p_time.
 * ----------------------------------------------------------------------- */
extern cpu_t start_tod;

/* -----------------------------------------------------------------------
 * Prototipo
 * ----------------------------------------------------------------------- */

/* Dispatcher principale. Non ritorna mai. */
void scheduler(void);

#endif /* SCHEDULER_H */