XT_PRG_PREFIX = riscv64-unknown-elf-
CC = $(XT_PRG_PREFIX)gcc
LD = $(XT_PRG_PREFIX)ld

URISCV_DIR_PREFIX  = /usr/local
URISCV_DATA_DIR    = $(URISCV_DIR_PREFIX)/share/uriscv
URISCV_INCLUDE_DIR = /usr/include
URISCV_LIB_DIR     = $(URISCV_DIR_PREFIX)/lib/uriscv

CFLAGS_LANG = -ffreestanding -static -c -nostdlib
CFLAGS = $(CFLAGS_LANG) -I$(URISCV_INCLUDE_DIR) -Ih -Wall -O0 -march=rv32imafd -mabi=ilp32d

LDAOUTFLAGS = -m elf32lriscv -G 0 -nostdlib -T $(URISCV_DATA_DIR)/uriscvaout.ldscript

EF   = uriscv-elf2uriscv
UDEV = uriscv-mkdev

all: shell.uriscv fibEight.uriscv echo.uriscv fibEleven.uriscv uname.uriscv calc.uriscv date.uriscv sl.uriscv

crti.o: $(URISCV_DATA_DIR)/crti.S
	$(CC) $(CFLAGS) $< -o $@

liburiscv.o: $(URISCV_DATA_DIR)/liburiscv.S
	$(CC) $(CFLAGS) $< -o $@

print.o: print.c h/print.h
	$(CC) $(CFLAGS) print.c -o print.o

%.o: %.c h/print.h h/tconst.h
	$(CC) $(CFLAGS) $< -o $@

%.t: %.o print.o crti.o liburiscv.o
	$(LD) $(LDAOUTFLAGS) crti.o $< print.o liburiscv.o -o $@

%.t.aout.uriscv: %.t
	$(EF) -a $<

%.uriscv: %.t.aout.uriscv
	$(UDEV) -f $@ $<

clean:
	rm -f *.o *.t *.uriscv