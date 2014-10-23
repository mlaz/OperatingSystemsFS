CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO11" -I "../sofs11"
LFLAGS = -L "../../lib"

all:			mkfs_sofs11

mkfs_sofs11	:	mkfs_sofs11.o
			$(CC) $(LFLAGS) -o $@ $^ -lsofs11 -lrawIO11 -ldebugging
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f ../../run/mkfs_sofs11
