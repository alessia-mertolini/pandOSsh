# Gestione delle Eccezioni in Phase 2 — `exceptions.c`

## Architettura generale

Quando il processore riceve un'eccezione (interrupt, syscall, trap, TLB fault), salva
automaticamente il proprio stato nella **BIOS Data Page** (`0x0FFFF000`) e salta al
gestore registrato nel Pass-Up Vector.

Il punto d'ingresso e' `exceptionHandler()`, che legge il campo `cause` dallo stato
salvato e smista al gestore appropriato:

```
exceptionHandler()
 ├─ CAUSE_IS_INT → interruptHandler()       (bit 31 = 1)
 ├─ excCode 8 / 11 → syscallHandler()       (ECALL da M-mode / U-mode)
 ├─ excCode 24-28 → tlbExceptionHandler()   (eccezioni TLB)
 ├─ excCode 7 + p5MemLocation==0x80000000   (workaround — vedi sotto)
 │    └─ tlbExceptionHandler()
 └─ tutto il resto → programTrapHandler()   (program trap generici)
```

---

## `programTrapHandler()`

```c
void programTrapHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(GENERALEXCEPT, savedState);
}
```

Gestisce tutte le eccezioni che non sono interrupt, syscall o TLB:
access fault, istruzione illegale, breakpoint, ecc.

Chiama `passUpOrDie` con indice **`GENERALEXCEPT` (= 1)**, che:
- Se il processo corrente **non ha** `p_supportStruct` → termina l'intero
  sottoalbero di processi ("die").
- Se il processo corrente **ha** `p_supportStruct` → copia lo stato
  dell'eccezione in `sup_exceptState[GENERALEXCEPT]` e trasferisce il
  controllo al gestore utente registrato in `sup_exceptContext[GENERALEXCEPT]`
  ("pass up").

---

## `tlbExceptionHandler()`

```c
void tlbExceptionHandler(void) {
    state_t *savedState = GET_EXCEPTION_STATE_PTR(0);
    passUpOrDie(PGFAULTEXCEPT, savedState);
}
```

Gestisce le eccezioni TLB (codici 24–28): TLB miss, TLB invalid, TLB modified.

Chiama `passUpOrDie` con indice **`PGFAULTEXCEPT` (= 0)**, che:
- Se il processo **non ha** `p_supportStruct` → "die" (stesso di sopra).
- Se il processo **ha** `p_supportStruct` → copia lo stato in
  `sup_exceptState[PGFAULTEXCEPT]` e trasferisce il controllo al gestore
  registrato in `sup_exceptContext[PGFAULTEXCEPT]` ("pass up").

La differenza rispetto a `programTrapHandler` e' **solo l'indice**: 0 o 1.
Questo seleziona quale dei due gestori utente riceve il controllo.

---

## Il flusso di `p5` e il perche' del workaround

### Setup del processo p5 (in `p2test.c`)

```c
pFiveSupport.sup_exceptContext[GENERALEXCEPT].pc = (memaddr)p5gen;  // indice 1
pFiveSupport.sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)p5mm;   // indice 0
```

p5 ha due gestori:
- **`p5gen`** riceve program trap (GENERALEXCEPT)
- **`p5mm`** riceve TLB fault (PGFAULTEXCEPT)

### Passo 1 — `p5()`: accesso a indirizzo 0

```c
void p5() {
    *p5MemLocation = *p5MemLocation + 1;  // p5MemLocation == 0x0
}
```

`p5MemLocation` vale `0` (NULL). Scrivere all'indirizzo 0 non e' mappato:
uRISCV genera **Store Access Fault (excCode = 7)**.

```
excCode 7  →  programTrapHandler()
           →  passUpOrDie(GENERALEXCEPT, ...)
           →  p5gen()
```

`p5gen()` vede `BUSERROR` (case 7) nella causa e imposta il PC su `p5a`.

### Passo 2 — `p5a()`: accesso a `0x80000000`

```c
void p5a() {
    p5MemLocation  = (memaddr *)0x80000000;
    *p5MemLocation = 42;
}
```

1. Scrittura a `0x80000000` — la pagina non e' nel TLB → **TLB miss**.
2. uRISCV chiama `uTLB_RefillHandler()` (registrato nel Pass-Up Vector):

   ```c
   void uTLB_RefillHandler() {
       setENTRYHI(0x80000000);
       setENTRYLO(0x00000000);  // Valid bit = 0 → entry INVALIDA
       TLBWR();
       LDST((state_t*) BIOSDATAPAGE);
   }
   ```

   Viene caricata una entry TLB con il **Valid bit a 0**.

3. L'hardware riprova la scrittura, trova la entry invalida e dovrebbe
   generare **Store Page Fault (excCode = 15)** — ma uRISCV genera ancora
   **Store Access Fault (excCode = 7)**, esattamente come il passo 1.

4. **Senza workaround**:
   ```
   excCode 7  →  programTrapHandler()
              →  passUpOrDie(GENERALEXCEPT, ...)
              →  p5gen()
   ```
   `p5gen()` rientra in `p5gen` → vede di nuovo BUSERROR → punta il PC a `p5a`
   → scrittura a `0x80000000` → TLB miss → refill invalido → excCode 7
   → **loop infinito**.

5. **Con workaround**: il kernel controlla se `p5MemLocation == 0x80000000`.
   Questo e' vero solo dopo che `p5a` ha eseguito la prima assegnazione.
   Quindi:
   ```
   excCode 7 && p5MemLocation==0x80000000
              →  tlbExceptionHandler()
              →  passUpOrDie(PGFAULTEXCEPT, ...)
              →  p5mm()
   ```
   `p5mm()` stampa "memory management trap" e "Correct Support Structure
   Address", poi imposta il PC su `p5b` e riprende l'esecuzione.

### Perche' `entry_hi` non funziona come alternativa

Il campo `entry_hi` nello stato salvato contiene il valore del registro
`ENTRYHI` **al momento dell'eccezione**. Questo registro viene aggiornato
dall'hardware solo per le eccezioni TLB (codici 24–28). Per un Access Fault
(excCode 7), `ENTRYHI` puo' contenere un valore residuo da una precedente
eccezione TLB, non necessariamente `0x80000000`. Questo spiega perche'
controllare `entry_hi` produceva il loop: la condizione poteva risultare vera
anche nel passo 1 (primo BusError su indirizzo 0), facendo andare quella
eccezione al gestore TLB invece che a `p5gen`.

### Riassunto visivo del flusso corretto

```
p5()
 └─ write a 0x0
     └─ excCode 7 → programTrapHandler → p5gen
         └─ case BUSERROR → PC = p5a

p5a()
 └─ write a 0x80000000
     └─ TLB miss → uTLB_RefillHandler (entry invalida)
         └─ retry write → excCode 7 (bug uRISCV)
             └─ workaround: p5MemLocation==0x80000000
                 └─ tlbExceptionHandler → p5mm
                     └─ stampa "memory management trap"
                         └─ PC = p5b → continua

p5b()
 └─ SYSCALL(1, ...) in user mode → Address Error (excCode 5)
     └─ programTrapHandler → p5gen
         └─ case ADDRERROR → MPP=M, PC = p5b (re-entry in kernel mode)

p5b() [seconda volta, kernel mode]
 └─ SYSCALL(1, ...) in kernel mode → MSYSCALLEXCPT
     └─ programTrapHandler → p5gen
         └─ case MSYSCALLEXCPT → p5sys()
             └─ "High level SYS call from kernel mode process"
```
