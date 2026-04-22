### To compile the project
``` bash
 cmake -B build
 cmake --build build
```

**Important:** Controllare che TLB Floor Address sia settato ad un valore diverso da VM OFF;
nel config_machine.json e' settato a `0x80000000`

### Authors
- Grazia Lisi
- Alessia Mertolini
- Gabriele Preti
- Mirko Zironi

## MultiPandOS - Phase 1

### Work Managment
- Grazia Lisi: pcb.c / 2.1 + 2.3 *
- Alessia Mertolini: asl.c / 3.1 *
- Gabriele Preti: pcb.c / 2.2 *
- Mirko Zironi: asl.c / 3.1 *

(*) := from the given spec file

---

## MultiPandOS - Phase 2

### Work Managment
- initial.c / [section 2] -> *Alessia*
- scheduler.c [section 3] -> *Mirko*
- exceptions.c
    - TLB refil events [section 4] -> *Mirko*
    - exceptions handling [section 5] -> *Gabriele*
    - syscall [section 6] -> *Gabriele*, *Alessia*
    - pass up or die [section 8] -> *Mirko*
        - process termination [section 10]
    - accumulated CPU time [section 9] -> *Gabriele*
    - bios data page [section 11] -> *Grazia*
- interrupts.c [section 7] -> *Grazia*
- debug: *Grazia*, *Alessia*, *Gabriele*, *Mirko*

[section *] := from the given spec file

---

## MultiPandOS - Phase 2

### Work Managment
- ARCHITETTO DELLA MEMORIA VIRTUALE -> Gestione della virtual memory -> vmSupport.c -> *Gabriele*
    - gestione della swap pool [section 4.1]
    - gestione dei page fault (pager) [section 4.2]

- SPECIALISTA DELLA I/O E SWAP -> vmSupport.c -> *Grazia*
    - I/O sui dispositivi flash [section 5.1]
    - algoritmo di sostituzione delle pagine [section 5.4]
    - program trap handler [section 8]

- persona 3 -> *Alessia*
    - gestione del TLB refill [section 3]
    - gestione page table [section 2]
    - aggiornamenti atomici [section 5.2+5.3]

- GESTORE DI SYSCALL E ECCEZIONI -> sysSupport.c -> *Mirko*
    - exception handler [section 6]
    - eccezioni e syscall [section 7]

<!-- - COORDINATORE DI PROCESSI E USERLAND -> testers -> [section 9]
    - file di test
    - file di inizializzazione -->
