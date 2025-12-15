/*
    L'ASL non è altro che una lista collegata di descrittori di semafori (semd_t), 
    dove ogni descrittore rappresenta un semaforo attivo. Un semaforo è "attivo" solo se 
    c'è almeno un PCB bloccato nella sua coda. Se la coda si svuota, il descrittore deve essere 
    rimosso dalla ASL e restituito alla lista dei liberi (semdFree).
*/

#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

/*
    Questa funzione serve a inizializzare le strutture dati globali necessarie per la
    gestione dei semafori prima che il sistema operativo inizi a lavorare.
    Creiamo la lista di semafori liberi riempiendola con gli elementi dell'array semdTable
*/
void initASL() {
    // Inizializzazione delle teste (semdFree_h e semd_h)
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);

    // Popolamento
    for (int i = 0; i < MAXPROC; i++) 
        list_add(&semd_table[i].s_link, &semdFree_h);
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
            list_add_tail(&p->p_list,&sem->s_procq);
            return 0;
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
        return 1;
    }
}

/*
    Questa funzione sblocca il primo processo (quello con priorità più alta) che è in attesa sul 
    semaforo indicato dal parametro. Quindi trovo il semaforo, rimuovo il primo PCB dalla coda
    e lo restituisco; se la coda è vuota disattivo il semaforo.
*/
pcb_t* removeBlocked(int* semAdd) {
    // Ricerca del semaforo (scorro la lista)
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        semd_t* item = container_of(iter, semd_t, s_link);
        if (item->s_key == semAdd) {
            // Sblocca il primo processo bloccato
            pcb_t* blocked = removeProcQ(&item->s_procq);

            // Se la coda si svuota, pulisci il semaforo
            if (list_empty(&item->s_procq)) {
                list_del(&item->s_link);
                list_add(&item->s_link, &semdFree_h);
            }
            return blocked; // Ritorna il processo bloccato
        }
    }
    return NULL; // Se maforo non trovato
}


/* 
    Questa funzione elimina un determinato processo p dalla coda del semaforo su cui è bloccato, 
    qualunque sia la sua posizione all'interno della coda.
*/
pcb_t* outBlocked(pcb_t* p) {
    // Identifico il semaforo
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        semd_t* item = container_of(iter, semd_t, s_link); 
        if (item->s_key == p->p_semAdd) {
            pcb_t* el_to_remove = outProcQ(&item->s_procq, p); // tento di rimuovere il processo specifico p
            
            // --- AGGIUNTA DI SICUREZZA ---
            // Se p non era nella coda, outProcQ ritorna NULL.
            // Interrompiamo subito per evitare di toccare lo stato del semaforo erroneamente.
            if (el_to_remove == NULL)
                return NULL;
            
            // Verifica se si svuota la coda ed in caso disattiva il semaforo
            if (list_empty(&item->s_procq)) {
                list_del(&item->s_link); // Elimina il collegamento
                list_add(&item->s_link, &semdFree_h); // Metti il semaforo fra quelli liberi
            }
            return el_to_remove; // Ritorno l'elemento che ho eliminato dalla coda
        }
    }
    return NULL; // Semaforo non trovato
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