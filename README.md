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

### Work Managment
- Grazia Lisi: pcb.c / 2.1 + 2.3 *
- Alessia Mertolini: asl.c / 3.1 *
- Gabriele Preti: pcb.c / 2.2 *
- Mirko Zironi: asl.c / 3.1 *

(*) := from the given spec file

---

#### Modifiche al CMakeLists.txt per funzionamento su Arch
- rimosso *unknown* da `set(XT_PRG_PREFIX riscv64-elf-)`
- aggiunto *local* in `set(URISCV_INC /usr/include)`
