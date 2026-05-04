#include "./headers/vmSupport.h"
#include <uriscv/liburiscv.h>

/*
  // ? chi assegna gli ASID values
*/
void initPageTable(support_t *s) {
  for (int i = 0; i < USERPGTBLSIZE; i++) {
    volatile unsigned int vpn;

    if (i == USERPGTBLSIZE -
                 1) { // per l'ultima pagina vi e' invece un registro preposto
      vpn = STACK_PAGE_VPN;
    } else { // per le prime 31 pagine e un semplice contatore da 1 a 31
      vpn = KUSEG_VPN + i;
    }

    // ogni entryHI e' composta dai primi 20 bit per il numero vpn, che quindi
    // shiftiamo a sx e dagli ultimi 3 per l'ASID, che quindi sommiamo per avere
    // in fondo a destra come indirizzo del registro puntato dalla pageTable
    s->sup_privatePgTbl[i].pte_entryHI =
        (vpn << VPNSHIFT) | (s->sup_asid << ASIDSHIFT);

    // attiviamo solo D e G
    s->sup_privatePgTbl[i].pte_entryLO = DIRTYON | GLOBALON;
  }
}

/*
  @param bitFlags: puo' essere VALIDON | DIRTYON | GLOBALON
  @param physicalAddr: indirizzo fisico del frame deve essere gia' nel formato
  corretto per pte_entryLO, cioe' PFN << PFNSHIFT
  @return: aggiorna la pagetable e il tlb con la pagina e i flag passati
*/
void updatePTE(support_t *s, int pageNo, unsigned int physicalAddr,
               int bitFlags) {
  // disabilitiamo gli interrupt
  unsigned int status = getSTATUS();
  setSTATUS(status & ~MSTATUS_MIE_MASK);

  // * 1 - prima aggiorniamo la pageTable
  pteEntry_t *pageTable =
      &(s->sup_privatePgTbl[pageNo]); // serve solo per riferirsi alla pageTable
                                      // in modo piu' compatto
  pageTable->pte_entryLO = physicalAddr | bitFlags;

  // * 2 - poi aggiorniamo il TLB
  // unsigned int oldEntryHi = getENTRYHI();
  // setENTRYHI(pageTable->pte_entryHI);

  // ricerca nel TLB
  // TLBP();
  // if(getINDEX() >= 0) {
  //   // Se c'è, aggiorna subito il registro fisico con il nuovo EntryLo (V, D,
  //   G e PFN) setENTRYLO(s->sup_privatePgTbl[pageNo].pte_entryLO); TLBWI();
  // }

  // ripristino
  // setENTRYHI(oldEntryHi);

  // TODO: sostituire la riga seguente con il frammento di codice precedente
  TLBCLR();

  setSTATUS(status);
}