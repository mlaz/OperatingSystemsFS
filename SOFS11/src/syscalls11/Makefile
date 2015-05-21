CC = gcc
CFLAGS = -Wall -g -D_FILE_OFFSET_BITS=64 -I "../debugging" -I "../rawIO11" -I "../sofs11"

OBJS  = sofs_syscalls_others.o
OBJS += sofs_syscalls_link.o
OBJS += sofs_syscalls_unlink.o
OBJS += sofs_syscalls_rename.o
OBJS += sofs_syscalls_mknod.o
OBJS += sofs_syscalls_read.o
OBJS += sofs_syscalls_write.o
OBJS += sofs_syscalls_truncate.o
OBJS += sofs_syscalls_mkdir.o
OBJS += sofs_syscalls_rmdir.o
OBJS += sofs_syscalls_readdir.o
OBJS += sofs_syscalls_symlink.o
OBJS += sofs_syscalls_readlink.o

GIVEN_OBJS = sofs_syscalls_bin.o

all:			$(GIVEN_OBJS) libsyscalls11

libsyscalls11:		$(OBJS)
			ar -r libsyscalls11.a $(GIVEN_OBJS) $^
			cp libsyscalls11.a ../../lib
			rm -f $^ libsyscalls11.a

clean:
			rm -f ../../lib/libsyscalls11.a
