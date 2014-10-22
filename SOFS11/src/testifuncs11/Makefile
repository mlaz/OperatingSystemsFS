CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO11" -I "../sofs11"
LFLAGS = -L "../../lib"

all:			testifuncs11

testifuncs11:		testifuncs11.o
			$(CC) $(LFLAGS) -o $@ $^ -lsofs11 -lrawIO11 -ldebugging
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f ../../run/testifuncs11
