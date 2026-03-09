#include "../headers/const.h"
#include "../headers/listx.h"
#include "./headers/scheduler.h"
#include "../phase1/headers/asl.h"

/*
    deviceSemaphores
    Indici      Dispositivo / Linea     Tipo
    0 - 7       Linea 3 (Disk)          Standard 
    8 - 15      Linea 4 (Flash)         Standard 
    16 - 23     Linea 5 (Network)       Standard 
    24 - 31     Linea 6 (Printer)       Standard 
    32 - 39     Linea 7 (Terminal)      Ricezione 
    40 - 47     Linea 7 (Terminal)      Trasmissione 
    48          Pseudo-clock            Clock 
*/

/*
    Differenza importante per il futuro: BIOS data page contiene la "fotografia"
    dello stato del processore prima dello scatto dell'eccezione
    Ecco perche' qui cerchiamo lo stato (user o kernel) nel BIOS datapage e non
    facciamo direttamente getSTATUS, che invece ci restituirebbe sempre kernel
    dato che quando gestiamo l'eccezione siamo in kernel mode
 */

//* variabili globali da definire in initial.c
extern pcb_t *currentProcess; // PC
extern int deviceSemaphores[SEMDEVLEN]; // 
extern int softBlockCount;
extern cpu_t start_tod;

void DoIO(void) {
    // otteniamo lo stato del processo corrente
    state_t *saved_exception_state = (state_t *) GET_EXCEPTION_STATE_PTR(getPRID());

    //* controllo per nuclei non-esistenti da fare nella funzione padre
    if ((saved_exception_state->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_U) {
        // se e' in user mode -> program trap exception
        saved_exception_state->cause = PRIVINSTR;

        // TODO: call program trap exception handler
        // program trap if in user mode -> GENERALEXCEPT

        return;
    }
    // altrimenti siamo in kernel mode e possiamo proseguire

    unsigned int *commandAddr = (unsigned int *) saved_exception_state->reg_a1;
    unsigned int commandValue = saved_exception_state->reg_a2;

    saved_exception_state->pc_epc += 4; // sommiamo una parola per evitare infinite loop
    currentProcess->p_s = *saved_exception_state;

    // Updating accumulated CPU time for the current process
    cpu_t now; STCK(now);
 *  currentProcess->p_time += (now - start_tod);

    // scrivo il valore di a2 nell'indirizzo contenuto in a1
    *commandAddr = commandValue;

    // Gestione semaforo
    int offset = (unsigned int)commandAddr - START_DEVREG;
    int DevNo = (offset % 0x80) / 0x10; // il modulo restituisce il device all'interno della stessa linea
    // int IntLineNo = ((offset - (DevNo * 0x10)) / 0x80) + 3; // formula inversa del del calcolo del devAddrBase
    int IntLineNo = (offset / 0x80) + 3; // (DevNo *0x10) sara' sempre minore di 0x80, risultando in 0
    int semIdx;

    if (IntLineNo < 7) { // standard device delle linee 3-4-5-6
        /* 
            all'interno del deviceSemaphores saranno nell'ordine delle linee di interrupt
            ogni linea continene 8 devices + sommiamo DevNo per il device corretto
        */
        semIdx = (IntLineNo - 3) * 8 + DevNo; 
    } else { // terminal device
        int internalOffset = offset % 0x10; // identifica il sub-device corrispondente

        if (internalOffset == 0x4) { // RECV_COMMAND
            // 32 perche' siamo nella linea 7 e quindi quanto meno siamo dopo 4(linee) * 8(devices)
            semIdx = 32 + DevNo;
        } else { // TRANSM_COMMAND
            semIdx = 40 + DevNo;
        }
    }

    // blocchiamo il processo nel semaforo corrispondente
    insertBlocked(&(deviceSemaphores[semIdx]), currentProcess);
    softBlockCount++;  
    currentProcess = NULL;

    scheduler(); // chiamo lo scheduler
}