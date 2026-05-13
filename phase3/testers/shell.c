 #include "h/tconst.h"
 #include "h/print.h"
 #include "../../headers/const.h"
 #include <uriscv/liburiscv.h>

#define MAX_PROGS 7

typedef struct {
    char *name;
    int   asid;
} prog_entry_t;

// Associamo ogni programma al suo ASID tramite una mappa statica
static prog_entry_t tests[MAX_PROGS] = {
    { "fibEight", 2 },
    { "echo", 3 },
    { "fibEleven", 4 },
    { "uname", 5 },
    { "date", 6 },
    { "sl", 7 },
    { "calc", 8 },
};

// Confronto tra stringhe
static int str_cmp(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return FALSE;
        a++; b++;
    }
    return (*a == EOS && *b == EOS);
}

// Rimuove il newline finale letto da ReadTerminal
static void rm_newline(char *buf) {
    while (*buf) {
        if (*buf == '\n' || *buf == '\r') {
            *buf = EOS;
            return;
        }
        buf++;
    }
}

int main() {
    char input_buf[MAXSTRLENG];
    int  nread, i, found;

    print(WRITETERMINAL, "[OK] PandOSsh shell ready. Type a program name or 'exit'.\n");


    // Tentativo di rallentare lo spam di chiamate ai programmi
    for(volatile int i = 0; i < 500; i++) {
        SYSCALL(EXECUTE, tests[0].asid, 0, 0);
        SYSCALL(EXECUTE, tests[2].asid, 0, 0);
        SYSCALL(EXECUTE, tests[3].asid, 0, 0);
        SYSCALL(EXECUTE, tests[4].asid, 0, 0);
        SYSCALL(EXECUTE, tests[5].asid, 0, 0);
    }
    print(WRITETERMINAL, "Stress test completato!\n");


    // while (1) {
    //     print(WRITETERMINAL, "> ");

    //     // SYS5: ReadTerminal - legge una riga da tastiera
    //     nread = SYSCALL(READTERMINAL, (int)&input_buf[0], 0, 0);

    //     if (nread < 0) {
    //         // Errore di lettura: termina
    //         print(WRITETERMINAL, "ERROR: ReadTerminal failed\n");
    //         continue;
    //     }

    //     // Controllo dei limiti della stringa
    //     if (nread < 128) {
    //         input_buf[nread] = EOS;
    //     } else if (nread >= 128) {
    //         input_buf[127] = EOS; 
    //     }

    //     rm_newline(input_buf);

    //     // se il comando e' "exit" termina la shell
    //     if (str_cmp(input_buf, "exit")) {
    //         print(WRITETERMINAL, "Shell exiting.\n");
    //         SYSCALL(TERMINATE, 0, 0, 0);
    //         /* non ritorna mai */
    //     }

    //     found = 0;

    //     // Ricerca nella mappa statica
    //     for (i = 0; i < MAX_PROGS; i++) {
    //         if (str_cmp(input_buf, tests[i].name)) {
    //             found = 1;

    //             // SYS6: Execute - crea il processo figlio e attende
    //             SYSCALL(EXECUTE, tests[i].asid, 0, 0);

    //             // Siamo tornati: il figlio è terminato
    //             print(WRITETERMINAL, "Program terminated.\n");
    //             break;
    //         }
    //     }

    //     if (!found) {
    //         print(WRITETERMINAL, "Command not found: ");
    //         print(WRITETERMINAL, input_buf);
    //         print(WRITETERMINAL, "\n");
    //     }
    // }
}