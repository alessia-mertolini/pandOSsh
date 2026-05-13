#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED
/* ------------------------------------------------------
 * exceptions.h  –  Phase 2 Exception & SYSCALL handlers
 * Questo modulo gestisce
 *   - exceptionHandler: funzione dispatcher che permette 
 *                       la gestione delle eccezioni riscontrate
 *   - syscallHandler: funzione dispatcher per le SYSCALL (NSYS1-NSYS10)
 *   - programTrapHandler: gestore dei trap per i `GENERALEXCEPT`
 *   - tlbExceptionHandler: gestore dei trap per `PGFAULTEXCEPT`
 * ----------------------------------------------------------- */

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "./scheduler.h"
#include "./constants.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "./interrupts.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/const.h>

//Prototipi funzioni di supporto

/* Entry point per la gestione delle eccezioni.
 * Viene registrato nel Pass Up Vector durante l'inizializzazione del Nucleus
 * Legge il registro 'caue' dalla BIOS Data Page e smista al gestore corretto */
void exceptionHandler(void);

/* Gestore delle eccezioni SYSCALL - codici 8 e 11
 * Chiamato da exceptionHandler
 * Verifica la modalità (kernel/user) e il numero della syscall (reg. a0)
 * Prima di eseguire il servizio richiesto aumenta la il PC.
 * Esegue il servizio richiesto o simula una Program Trap*/
void syscallHandler(void);

 /* Gestore delle Program Trap (codici eccezione 0-7, 9, 10, 12-23)
 * Chiamato da exceptionHandler. Chiama il passUpOrDie
 * Esegue Pass Up or Die con indice GENERALEXCEPT */
void programTrapHandler(void);

/* Gestore delle eccezioni TLB (codici 24-28)
 * Chiamato exceptionHandler
 * Esegue Pass Up or Die con indice PGFAULTEXCEPT*/
void tlbExceptionHandler(void);

/* Gestore dei page faults
 * Trova la page table entry per la support struct nel BIOSDATAPAGE
 * Chiamato dal Pass Up or Die con indice PGFAULTEXCEPT
 */
void uTLB_RefillHandler(void);

#endif /* EXCEPTIONS_H_INCLUDED */
