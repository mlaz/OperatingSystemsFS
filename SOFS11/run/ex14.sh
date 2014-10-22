#!/bin/bash

# This test vector deals mainly with operations get a directory entry by path / get a directory by name.
# It defines a storage device with 100 blocks and formats it with 56 inodes data clusters.
# It starts by allocating seven inodes, associated to regular files and directories, and organize them in a hierarchical faction. Then it proceeds by
# trying to find different directory entries through the use of different paths.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs11 -i 56 -z myDisk
./testifuncs11 -b -l 100,500 -L testVector14.rst myDisk <testVector14.cmd
