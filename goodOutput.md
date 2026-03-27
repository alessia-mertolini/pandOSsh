# Output di Successo Atteso per p2test.c

Di seguito è riportata la sequenza di stampe a terminale che indica un'esecuzione corretta della Fase 2, basata sul file di test fornito.

### Fase Iniziale (Boot e Processi 2/3)
L'esecuzione inizia con i test di base sui semafori e sul tempo di CPU.
* `p1 v(sem_testsem)`
* `p2 was started`
* `p2 starts`
* `p2 v's successfully`
* `p2 is OK`
* `p3 is started`
* `p3 - CLOCKWAIT OK`
* `p3 - CPU time correctly maintained`

### Fase Centrale (Trap, Eccezioni e TLB)
Qui vengono lanciati svariati processi in parallelo. L'ordine esatto può variare in base allo scheduling.
* `hp_p2 starts`
* `hp_p2 ends`
* `hp_p1 starts`
* `first incarnation of p4 starts`
* `second incarnation of p4 starts`
* `p4 is OK`
* `p5 starts`
* `Bus Error (as expected): Access non-existent memory`
* `memory management trap`
* `Correct Support Structure Address`
* `Address Error (as expected): non-kuseg access w/KU=1`
* `High level SYS call from kernel mode process`
* `p6 starts` *(Nota: Muore silenziosamente subito dopo perché non ha il passup vector inizializzato)*
* `p7 starts` *(Nota: Muore silenziosamente subito dopo per un program trap)*
* `p9 starts`
* `p10 starts`
* `hp_p1 ends`
* `p1 knows p5 ended`

### Fase Finale (Albero dei Processi p8)
Il processo principale verificherà per **4 volte consecutive** la corretta terminazione di un intero albero di processi. Pertanto, **il blocco seguente verrà stampato 4 volte**:
* `p8root starts`
* `child1 starts`
* `child2 starts`
* `leaf process (1) starts`
* `leaf process (2) starts`
* `leaf process (3) starts`
* `leaf process (4) starts`

### Conclusione
Se il kernel sopravvive a tutte le uccisioni di processi, page fault e system call errate, stamperà questo messaggio trionfale prima di terminare sé stesso.
* `p1 finishes OK -- TTFN`


TERM (0) `hp_p1 starts`
[!] TERM (0) `hp_p2 starts`
[!] TERM (0) `first incarnation of p4 starts`
[!] TERM (0) `p5 starts`
[!] TERM (0) `p6 starts`
[!] TERM (0) `p7 starts`
[!] TERM (0) `p9 starts`
[!] TERM (0) `hp_p1 ends`
[!] TERM (0) `hp_p2 ends`
[!] TERM (0) `second incarnation of p4 starts`
[!] TERM (0) `Bus Error (as expected): Access non-existent memory`
[!] TERM (0) `p10 starts`
[!] TERM (0) `p4 is OK`
[!] TERM (0) `Address Error (as expected): non-kuseg access w/KU=1`
[!] TERM (0) `High level SYS call from kernel mode process`
[!] TERM (0) `p1 knows p5 ended`