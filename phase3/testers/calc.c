#include <uriscv/liburiscv.h>
#include "h/tconst.h"
#include "h/print.h"

// == PROGRAMMA TESTER CALC ==
// il programma è una calcolatrice single digit
// operazioni supportate:
// somma, sottrazione, moltiplicazione e divisione

// Funzione che permette di rimuove il newline finale letto da ReadTerminal
static void removeNewLine(char *buf) {
    while (*buf) {
        if (*buf == '\n' || *buf == '\r') {
            *buf = '\0';
            return;
        }
        buf++;
    }
}

// Funzione che permette la conversione da stringa a intero
static int stringToInt(char *str) {
    int partialNumber = 0;
    int sign = 1; //si suppone segno positivo
    int i = 0;
    if (str[0] == '-') { //gestione dei numeri negativi
        sign = -1;
        i++;
    }
    //finche' non si legge un l'EOL (EndOfLine) e finche' si tratta di numeri
    while(str[i] != '\0' && str[i] >= '0' && str[i] <= '9') {
        partialNumber = partialNumber * 10 + (str[i] - '0'); // str[i] - '0' permette di ottenere il numero
        i++;
    }
    return sign * partialNumber;
}

// Funzione che permette la conversione da intero a stringa
static void intToString(int num, char* str) {
    int i = 0;
    int isNegative = 0; //si suppone segno positivo
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (num < 0) {
        isNegative = 1;
        //si porta a positivo il numero e poi si aggiungerà il segno negativo
        num = -num;
    }
    //estrazione delle cifre al contrario
    while (num != 0) {
        int rem = num % 10; //modulo 10 per prendere l'ultima cifra
        str[i++] = rem + '0'; //vine aggiunto lo '0' per la conversione ASCII
        num = num / 10; //vine tolta dal numero l'ultima cifra (appena considerata)
    }
    if (isNegative) {
        str[i++] = '-';
    }
    str[i] = '\0';

    //ESEMPIO
    //in questo istante il numero '-42' è in str[] come '24-'
    int start = 0;
    int end = i - 1;
    while (start < end) { //il ciclo permette di scambiare le cifre
        char tmp = str[start];
        str[start] = str[end];
        str[end] = tmp;
        start++;
        end--;
    }
}
//ritorna 1 if a == b
static int str_cmp(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

void main() {
    int status;
    char firstInput[32]; //primo numero
    char secondInput[32]; //segno
    char thirdInput[32]; //secondo numero
    char output[32]; //risultato

    print(WRITETERMINAL, "Calcolatrice avviata \nInserisci un numero: ");
    status = SYSCALL(READTERMINAL, (int)&firstInput[0], 0, 0);
    firstInput[status] = EOS;
    removeNewLine(firstInput);
    //validazione primo numero
    if (firstInput[0] == '-') {
        //caso numero negativo: deve avere un secondo carattere tra '0' e '9' e terminare subito dopo
        if (firstInput[1] < '0' || firstInput[1] > '9' || firstInput[2] != '\0') {
            print(WRITETERMINAL, "Errore! Inserire un numero valido\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
    } else {
        //caso numero positivo: deve avere un primo carattere tra '0' e '9' e terminare subito dopo
        if (firstInput[0] < '0' || firstInput[0] > '9' || firstInput[1] != '\0') {
            print(WRITETERMINAL, "Errore! Inserire un numero valido\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
    }
    int firstNumber = stringToInt(firstInput);

    print(WRITETERMINAL, "Inserisci un'operazione: ");
    status = SYSCALL(READTERMINAL, (int)&secondInput[0], 0, 0);
    secondInput[status] = EOS;
    removeNewLine(secondInput);

    //controllo sull'operazione inserita da tastiera
    if (! ( str_cmp(secondInput, "+") || str_cmp(secondInput, "-") || str_cmp(secondInput, "*") || str_cmp(secondInput, "/") ) ) {
        print(WRITETERMINAL, "Operazione non consentita \n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    print(WRITETERMINAL, "Inserisci un numero: ");
    status = SYSCALL(READTERMINAL, (int)&thirdInput[0], 0, 0);
    thirdInput[status] = EOS;
    removeNewLine(thirdInput);
    //validazione primo numero
    if (thirdInput[0] == '-') {
        //caso numero negativo: deve avere un secondo carattere tra '0' e '9' e terminare subito dopo
        if (thirdInput[1] < '0' || thirdInput[1] > '9' || thirdInput[2] != '\0') {
            print(WRITETERMINAL, "Errore! Inserire un numero valido\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
    } else {
        //caso numero positivo: deve avere un primo carattere tra '0' e '9' e terminare subito dopo
        if (thirdInput[0] < '0' || thirdInput[0] > '9' || thirdInput[1] != '\0') {
            print(WRITETERMINAL, "Errore! Inserire un numero valido\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
    }
    int secondNumber = stringToInt(thirdInput);

    int result = 0;
    if (str_cmp(secondInput, "+")) {
        result = firstNumber + secondNumber;
    } else if (str_cmp(secondInput, "-")) {
        result = firstNumber - secondNumber;
    } else if (str_cmp(secondInput, "*")) {
        result = firstNumber * secondNumber;
    } else if (str_cmp(secondInput, "/")) {
        if (secondNumber == 0) {
            print(WRITETERMINAL, "Divisione per zero riscontrata \n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
        result = firstNumber / secondNumber;
    }

    print(WRITETERMINAL, "Risultato: ");
    //conversione e stampa del risultato
    intToString(result, output);
    print(WRITETERMINAL, &output[0]);
    print(WRITETERMINAL, "\n");
    SYSCALL(TERMINATE, 0, 0, 0);
}