CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO11" -I "../sofs11"
LFLAGS = -L "../../lib"

OBJS = fsck11_sb.o fsck11_inode.o fsck11_datacluster.o fsck11_dir.o

all:			fsck_sofs11

fsck_sofs11	:	fsck_sofs11.o $(OBJS) fsck11_stack.o
			$(CC) $(LFLAGS) -o $@ $^ -lsofs11 -lrawIO11 -ldebugging
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f ../../run/fsck_sofs11
