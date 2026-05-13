#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../phase2/headers/constants.h"
// l'unica cosa che prendiamo da qua e' STACK_PAGE_VPN e KUSEG_VPN

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "./sysSupport.h"

#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>


//utilizzato nel pager
#define SWAP_POOL_START_ADDR (0x20000000 + (32 * PAGESIZE)) 

// --------- Funzioni esportate --------
void releaseFramesOfProcess(int asid);

//Funzione di inizializzazione delle strutture dati necessarie alla swap pool
//'swapPoolSemaphore' viene inizializzato come da specifiche pronto per accettare una 'P'
//inizializzazione dei campi della struct 'swap_t' utilizzata da 'swapPoolTable'
void initSwapStructures();

/**
 * Gestisce le eccezioni di tipo Page Fault (TLB-Invalid Load/Store),
 * si occupa di caricare in Swap Pool le pagine logiche mancanti dal
 * dispositivo flash del processo corrente, utilizzando
 * un algoritmo di rimpiazzamento RoundRobin
 */
void pager();

extern int swapPoolSemaphore;

#endif