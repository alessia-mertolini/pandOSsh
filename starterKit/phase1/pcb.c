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
    //il chiamante si assicura di aver tolto il PCB da tutte le altre strutture dati
}

pcb_t* allocPcb() {   //remove an elem from pcbFree list
    if (list_empty(&pcbFree_h)) {
        return NULL;
    }else{
        struct list_head *tmp;
        pcb_t *p;

        tmp = pcbFree_h.next;
        list_del(tmp);
        p = container_of(tmp, pcb_t, p_list);

        // azzeramento dati: initial values for all
        INIT_LIST_HEAD(&p->p_list);
        INIT_LIST_HEAD(&p->p_child);
        INIT_LIST_HEAD(&p->p_sib);

        p->p_parent = NULL;
        memset(&p->p_s, 0, sizeof(state_t));
        p->p_time = 0;
        p->p_semAdd = NULL;
        p->p_supportStruct = NULL;
        p->p_prio = 0;

        p->p_pid = next_pid++;

        return p;
    }
}// <--

void mkEmptyProcQ(struct list_head* head) {
}

int emptyProcQ(struct list_head* head) {
}

void insertProcQ(struct list_head* head, pcb_t* p) {
}

pcb_t* headProcQ(struct list_head* head) {
}

pcb_t* removeProcQ(struct list_head* head) {
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
}

// -->
int emptyChild(pcb_t* p) {
}

void insertChild(pcb_t* prnt, pcb_t* p) {
}

pcb_t* removeChild(pcb_t* p) {
}

pcb_t* outChild(pcb_t* p) {
}
// <--