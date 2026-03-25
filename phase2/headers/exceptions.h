#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED
/* =========================================================================
 * exceptions.h  –  Phase 2 Exception & SYSCALL handlers
 * Questo modulo gestisce:
 *   - uTLB_RefillHandler   : placeholder TLB-Refill (verrà sostituito in Fase 3)
 *   - exceptionHandler     : dispatcher principale di tutte le eccezioni
 *   - syscallHandler       : gestione delle NSYS1-NSYS10
 *   - programTrapHandler   : Pass Up or Die per Program Trap
 *   - tlbExceptionHandler  : Pass Up or Die per eccezioni TLB
 * ========================================================================= */

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "./scheduler.h"
#include "./constants.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "./interrupts.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

//Prototipi funzioni di supporto
//-------
/* passUpOrDie – gestione generica "Pass Up or Die"
 * Chiamata da programTrapHandler e tlbExceptionHandler
 * Se il processo corrente non ha una Support Structure, il processo viene terminato come tutti i suoi dipendenti
 * Altrimenti: copia lo stato dell'eccezione nella Support Structure e trasferisce il controllo al Support Level (Pass Up)*/
static void passUpOrDie(int exceptionType, state_t *savedState);

/* terminateProcessTree – termina un sottoalbero di processi
 *
 * Scorre tutti i figli di "root", li termina, poi termina "root" stesso. 
 * Aggiorna processCount e softBlockCount. Non chiama lo Scheduler in quanto compito del chiamante*/
static void terminateProcessTree(pcb_t *root);

/* updateCPUTime – aggiorna p_time del processo corrente
 * Calcola il tempo trascorso dall'ultima volta che start_tod è stato
 * aggiornato e lo somma a currentProcess->p_time
 * Aggiorna start_tod al valore TOD corrente*/
static void updateCPUTime(void);

/* getSemaphoreIndex – calcola l'indice nell'array deviceSemaphores
 * a partire dall'indirizzo del campo command del device register
 * Per i terminali distingue TX (TRANSM_COMMAND) da RX (RECV_COMMAND)
 * controllando l'offset all'interno del device register*/
static int getSemaphoreIndex(int *commandAddr);


/* Entry point per tutte le eccezioni (escluso TLB-Refill).
 * Viene registrato nel Pass Up Vector durante l'inizializzazione del Nucleus
 * Legge il registro cause dalla BIOS Data Page e smista al gestore corretto */
void exceptionHandler(void);

/* Gestore delle eccezioni SYSCALL (codici 8 e 11)
 * Chiamato da exceptionHandler
 * Verifica la modalità (kernel/user) e il numero della syscall (a0),
 * poi esegue il servizio richiesto o simula una Program Trap*/
void syscallHandler(void);

 /* Gestore delle Program Trap (codici eccezione 0-7, 9, 10, 12-23)
 * Chiamato da exceptionHandler
 * Esegue Pass Up or Die con indice GENERALEXCEPT*/
void programTrapHandler(void);

/* Gestore delle eccezioni TLB (codici 24-28)
 * Chiamato da exceptionHandler
 * Esegue Pass Up or Die con indice PGFAULTEXCEPT*/
void tlbExceptionHandler(void);

#endif /* EXCEPTIONS_H_INCLUDED */
