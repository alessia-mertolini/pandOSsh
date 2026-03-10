#ifndef CONSTANTS_H
#define CONSTANTS_H

/* -----------------------------------------------------------------------
 * PHASE 2 constants
 * ----------------------------------------------------------------------- */

/* Device layout */
#define DEVPERINT    8   /* device per interrupt line (linee 3-7)          */
#define DEVINTNUM    5   /* numero di linee device (3..7)                   */

/* Indice semaforo Pseudo-clock nell'array deviceSemaphores[] */
// #define PSEUDOCLOCKIDX  (SEMDEVLEN - 1)   /* = 48                          */
#define PSEUDOCLOCK_SEM_IDX 48 // Indice del semaforo Pseudo-clock nell'array device Semaphores[] = SEMDELEVEN - 1

//Codici di eccezione rilevanti
// #define EXC_SYSCALL 8 //ECALL da M-mode
// #define EXC_BREAKPOINT 9 //EBREAK
#define EXC_SYSCALL_U 11 //ECALL da U-mode
#define EXC_TLB_MIN 24 //Prima eccezione TLB
#define EXC_TLB_MAX 28 //Ultima eccezione TLB

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

/* Controlla se l'eccezione è un interrupt (bit 31 del registro cause impostato a 1) */
#define CAUSE_IS_INT(cause)  ((cause) & KUSEG)

/* -----------------------------------------------------------------------
 * PHASE 2 macros
 * ----------------------------------------------------------------------- */

#define IS_KERNEL_MODE(status) (((status) & MSTATUS_MPP_MASK) == MSTATUS_MPP_M) //macro consigliata per la modalità del processore
/*
 * estrae il codice di eccezione dal registro cause. Si fa un And Bit a Bit tra il contenuto del registro cause e GETEXECCODE.
 * Lo shift è necessario per ottenenere un dato DA 0000 0000 0010 0000 A 0000 0000 0000 1000
*/
#define CAUSE_EXCCODE(cause) (((cause) & GETEXECCODE) >> CAUSESHIFT) // Estrae il codice di eccezione dal registro

#endif