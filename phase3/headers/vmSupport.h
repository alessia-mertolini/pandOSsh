#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../phase2/headers/constants.h"
// l'unica cosa che prendiamo da qua e' STACK_PAGE_VPN e KUSEG_VPN

#include "../../headers/types.h"
#include "../../headers/const.h"

#include <uriscv/liburiscv.h>

//utilizzato nel 'pager'
#define SWAP_POOL_START_ADDR (0x20000000 + (32 * PAGE_SIZE))

void initPageTable(support_t *s);

void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int flags);

//Funzione di inizializzazione delle strutture dati necessarie alla swap pool
//'swapPoolSemaphore' viene inizializzato come da specifiche pronto per accettare una 'P'
//inizializzazione dei campi della struct 'swap_t' utilizzata da 'swapPoolTable'
void initSwapStructures();

#endif