/*
 * shell.c
 * Shell interattiva per PandOSsh Phase 3
 *
 * La shell:
 *   - Legge un comando da terminale (SYS5 / READTERMINAL)
 *   - Cerca il programma nella mappa statica nome -> ASID
 *   - Esegue il programma (SYS6 / EXECUTE) e aspetta la terminazione
 *   - Termina con "exit" (SYS2 / TERMINATE)
 */

#include <uriscv/liburiscv.h>
#include "h/tconst.h"
#include "h/print.h"
#include <uriscv/const.h>

/* --------------------------------------------------------------------------
 * Mappa statica: nome programma -> ASID
 *
 * Ogni U-proc corrisponde a un flash device precaricato con il suo
 * load image. L'ASID identifica sia il processo che il device.
 * ASID 1..8 sono validi; 0 è riservato al kernel.
 * -------------------------------------------------------------------------- */
#define MAX_PROGS 7

typedef struct {
    char *name;
    int   asid;
} prog_entry_t;

static prog_entry_t tests[MAX_PROGS] = {
    { "fibEight", 2 },
    { "echo", 3 },
    { "fibEleven", 4 },
    { "uname", 5 },
    { "date", 6 },
    { "sl", 7 },
    { "calc", 8 },
};

/* --------------------------------------------------------------------------
 * Utility: confronto stringhe (no libc in kernel/support level)
 * -------------------------------------------------------------------------- */
static int str_cmp(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return FALSE;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

/* Rimuove il newline finale letto da ReadTerminal */
static void rm_newline(char *buf) {
    while (*buf) {
        if (*buf == '\n' || *buf == '\r') {
            *buf = '\0';
            return;
        }
        buf++;
    }
}

int main() {
    char input_buf[128];
    int  nread, i, found;

    print(WRITETERMINAL, "[OK] PandOSsh shell ready. Type a program name or 'exit'.\n");

    while (1) {
        /* Prompt */
        print(WRITETERMINAL, "> ");

        /* SYS5: ReadTerminal - legge una riga da tastiera */
        nread = SYSCALL(READTERMINAL, (int)input_buf, 0, 0);

        if (nread < 0) {
            /* Errore di lettura: termina */
            print(WRITETERMINAL, "ERROR: ReadTerminal failed\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }

        /* Rimuove il '\n' finale inserito dal terminale */
        rm_newline(input_buf);

        /* Comando "exit": termina la shell */
        if (str_cmp(input_buf, "exit")) {
            print(WRITETERMINAL, "Shell exiting.\n");
            SYSCALL(TERMINATE, 0, 0, 0);
            /* non ritorna mai */
        }

        /* Ricerca nella mappa statica */
        found = 0;

        // Tentativo di rallentare lo spam di chiamate ai programmi
        for(int i = 0; i < 1000; i++) {
            SYSCALL(EXECUTE, tests[0].asid, 0, 0);
            for (int i = 0; i < 1000; i++) {;}
            SYSCALL(EXECUTE, tests[2].asid, 0, 0);
            for (int i = 0; i < 1000; i++) {;}
            SYSCALL(EXECUTE, tests[3].asid, 0, 0);
            for (int i = 0; i < 1000; i++) {;}
            SYSCALL(EXECUTE, tests[4].asid, 0, 0);
            for (int i = 0; i < 1000; i++) {;}
            SYSCALL(EXECUTE, tests[5].asid, 0, 0);
            for (int i = 0; i < 1000; i++) {;}
        }
        print(WRITETERMINAL, "Stress test completato!\n");

        // for (i = 0; i < MAX_PROGS; i++) {
        //     if (str_cmp(input_buf, tests[i].name)) {
        //         found = 1;


        //         /* SYS6: Execute - crea il processo figlio e attende */
        //         SYSCALL(EXECUTE, tests[i].asid, 0, 0);

        //         /* Siamo tornati: il figlio è terminato */
        //         print(WRITETERMINAL, "Program terminated.\n");
        //         break;
        //     }
        // }

        // if (!found) {
        //     print(WRITETERMINAL, "Command not found: ");
        //     print(WRITETERMINAL, input_buf);
        //     print(WRITETERMINAL, "\n");
        // }
    }
}