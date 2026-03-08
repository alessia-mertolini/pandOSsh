#include "../headers/const.h"
#include "../headers/listx.h"
#include "./headers/scheduler.h"

/*
    Differenza importante per il futuro: BIOS data page contiene la "fotografia"
    dello stato del processore prima dello scatto dell'eccezione
    Ecco perche' qui cerchiamo lo stato (user o kernel) nel BIOS datapage e non
    facciamo direttamente getSTATUS, che invece ci restituirebbe sempre kernel
    dato che quando gestiamo l'eccezione siamo in kernel mode
 */

void DoIO(void) {
    // verify kernel mode -> PRIVINSTR
    // program trap if in user mode -> GENERALEXCEPT

    // otteniamo lo stato del processo corrente
    state_t *saved_exception_state = (state_t *) GET_EXCEPTION_STATE_PTR(getPRID())

    //* controllo per nuclei non-esistenti da fare nella funzione padre
    if ((saved_exception_state->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_U) {
        // se e' in user mode -> program trap exception
        saved_exception_state->cause = PRIVINSTR;

        // TODO: call program trap exception handler

        return;
    }

    unsigned int *commandAddr = (unsigned int *) saved_exception_state->reg_a1;
    unsigned int commandValue = saved_exception_state->reg_a2;

    saved_exception_state->pc_epc += 4; // sommiamo una parola per evitare infinite loop
    
    // TODO: sto assumendo che esista currentProcess e che sia un puntatore alla struttura del pcb
    currentProcess->p_s = *saved_exception_state;

    // TODO: update the accumulated CPU time for the Current Process

    // scrivo il valore di a2 nell'indirizzo contenuto in a1
    *commandAddr = commandValue;

    scheduler(); // chiamo lo scheduler
}