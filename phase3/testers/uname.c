#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

/* ASCII Art centrata per terminale a 80 colonne */
#define LOGO1 "                 ____                 _  ___  ____      _     "
#define LOGO2 "                |  _ \\ __ _ _ __   __| |/ _ \\/ ___| ___| |__  "
#define LOGO3 "                | |_) / _` | '_ \\ / _` | | | \\___ \\/ __| '_ \\ "
#define LOGO4 "                |  __/ (_| | | | | (_| | |_| |___) \\__ \\ | | |"
#define LOGO5 "                |_|   \\__,_|_| |_|\\__,_|\\___/|____/|___/_| |_|"

#define NUM1  "                               44   44     22222  "
#define NUM2  "                               44   44    22   22 "
#define NUM3  "                               4444444       22   "
#define NUM4  "                                    44     22     "
#define NUM5  "                                    44    2222222 "

void main() {
	print(WRITETERMINAL, "\n");
	print(WRITETERMINAL, LOGO1"\n");
	print(WRITETERMINAL, LOGO2"\n");
	print(WRITETERMINAL, LOGO3"\n");
	print(WRITETERMINAL, LOGO4"\n");
	print(WRITETERMINAL, LOGO5"\n");
	print(WRITETERMINAL, "\n");
	print(WRITETERMINAL, NUM1"\n");
	print(WRITETERMINAL, NUM2"\n");
	print(WRITETERMINAL, NUM3"\n");
	print(WRITETERMINAL, NUM4"\n");
	print(WRITETERMINAL, NUM5"\n");
	print(WRITETERMINAL, "\n");
	SYSCALL(TERMINATE, 0, 0, 0);
}