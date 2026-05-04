// File coordinatore
// Risiede la funzione TEST che funge da instantiator Process
// E' compito di questa funzione:
// inizializzare: SWAP POOL TABLE, SEMAPHORE POOL TABLE -> lo fa chimando initSwapStructs
// oltre a fare anche altre cose

#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/liburiscv.h>

#include "./headers/initProc.h"   /* semafori e support struct esportati */
#include "./headers/vmSupport.h"  /* initSwapStructs()                   */
#include "./headers/sysSupport.h" /* indirizzo handler generali          */

void test() {
    initSwapStructures();
}