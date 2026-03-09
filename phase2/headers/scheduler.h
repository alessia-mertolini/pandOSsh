#ifndef SCHEDULER_H
#define SCHEDULER_H

/* -----------------------------------------------------------------------
 * scheduler.h - PandOSsh Phase 2 - Scheduler
 *
 * Include questo header in:
 *   - scheduler.c  (implementazione)
 *   - initial.c    (chiama scheduler() alla fine del main)
 *   - exceptions.c (chiama scheduler() dopo syscall bloccanti)
 *   - interrupts.c (chiama scheduler() dopo PLT interrupt)
 * ----------------------------------------------------------------------- */

#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../phase1/headers/pcb.h"

/* -----------------------------------------------------------------------
 * Variabili globali del Nucleus
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
 * Ultimo elemento (indice PSEUDOCLOCKIDX) = semaforo Pseudo-clock */
extern int deviceSemaphores[SEMDEVLEN];

/* -----------------------------------------------------------------------
 * Timestamp TOD all'inizio del quanto del processo corrente.
 * Settato dallo scheduler ad ogni dispatch.
 * Letto da exceptions.c e interrupts.c per aggiornare p_time:
 *
 *   cpu_t now; STCK(now);
 *   currentProcess->p_time += (now - start_tod);
 * ----------------------------------------------------------------------- */
extern cpu_t start_tod;

/* -----------------------------------------------------------------------
 * Prototipi
 * ----------------------------------------------------------------------- */

/* Dispatcher principale. Non ritorna mai. */
void scheduler(void);

#endif /* SCHEDULER_H */