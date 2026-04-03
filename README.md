### To compile the project
``` bash
 cmake -B build
 cmake --build build
```

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
- scheduler.c [section 3] *Mirko*
- exceptions.c
    - TLB refil events [section 4] -> *Mirko*
    - exceptions handling [section 5] -> *Grazia*
    - syscall [section 6] -> *Gabriele*, *Alessia*
    - pass up or die [section 8] -> *Mirko*
        - process termination [section 10]
    - accumulated CPU time [section 9] -> *Gabriele*
    - bios data page [section 11] -> *Grazia*
- interrupts.c [section 7] *Grazia*
- debug: *Grazia*, *Alessia*, *Gabriele*, *Mirko*

[section *] := from the given spec file
