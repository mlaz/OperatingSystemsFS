CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO11" -I "../sofs11"
LFLAGS = -L "../../lib"

all:			showblock_sofs11

showblock_sofs11:	showblock_sofs11.o
			$(CC) $(LFLAGS) -o $@ $^ -lsofs11 -lrawIO11 -ldebugging
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f ../../run/showblock_sofs11
