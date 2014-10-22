#!/bin/bash

# This test vector deals with the operations alloc inode and alloc / free data cluster.
# It defines a storage device with 222 blocks and formats it with an inode table of 8 inodes.
# It starts by allocating inodes. Then, it allocates all the data clusters. Finally, it frees all the data
# clusters in reverse order.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 222
./mkfs_sofs11 -i 8 -z myDisk
./testifuncs11 -b -l 200,500 -L testVector5.rst myDisk <testVector5.cmd
