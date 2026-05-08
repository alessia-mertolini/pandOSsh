#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

#define PANDOSSH1 "PPPP    AA   N   N DDDD   OOO   SSSS  SSSS H   H "
#define PANDOSSH2 "P   P  A  A  NN  N D   D O   O S     S     H   H "
#define PANDOSSH3 "PPPP   AAAA  N N N D   D O   O  SSS   SSS  HHHHH "
#define PANDOSSH4 "P      A  A  N  NN D   D O   O     S     S H   H "
#define PANDOSSH5 "P      A  A  N   N DDDD   OOO  SSSS  SSSS  H   H "

void main() {
    for (int i = 0; i < 10; i++) {
        print(WRITETERMINAL, PANDOSSH1"\n");
        print(WRITETERMINAL, PANDOSSH2"\n");
        print(WRITETERMINAL, PANDOSSH3"\n");
        print(WRITETERMINAL, PANDOSSH4"\n");
        print(WRITETERMINAL, PANDOSSH5"\n");
    }

    SYSCALL(TERMINATE, 0, 0, 0);
}