### MultiPandOS - Phase 1

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

### Work Managment phase1
- Grazia Lisi: pcb.c / 2.1 + 2.3 *
- Alessia Mertolini: asl.c / 3.1 *
- Gabriele Preti: pcb.c / 2.2 *
- Mirko Zironi: asl.c / 3.1 *

(*) := from the given spec file

### Work Managment phase2
- initial [section 2]
- scheduler [section 3] *Grazia*
    - accumulated CPU time [section 9]
- exceptions
    - TLB refil events [section 4] (page faults) **(codice pagina 4 dato)**
    - exceptions handling [section 5]
    - syscall [section 6] *Gabriele*
    - pass up or die [section 8] **(codice pagina 13 dato)**
        - process termination [section 10]
    - bios data page [section 11]
- interrupts [section 7] *Mirko*
- device registers [section 12] *Alessia* + alcune syscall

(*) := from the given spec file

---

#### Modifiche al CMakeLists.txt per funzionamento su Arch
- rimosso *unknown* da `set(XT_PRG_PREFIX riscv64-elf-)`
- aggiunto *local* in `set(URISCV_INC /usr/include)`

#### Compilazione per Arch speciale
`gcc -I/usr/local/include main.c -o main`
