#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <uriscv/const.h>

/* -----------------------------------------------------------------------
 * PHASE 2 constants
 * ----------------------------------------------------------------------- */

/* Device layout */
#define DEVPERINT    8   /* device per interrupt line (linee 3-7)          */
#define DEVINTNUM    5   /* numero di linee device (3..7)                   */

/* Indice semaforo Pseudo-clock nell'array deviceSemaphores[] */
#define PSEUDOCLOCK_SEM_IDX 48 // Indice del semaforo Pseudo-clock nell'array device Semaphores[] = SEMDELEVEN - 1

/* Codici di eccezione rilevanti */
#define EXC_SYSCALL_U 11 //ECALL da U-mode
#define EXC_TLB_MIN 24 //Prima eccezione TLB
#define EXC_TLB_MAX 28 //Ultima eccezione TLB

/* Indici Semafori -
* Offset nell'array semDevices per ciascuna classe di device */
#define SEMDEV_DISK_BASE     0   /* disk:    line 3, indici 0-7  */
#define SEMDEV_FLASH_BASE    8   /* flash:   line 4, indici 8-15 */
#define SEMDEV_ETH_BASE      16  /* ethernet:line 5, indici 16-23*/
#define SEMDEV_PRINT_BASE    24  /* printer: line 6, indici 24-31*/
#define SEMDEV_TERMTX_BASE   32  /* term TX: line 7, indici 32-39*/
#define SEMDEV_TERMRX_BASE   40  /* term RX: line 7, indici 40-47*/

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
#define CAUSE_IS_INT(cause)  ((cause) & 0x80000000)

/* -----------------------------------------------------------------------
 * PHASE 2 macros
 * ----------------------------------------------------------------------- */

#define IS_KERNEL_MODE(status) (((status) & MSTATUS_MPP_MASK) == MSTATUS_MPP_M) //macro consigliata per la modalità del processore

/* Maschera per isolare il codice di eccezione in RISC-V (spegne il bit 31) */
#define CAUSE_EXCCODE_MASK 0x7FFFFFFF

/* Estrae il codice di eccezione dal registro cause in ambiente RISC-V */
#define CAUSE_EXCCODE(cause) ((cause) & CAUSE_EXCCODE_MASK)

#endif