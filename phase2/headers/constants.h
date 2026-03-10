#ifndef CONSTANTS_H
#define CONSTANTS_H
/* -----------------------------------------------------------------------
 * PHASE 2 constants
 * ----------------------------------------------------------------------- */

/* Device layout */
#define DEVPERINT    8   /* device per interrupt line (linee 3-7)          */
#define DEVINTNUM    5   /* numero di linee device (3..7)                   */

/* Indice semaforo Pseudo-clock nell'array deviceSemaphores[] */
#define PSEUDOCLOCKIDX  (SEMDEVLEN - 1)   /* = 48                          */

/* Calcola l'indirizzo base del device register dato interrupt line e device.
 * Formula dalla spec sezione 7.1:
 *   devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10)  */
#define DEV_REG_ADDR(line, dev) \
    (START_DEVREG + ((line) - 3) * 0x80 + (dev) * 0x10)

/* Indirizzo base Interrupting Devices Bit Map (linea = 3..7):
 *   word index = line - 3, indirizzo = 0x10000040 + (line-3)*4             */
#define DEVBITMAP_ADDR(line) \
    ((unsigned int *)(0x10000040 + ((line) - 3) * 0x04))

/* Acknowledge command code per device */
#define ACK  1

/* Controlla se l'eccezione è un interrupt (bit 31 del registro cause) */
#define CAUSE_IS_INT(cause)  ((cause) & 0x80000000)

#endif