CC = gcc
CFLAGS = -Wall -I "../debugging"

all:			librawIO11

librawIO11:		sofs_rawdisk.o
			ar -r librawIO11.a $^ sofs_buffercacheinternals.o sofs_buffercache.o
			cp librawIO11.a ../../lib
			rm -f $^ librawIO11.a

clean:
			rm -f ../../lib/librawIO11.a
