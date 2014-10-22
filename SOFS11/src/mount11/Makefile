CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -I "../debugging" -I "../rawIO11" -I "../sofs11" -I "../syscalls11"
LFLAGS = -L "../../lib" -L/lib

all:			mount_sofs11_bin

mount_sofs11_bin:	mount_sofs11_bin.o
			$(CC) $(LFLAGS) -o $@ $^ -lsyscalls11 -lsofs11 -lrawIO11 -ldebugging -pthread -lfuse -lrt -ldl
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f ../../run/mount_sofs11_bin
