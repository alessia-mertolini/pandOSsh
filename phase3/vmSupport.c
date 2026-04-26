#include "./headers/vmSupport.h"
#include <uriscv/liburiscv.h>

//TODO controllare se 'swapPoolTable' e 'swapPoolSemaphore' devono essere static o no
//TODO capire se sono estetiche le variabili qui
//definizione della swapPoolTable utilizzando le strutture definite in types.h
//impostiamo la swapPoolTable come da specifiche
static swap_t swapPoolTable[POOLSIZE];
//definizione del semaforo
static int swapPoolSemaphore;

void initSwapStructures() {
    //semaforo pronto per accettare una P
    swapPoolSemaphore = 1;
    for (int i = 0; i < POOLSIZE; i++) {
        swapPoolTable[i].sw_asid = -1;
        swapPoolTable[i].sw_pageNo = -1;
        swapPoolTable[i].sw_pte = NULL;
    }
}

