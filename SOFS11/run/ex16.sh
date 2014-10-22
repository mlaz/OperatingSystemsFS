#!/bin/bash

# This test vector deals mainly with operations attach a directory, detach a direntry and add a direntry.
# It defines a storage device with 100 blocks and formats it with 56 inodes data clusters.
# It starts by allocating ten inodes, associated to regular files, directories and symbolic links, and organize them in a hierarchical faction. Then it proceeds by
# moves one directory in the hierarchical tree.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs11 -i 56 -z myDisk
./testifuncs11 -b -l 100,500 -L testVector16.rst myDisk <testVector16.cmd
