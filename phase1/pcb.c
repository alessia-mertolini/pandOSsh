#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

// -->
void initPcbs() {   // initialize the pcbFree list to contain all elem of pcbFree_table
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i = 0; i < MAXPROC; i++) {
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h);
    }
}

void freePcb(pcb_t* p) {    //insert an elem to pcbFree list
    list_add_tail(&p->p_list, &pcbFree_h);
    // !! il chiamante si deve assicurare di aver tolto il PCB da tutte le altre strutture dati
}

pcb_t* allocPcb() {   //remove an elem from pcbFree list
    if (list_empty(&pcbFree_h)) {
        return NULL;
    }else{
        struct list_head *tmp;
        pcb_t *p;

        tmp = pcbFree_h.next;
        list_del(tmp);          // tolgo tmp dalla lista pcbFree_h
        p = container_of(tmp, pcb_t, p_list);

        // azzeramento dati: initial values for all
        INIT_LIST_HEAD(&p->p_list);
        INIT_LIST_HEAD(&p->p_child);
        INIT_LIST_HEAD(&p->p_sib);

        p->p_parent = NULL;
        
        // azzero state_t: --> /usr/include/uriscv/types.h
        
        p->p_s.entry_hi = 0;
        p->p_s.cause = 0;
        p->p_s.status = 0;
        p->p_s.pc_epc = 0;
        p->p_s.mie = 0;
        for (int i=0; i < STATE_GPR_LEN; i++){
            p->p_s.gpr[i] = 0;
        }

        p->p_time = 0;
        p->p_semAdd = NULL;
        p->p_supportStruct = NULL;
        p->p_prio = 0;

        p->p_pid = next_pid++;  // incremento pid per il prossimo PCB

        return p;
    }
}// <--

void mkEmptyProcQ(struct list_head* head) {
    INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head* head) {
    return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    if(list_empty(head)){ //se la lista è vuota -> inserisco direttamente in testa
        list_add(&p->p_list, head);
        return;
    }
    else{
        pcb_t* iter; //imposto iteratore per usare macro
        list_for_each_entry(iter, head, p_list){ //scorro la lista con la macro
            if(p->p_prio > iter->p_prio){ //punto di inserimento trovato
                list_add_tail(&p->p_list, &iter->p_list); //effettuo l'inserimento
                return;
            }
        }
        //se arrivo fino in fondo senza trovare nessuno slot di insermento -> inserisco in coda
        list_add_tail(&p->p_list, head);
        return;
    }
}

pcb_t* headProcQ(struct list_head* head) {
    if(list_empty(head)){
        return NULL;
    }
    else{ //ritorno il primo PCB puntato da head. Non rimuovo l'elemento
        return container_of(head->next, pcb_t, p_list); //utilizzo container_of per ottenere informazioni sul PCB
    }
}

pcb_t* removeProcQ(struct list_head* head) { //ottengo informazioni sul primo PCB e lo rimuovo dalla lista
    if (list_empty(head)){
        return NULL;
    }
    else{
        pcb_t *first_pcb = container_of(head->next, pcb_t, p_list); //salvo informazioni sul primo PCB
        list_del(&first_pcb->p_list); //rimuovo il PCB dalla lista
        return first_pcb;
    }
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) { //remove mirata di un PCB
    if (list_empty(head)){
        return NULL;
    }
    else{
        pcb_t *iter;
        pcb_t *extract = NULL;
        list_for_each_entry(iter, head, p_list){ //individuo elemento da rimuovere con iteratore
            if (iter->p_pid == p->p_pid){
                extract = iter; //salvo il puntatore all'elemento da rimuovere
            }
        }
        if (extract != NULL){ //faccio un controllo per valore di ritorno
            list_del(&extract->p_list); //rimovo l'elemento dalla lista
            return extract; //ritorno l'elemento rimosso
        }
        else {
            return NULL;
        }
    }
}

// -->

/*Return TRUE if the PCB pointed to by p has no children*/
int emptyChild(pcb_t* p) {
    return (list_empty(&p->p_child));
}

/*Make the PCB pointed to by p a child of the PCB pointed to by prnt.*/
void insertChild(pcb_t* prnt, pcb_t* p) {       // p.parent punto a un pcb_t, mentre p.sib e p.child somo una lista di pcb_t
    // considero già fatto INIT_LIST_HEAD su p->p_sib e p->p_child
    list_add_tail(&p->p_sib, &prnt->p_child);
    p->p_parent = prnt;
}

/*Make the first child of the PCB no longer a child of p*/
pcb_t* removeChild(pcb_t* p) {
    if (list_empty(&p->p_child)) {
        return NULL;
    } else {
        struct list_head *tmp;
        pcb_t *child;

        tmp = p->p_child.next;  // first child
        list_del(tmp);

        child = container_of(tmp, pcb_t, p_child);

        child->p_parent = NULL;     // isolo il child rimosso
        INIT_LIST_HEAD(&child->p_sib);

        return child;
    }
}

/*Make the PCB pointed to by p no longer the child of its parent*/
pcb_t* outChild(pcb_t* p) {
    if (p->p_parent == NULL) {
        return NULL;
    } else {
        list_del(&p->p_sib);    // rimuove p qualunque posizione abbia
        p->p_parent = NULL;
        INIT_LIST_HEAD(&p->p_sib);
        return p;
    }
}
// <--