#!/bin/bash

# This test vector deals with the operations alloc / free inode.
# It defines a storage device with 23 blocks and formats it with an inode table of 16 inodes.
# It starts by allocating some inodes and tests an error condition. Then, it frees all the allocated inodes and tests different error conditions.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 19
./mkfs_sofs11 -i 16 -z myDisk
./testifuncs11 -b -l 400,500 -L testVector1.rst myDisk <testVector1.cmd
