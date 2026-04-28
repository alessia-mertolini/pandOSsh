#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include "../../headers/const.h"

/* --- Costanti Phase 3 ---------------------------------------------------- */
#define READTERMINAL   5
#define EXECUTE        6
#define FLASH_STATUS_OK 1

/* Indirizzo di partenza della Swap Pool (dopo 32 frame di OS code)          */
#define FLASHPOOLSTART (RAMSTART + (OSFRAMES * PAGESIZE))
#define SWAP_POOL_ADDR FLASHPOOLSTART

/* Indici base dei semafori terminale nel Support Level                       */
#define SEMDEV_TERMTX_BASE 32
#define SEMDEV_TERMRX_BASE 40

/* --- Struttura entry della Swap Pool table -------------------------------- */
typedef struct {
    int        asid;  /* ASID occupante; -1 = frame libero                  */
    int        vpn;   /* VPN della pagina logica occupante                   */
    pteEntry_t *pte;  /* puntatore alla PTE nella Page Table del proprietario*/
} swapEntry_t;

/* --- Variabili globali esportate ----------------------------------------- */
extern int swapPoolSem;          /* mutex sulla Swap Pool table (init a 1)   */
extern int flashSem[DEVPERINT];  /* mutex per ogni flash device (init a 1)   */
extern int termReadSem;          /* mutex RX terminale (init a 1)            */
extern int termWriteSem;         /* mutex TX terminale (init a 1)            */

/* --- Funzioni esportate --------------------------------------------------- */

/* Inizializza la Swap Pool table (chiamata da initProc/test)                 */
void initSwapStructs(void);

/* TLB exception handler del Support Level (Pager) [Section 4.2]             */
void pager(void);

/* TLB-Refill event handler [Section 3] – rimane in exceptions.c             */
/* void uTLB_RefillHandler(void);  (dichiarata in exceptions.h)              */

#endif