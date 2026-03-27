/* File: $Id: p2test.c,v 1.1 1998/01/20 09:28:08 morsiani Exp morsiani $ */

/*********************************P2TEST.C*******************************
 *
 *	Test program for the PandosPlus Kernel: phase 2.
 *      v.0.1: March 20, 2022
 *
 *	Produces progress messages on Terminal0.
 *
 *	This is pretty convoluted code, so good luck!
 *
 *		Aborts as soon as an error is detected.
 *
 *      Modified by Michael Goldweber on May 15, 2004
 *		Modified by Michael Goldweber on June 19, 2020
 */

#include "../headers/const.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>

typedef unsigned int devregtr;

/* hardware constants */
#define PRINTCHR 2
#define RECVD 5

#define CLOCKINTERVAL 100000UL /* interval to V clock semaphore */

#define BUSERROR 7
#define ADDRERROR 5
#define USYSCALLEXCPT 8
#define MSYSCALLEXCPT 11
#define SELF NULL

#define QPAGE 1024

#define MINLOOPTIME 30000
#define LOOPNUM 10000

#define CLOCKLOOP 10
#define MINCLOCKLOOP 3000
#define TERMSTATMASK 0xFF

#define BADADDR 0xFFFFFFFF
#define TERM0ADDR 0x10000254

#define MSTATUS_MIE_MASK 0x8
#define MIE_MTIE_MASK 0x40
#define MIP_MTIP_MASK 0x40
#define MIE_ALL 0xFFFFFFFF

#define MSTATUS_MPIE_BIT 7
#define MSTATUS_MIE_BIT 3
#define MSTATUS_MPRV_BIT 17
#define MSTATUS_MPP_BIT 11
#define MSTATUS_MPP_M 0x1800
#define MSTATUS_MPP_U 0x0000
#define MSTATUS_MPP_MASK 0x1800

/* just to be clear */
#define NOLEAVES 4 /* number of leaves of p8 process tree */

#define START 0

/* just to be clear */
#define MAXSEM 20

int sem_term_mut = 1,              /* for mutual exclusion on terminal */
    s[MAXSEM + 1],                 /* semaphore array */
    sem_testsem = 0,               /* for a simple test */
    sem_startp2 = 0,               /* used to start p2 */
    sem_endp2 = 0,                 /* used to signal p2's demise */
    sem_endp3 = 0,                 /* used to signal p3's demise */
    sem_blkp4 = 1,                 /* used to block second incaration of p4 */
    sem_synp4 = 0,                 /* used to allow p4 incarnations to synhronize */
    sem_endp4 = 0,                 /* to signal demise of p4 */
    sem_endp5 = 0,                 /* to signal demise of p5 */
    sem_endp8 = 0,                 /* to signal demise of p8 */
    sem_endcreate[NOLEAVES] = {0}, /* for a p8 leaf to signal its creation */
    sem_blkp8 = 0,                 /* to block p8 */
    sem_blkp9 = 0;                 /* to block p9 */

state_t p2state, p3state, p4state, p5state, p6state, p7state, p8rootstate, child1state, child2state, gchild1state,
    gchild2state, gchild3state, gchild4state, p9state, p10state, hp_p1state, hp_p2state;

int p2pid, p3pid, p4pid, p8pid, p9pid;

/* support structure for p5 */
support_t pFiveSupport;

int p1p2synch = 0; /* to check on p1/p2 synchronization */

int p8inc;     /* p8's incarnation number */
int p4inc = 1; /* p4 incarnation number */

unsigned int p5Stack; /* so we can allocate new stack for 2nd p5 */

int creation = 0;           /* return code for SYSCALL invocation */
memaddr *p5MemLocation = 0; /* To cause a p5 trap */

void p2(), p3(), p4(), p5(), p5a(), p5b(), p6(), p7(), p7a(), p5prog(), p5mm();
void p5sys(), p8root(), child1(), child2(), p8leaf1(), p8leaf2(), p8leaf3(), p8leaf4(), p9(), p10(), hp_p1(), hp_p2();

extern void p5gen();
extern void p5mm();

/* a procedure to print on terminal 0 */
void print(char *msg)
{

    char *s = msg;
    devregtr *base = (devregtr *)(TERM0ADDR);
    devregtr *command = base + 3;
    devregtr status;

    SYSCALL(PASSEREN, (int)&sem_term_mut, 0, 0); /* P(sem_term_mut) */
    while (*s != EOS)
    {
        devregtr value = PRINTCHR | (((devregtr)*s) << 8);
        status = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & TERMSTATMASK) != RECVD)
        {
            PANIC();
        }
        s++;
    }
    SYSCALL(VERHOGEN, (int)&sem_term_mut, 0, 0); /* V(sem_term_mut) */
}

/* TLB-Refill Handler */
/* One can place debug calls here, but not calls to print */
void uTLB_RefillHandler()
{
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)BIOSDATAPAGE);
}

/*********************************************************************/
/*                                                                   */
/*                 p1 -- the root process                            */
/*                                                                   */
void test()
{
    char *msg = "???";

    // Indirizzo base del Terminale 0
    volatile unsigned int *term0_base = (unsigned int *)TERM0ADDR;

    // Il TRANSM_COMMAND si trova all'offset 0xC (che equivale a +3 word)
    volatile unsigned int *commandReg = term0_base + 3;

    while (*msg != '\0')
    {
        // Formatta il comando: il carattere va shiftato di 8 bit,
        // unito in OR con il comando di stampa (PRINTCHR = 2)
        unsigned int cmdValue = (((unsigned int)*msg) << 8) | PRINTCHR;

        // Esegue la DoIO: passa l'indirizzo del registro e il comando formattato
        unsigned int status = SYSCALL(DOIO, (int)commandReg, (int)cmdValue, 0);

        // Maschera lo stato e verifica che sia TRANSMITTED (5)
        if ((status & TERMSTATMASK) != 5)
        {
            PANIC();
        }
        msg++;
    }

    // Terminiamo il processo di test
    SYSCALL(TERMPROCESS, 0, 0, 0);
}