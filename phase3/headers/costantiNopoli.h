#ifndef SYSSUPPORT_CONST_H_INCLUDED
#define SYSSUPPORT_CONST_H_INCLUDED

/*
 * sysSupport_const.h
 * Costanti aggiuntive per sysSupport.c (Phase 3 - Level 4).
 *
 * Include SOLO costanti non già presenti in pandos_const.h.
 * Per le costanti del prof fare riferimento a pandos_const.h:
 *   - TERMINATE 2, WRITETERMINAL 4          (SYSCALL positivi)
 *   - PGFAULTEXCEPT 0, GENERALEXCEPT 1      (indici Support Structure)
 *   - IL_TERMINAL 21                         (interrupt line terminale)
 *   - OKCHARTRANS 5, TRANSMITCHAR 2,
 *     RECEIVECHAR 2                          (comandi/stati terminale)
 *   - MAXSTRLENG 128                         (lunghezza max stringa)
 *   - KUSEG 0x80000000                       (base logical address space)
 *   - WORDLEN 4                              (dimensione word)
 *   - UPROCMAX 8                             (max U-proc concorrenti)
 *   - SYSEXCEPTION 8                         (excCode SYSCALL)
 *   - GETEXECCODE 0x0000007C                 (maschera Cause per excCode)
 *   - DOIO, VERHOGEN, PASSEREN,
 *     GETSUPPORTPTR, TERMPROCESS             (numeri SYSCALL Nucleus)
 */

/* -----------------------------------------------------------------------
 * SYSCALL Nucleus: TERMPROCESS è -2 in pandos_const.h ma non ha un alias
 * TERMINATEPROCESS che è il nome usato nelle spec di Phase 2.
 * Si definisce qui per chiarezza nel codice.
 * ----------------------------------------------------------------------- */
#define TERMINATEPROCESS    TERMPROCESS     /* NSYS2: alias leggibile       */

/* -----------------------------------------------------------------------
 * SYSCALL Support Level: EXECUTE (SYS6) non è in pandos_const.h
 * (TERMINATE=2, WRITETERMINAL=4 sono già presenti)
 * ----------------------------------------------------------------------- */
#define READTERMINAL    5   /* SYS5: legge stringa dal terminale            */
#define EXECUTE         6   /* SYS6: avvia un nuovo U-proc (solo shell)     */

/* -----------------------------------------------------------------------
 * Top dello stack U-proc: limite superiore del logical address space.
 * KUSEG_BASE = KUSEG (0x80000000) è già in pandos_const.h.
 * KUSEG_TOP non è definito: corrisponde alla fine dello stack page.
 * Stack SP iniziale = USERSTACKTOP = 0xC0000000 (già in pandos_const.h).
 * La stack page è [0xBFFFF000, 0xC0000000), quindi il top è 0xC0000000.
 * ----------------------------------------------------------------------- */
#define KUSEG_TOP   USERSTACKTOP    /* 0xC0000000: fine area kuseg          */

/* -----------------------------------------------------------------------
 * ASID della shell (convenzione del progetto, Sezione 9)
 * ----------------------------------------------------------------------- */
#define SHELL_ASID      1

/* -----------------------------------------------------------------------
 * Numero del terminale usato dalla shell (Sezione 9, 11)
 * ----------------------------------------------------------------------- */
#define SHELL_TERMINAL  0

/* -----------------------------------------------------------------------
 * Maschere per la status word restituita dai device terminale (Sez. 7.2/7.3)
 * Il byte basso (bit 0-7) contiene il codice di stato.
 * Il secondo byte (bit 8-15) contiene il carattere trasmesso/ricevuto.
 * ----------------------------------------------------------------------- */
#define TERM_STATUS_MASK    0x000000FF  /* estrae il codice di stato        */
#define TERM_CHAR_MASK      0x0000FF00  /* estrae il carattere              */
#define TERM_CHAR_SHIFT     8           /* shift per estrarre il carattere  */

/* -----------------------------------------------------------------------
 * Macro per calcolare l'indirizzo base del device register di un terminale.
 * Formula da Phase 2 Sezione 7.1:
 *   devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10)
 * START_DEVREG = 0x10000054 è già in pandos_const.h.
 * ----------------------------------------------------------------------- */
#define TERM_REG_ADDR(devNo) \
    ((termreg_t *)(START_DEVREG + ((IL_TERMINAL - 3) * 0x80) + ((devNo) * 0x10)))

#endif /* SYSSUPPORT_CONST_H_INCLUDED */