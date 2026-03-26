/* -------------------------------------------------------------------------
 * Costanti hardware del terminale 0
 * Stessa logica di p1test.c: accesso diretto ai registri del device.
 * ------------------------------------------------------------------------- */
#define TERM0ADDR       0x10000254
#define TERM_TRANSM_STATUS  0x8   /* offset TRANSM_STATUS nel device register */
#define TERM_TRANSM_CMD     0xC   /* offset TRANSM_COMMAND nel device register */
#define PRINTCHR_CMD    2
#define TRANSMITTED     5
#define STATUSMASK      0xFF
#include "../headers/const.h"

/* -------------------------------------------------------------------------
 * termprint_direct() - stampa SENZA usare SYSCALL
 *
 * Accede direttamente ai registri hardware del terminale 0.
 * Non dipende da interrupt handler, doIO, passeren/verhogen.
 * Se questa funzione non stampa nulla → problema in initial.c/scheduler.
 * Se stampa → il kernel parte, il problema è nelle SYSCALL o negli interrupt.
 * ------------------------------------------------------------------------- */
static void termprint_direct(char *str) {
    volatile unsigned int *status  = (unsigned int *)(TERM0ADDR + TERM_TRANSM_STATUS);
    volatile unsigned int *command = (unsigned int *)(TERM0ADDR + TERM_TRANSM_CMD);
    unsigned int stat;

    /* Disabilita interrupt globali durante la stampa diretta HW.
     * Senza questo, il terminale genera un interrupt dopo ogni carattere
     * e il kernel lo intercetta rovinando lo stato. */
    unsigned int oldStatus = getSTATUS();
    setSTATUS(oldStatus & ~MSTATUS_MIE_MASK);

    stat = *status & STATUSMASK;
    if (stat != READY && stat != TRANSMITTED) {
        setSTATUS(oldStatus);
        return;
    }

    while (*str != '\0') {
        *command = (((unsigned int)*str) << 8) | PRINTCHR_CMD;

        stat = *status & STATUSMASK;
        while (stat == BUSY)
            stat = *status & STATUSMASK;

        if (stat != TRANSMITTED) {
            setSTATUS(oldStatus);
            while(1);
        }

        *command = ACK;
        str++;
    }

    /* Ripristina gli interrupt */
    setSTATUS(oldStatus);
}
