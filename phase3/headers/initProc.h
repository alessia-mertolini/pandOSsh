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

support_t* launchUProc(int asid); /* crea e lancia un U-proc tramite NSYS1*/
void deallocSupportStruct(support_t *sup);

#endif