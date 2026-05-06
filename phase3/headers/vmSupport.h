#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../phase2/headers/constants.h"
// l'unica cosa che prendiamo da qua e' STACK_PAGE_VPN e KUSEG_VPN

#include "./sysSupport.h"

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../phase2/headers/constants.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

//utilizzato nel 'pager'
#define SWAP_POOL_START_ADDR (0x20000000 + (32 * PAGESIZE))

//Array dei semafori dei device - 8 flash device
extern int flashSem[DEVPERINT];

//TODO guardare se alcune di queste funzioni devono andare "static"

void releaseFramesOfProcess(int asid);

void initPageTable(support_t *s);

/*
  @param s: puntatore alla support_t del processo
  @param pageNo: il Virtual Page Number
  @param physicalAddr: l'indirizzo fisico calcolato per il frame
  @param bitFlags: i flag da accendere (es. VALIDON | DIRTYON)
*/
void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr, int bitFlags);

//Restituisce l'indirizzo fisico di un frame della Swap Pool
memaddr frameAddressFunction(int frameIdx);

//Funzione di inizializzazione delle strutture dati necessarie alla swap pool
//'swapPoolSemaphore' viene inizializzato come da specifiche pronto per accettare una 'P'
//inizializzazione dei campi della struct 'swap_t' utilizzata da 'swapPoolTable'
void initSwapStructures();

//semplice algoritmo Round Robin per la sostituzione delle pagine
void pageReplacementAlgorithm(int *i);

//funzione che permette di entrare in sessione atomica
void enterAtomicSession();

//funzione che permette di uscire dalla sessione atomica precedentemente iniziata
void exitAtomicSession();

//Scrive il contenuto di un frame fisico (presente nella Swap Pool) nel blocco
//corrispondente del dispositivo Flash associato a un determinato processo
/*
 * @param asid: asid del processo, usato per calcolare quale flash device usare
 * @param vpn: virtual page number
 * @param frameNumber: l'indice del frame nella Swap Pool da cui leggere i dati
 *
 * @return: restituisce 0 in caso di successo, 1 altrimenti
 */
int writeToFlash(int asid, unsigned int vpn, int frameNumber);

//Legge un blocco dal dispositivo Flash associato a un determinato processo
//e lo copia in un frame fisico (nella Swap Pool)
/*
 * @param asid: asid del processo, usato per calcolare quale flash device usare
 * @param vpn: virtual page number
 * @param frameNumber: l'indice del frame nella Swap Pool in cui scrivere i dati
 *
 * @return: restituisce 0 in caso di successo, 1 altrimenti
 */
int readToFlash(int asid, unsigned int vpn, int frameNumber);

int readOrWriteToFlash(int asid, int vpn, int frameNumber, int command);

/**
 * Gestisce le eccezioni di tipo Page Fault (TLB-Invalid Load/Store),
 * si occupa di caricare in Swap Pool le pagine logiche mancanti dal
 * dispositivo flash del processo corrente, utilizzando
 * un algoritmo di rimpiazzamento RR
 */
void pager();

void releaseFramesOfProcess(int asid);

#endif