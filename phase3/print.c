
//---------------- DEBUG: print di fase 2 ----------------
#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include "print.h"

typedef unsigned int devregtr;

#define TERM0ADDR 0x10000254
#define TERMSTATMASK 0xFF
#define MAXSEM   20
#define NOLEAVES 4 /* number of leaves of p8 process tree */

extern int processCount;

int sem_term_mut = 1,              /* for mutual exclusion on terminal */
    s[MAXSEM + 1],                 /* semaphore array */
    sem_testsem             = 0,   /* for a simple test */
    sem_startp2             = 0,   /* used to start p2 */
    sem_endp2               = 0,   /* used to signal p2's demise */
    sem_endp3               = 0,   /* used to signal p3's demise */
    sem_blkp4               = 1,   /* used to block second incaration of p4 */
    sem_synp4               = 0,   /* used to allow p4 incarnations to synhronize */
    sem_endp4               = 0,   /* to signal demise of p4 */
    sem_endp5               = 0,   /* to signal demise of p5 */
    sem_endp8               = 0,   /* to signal demise of p8 */
    sem_endcreate[NOLEAVES] = {0}, /* for a p8 leaf to signal its creation */
    sem_blkp8               = 0,   /* to block p8 */
    sem_blkp9               = 0;   /* to block p9 */

void print(char *msg) {

    char     *s       = msg;
    devregtr *base    = (devregtr *)(TERM0ADDR);
    devregtr *command = base + 3;
    devregtr  status;

    SYSCALL(PASSEREN, (int)&sem_term_mut, 0, 0); /* P(sem_term_mut) */
    while (*s != EOS) {
        devregtr value = PRINTCHR | (((devregtr)*s) << 8);
        status         = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & TERMSTATMASK) != RECVD) {
            PANIC();
        }
        s++;
    }
    SYSCALL(VERHOGEN, (int)&sem_term_mut, 0, 0); /* V(sem_term_mut) */
}
