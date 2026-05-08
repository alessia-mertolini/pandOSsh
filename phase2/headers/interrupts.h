#ifndef INTERRUPTS_H_INCLUDED
#define INTERRUPTS_H_INCLUDED

/*
 * interrupts.h
 *
 * Header del modulo di gestione delle interruzioni hardware per PandOSsh Phase 2.
 * Gestisce: PLT (Processor Local Timer), Interval Timer (pseudo-clock),
 * e tutti i device periferici (disk, flash, ethernet, printer, terminal).
 *
 * Include path (da usare nel .h): ../../headers/types.h
 */

#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "./scheduler.h"
#include "../../phase1/headers/asl.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

/* ===========================================================================
 * SCHEMA DEI SEMAFORI DEI DEVICE (array semDevices[SEMDEVLEN], SEMDEVLEN=49)
 * ===========================================================================
 *
 * L'array semDevices[] è definito in initial.c  <-- DA IMPLEMENTARE ESTERNAMENTE
 * Lo schema scelto è il seguente, coerente con l'ordine delle interrupt line:
 *
 *   Indici  0 -  7  → Disk devices       (interrupt line 3, device 0-7)
 *   Indici  8 - 15  → Flash devices      (interrupt line 4, device 0-7)
 *   Indici 16 - 23  → Ethernet devices   (interrupt line 5, device 0-7)
 *   Indici 24 - 31  → Printer devices    (interrupt line 6, device 0-7)
 *   Indici 32 - 39  → Terminal TX        (interrupt line 7, device 0-7, trasmissione)
 *   Indici 40 - 47  → Terminal RX        (interrupt line 7, device 0-7, ricezione)
 *   Indice  48      → Pseudo-clock       (interval timer, ogni 100ms)
 *
 * Formula generica per device non-terminale:
 *   semIndex = (intLineNo - 3) * DEVPERINT + devNo
 *
 * Per terminali TX:  semIndex = SEMDEV_TERMTX_BASE + devNo
 * Per terminali RX:  semIndex = SEMDEV_TERMRX_BASE + devNo
 * ===========================================================================
 */

/* Indirizzo base dell'area dei Device Registers (spec sezione 12) */
#define DEV_REG_BASE     0x10000054U

/* Dimensione in byte di tutti gli 8 device su una interrupt line */
#define DEV_LINE_SIZE    (DEVPERINT * DEV_REG_SIZE)  /* 8 * 16 = 0x80 */

/*
 * Indirizzo base dell'area Interrupting Devices Bit Map (spec Table 2):
 *   Word 0 @ 0x10000040 → interrupt line 3 (disk)
 *   Word 1 @ 0x10000044 → interrupt line 4 (flash)
 *   Word 2 @ 0x10000048 → interrupt line 5 (ethernet)
 *   Word 3 @ 0x1000004C → interrupt line 6 (printer)
 *   Word 4 @ 0x10000050 → interrupt line 7 (terminal)
 */
#define INT_BITMAP_BASE  0x10000040U

/*
 * Legge la parola della bitmap per la interrupt line intLineNo (3-7).
 * La word 0 corrisponde alla line 3, quindi si sottrae 3 all'indice.
 */
#define INT_BITMAP_WORD(intLineNo) \
    (*((volatile unsigned int *)(INT_BITMAP_BASE + ((intLineNo - 3) * WORDLEN))))

/*
 * Comando ACK da scrivere nel campo command del device register
 * per acknowledgare un interrupt pendente (spec sezione 7.1, step 3).
 * NOTA: se ACK non è già definito in uriscv/const.h usare 1.
 */
#define ACK 1

/* ===========================================================================
 * DICHIARAZIONE DELLA FUNZIONE PUBBLICA
 * ===========================================================================
 *
 * interruptHandler() è l'entry point di questo modulo.
 * Viene chiamata da exceptionHandler() in exceptions.c quando
 * CAUSE_IS_INT risulta vero sul cause register dell'eccezione corrente.
 *
 * La funzione:
 *  1. Determina la sorgente dell'interrupt con priorità massima
 *  2. La gestisce (PLT / Interval Timer / device periferico)
 *  3. Ritorna il controllo al Current Process oppure chiama lo scheduler
 */
void interruptHandler();

#endif /* INTERRUPTS_H_INCLUDED */