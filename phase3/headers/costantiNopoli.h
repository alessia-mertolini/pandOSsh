#ifndef COSTANTINOPOLI_H
#define COSTANTINOPOLI_H

#define SHELL_ASID 1
#define SHELL_TERMINAL 0
/* -----------------------------------------------------------------------
 * Maschere per la status word restituita dai device terminale (Sez. 7.2/7.3)
 * Il byte basso (bit 0-7) contiene il codice di stato.
 * Il secondo byte (bit 8-15) contiene il carattere trasmesso/ricevuto.
 * ----------------------------------------------------------------------- */
#define TERM_STATUS_MASK    0x000000FF  /* estrae il codice di stato*/
#define TERM_CHAR_MASK      0x0000FF00  /* estrae il carattere */
#define TERM_CHAR_SHIFT     8           /* shift per estrarre il carattere  */




#endif