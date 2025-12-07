#include "./headers/asl.h"
#include "./headers/pcb.h"

/**
    * @param semdFree_h head of semdFree list that contains all the unused semaphore descriptores
    * @param semd_h is the Active Semaphore List (ASL)
    * semd = semaphore descriptor
*/
static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL() {
}

/**
 * @param semAdd semaphore address
 * @param p pointer to the pcb to be inserted
 * @return true if a new semaphore descriptor needs to be allocated and semdFree list is empty, false otherwise
 */
int insertBlocked(int* semAdd, pcb_t* p) {
    // list_add_tail from listx.h
    struct list_head* iter = NULL;
    list_for_each(iter,&semd_h) { // semaphore active
        semd_t *sem = container_of(iter,semd_t,s_link);

        if(sem->s_key == semAdd) { // ho trovato il semaforo semAdd -> SEMAFORO ATTIVO
            p->p_semAdd = semAdd;
            list_add_tail(p,&sem->s_procq);
            return 0; // ? immagino che "tutti gli altri casi" intenda anche questo
        }
    }

    // non ho trovato semAdd -> SEMAFORO INATTIVO
    if(!list_empty(&semdFree_h)) {
        struct list_head *next_sem = semdFree_h.next;
        list_del(semdFree_h.next); // tolgo un semaforo dalla lista dei semafori liberi
        
        semd_t *new_sem = container_of(next_sem,semd_t,s_link); // ottengo il puntatore

        // inizializzazione
        new_sem->s_key = semAdd;
        mkEmptyProcQ(&new_sem->s_procq);

        list_add_tail(&new_sem->s_link,&semd_h); // inserimento del descrittore fra i semafori attivi (asl)

        // blocco del pcb sul nuovo semaforo
        p->p_semAdd = semAdd;
        list_add_tail(&p->p_list,&new_sem->s_procq); // aggiungo il processo p alla coda di questo semaforo
        
        return 0;
    }
    else {
        // ? devo fare qualcosa? non ci sono semd liberi
        return 1;
    }
}

pcb_t* removeBlocked(int* semAdd) {
}

pcb_t* outBlocked(pcb_t* p) {
}

/**
 * @param semAdd semaphore address 
 * @return puntatore al pcb con chiave semAdd o NULL se non e' presente
*/
pcb_t* headBlocked(int* semAdd) {
    struct list_head* iter = NULL;
    list_for_each(iter,&semd_h) {
        semd_t *sem = container_of(iter,semd_t,s_link);
        if(sem->s_key == semAdd) { 
            if(!list_empty(&sem->s_procq))
                return container_of(sem->s_procq.next,pcb_t,p_list); // ritorna il puntatore al primo pcb_t
            else 
                return NULL; // se la coda dei processi e' vuota
        }
    }
    return NULL; // se non trova il semaforo ritorna NULL
}