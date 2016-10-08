#
# CZ8RL1 comtroller / emulator makefile
# for DJGPP
CC = gcc
LD = gcc
# LIBS   = allegro

x8rl1: x8rl1.o cz8rl1.o x1tape.o osdepend.o realtime.o
	$(LD) $^ $(LIBS) -lz -lallegro -finput-charset=CP932 -o $@

%.o: %.c
	$(CC) -c $< -o $@
