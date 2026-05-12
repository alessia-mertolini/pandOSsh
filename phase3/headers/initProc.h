#ifndef INITPROC_H
#define INITPROC_H

#include "../../headers/const.h"
#include "../../headers/types.h"

extern int  masterSemaphore;   /* P'ato da InstantiatorProcess, V'ato dalla shell alla fine */
extern int  shellSemaphore;    /* P'ato dalla shell in SYS6, V'ato dal figlio in SYS2*/
extern int swapPoolSemaphore;

extern int writeTerminalSemaphore;
extern int readTerminalSemaphore;
extern int flashSem[DEVPERINT];

/*
  crea e lancia un U-proc tramite NSYS1
  azzera o assegna il valore corretto ad ogni campo del U-proc, abilitando gli interrupt e ponendosi in user mode  
*/ 
support_t* launchUProc(int asid); 

// Aggiunge una support structure alla free list
void deallocSupportStruct(support_t *sup);

#endif