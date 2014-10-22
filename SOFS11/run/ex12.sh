#!/bin/bash

# This test vector deals mainly with operations get a directory entry by name, add / remove directory entries, rename directory entries and
# check a directory emptiness.
# It defines a storage device with 100 blocks and formats it with 56 inodes data clusters.
# It starts by allocating seven inodes, associated to regular files and directories, and organize them in a hierarchical faction. Then it proceeds by
# renaming some of the entries and ends by removing all of them.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs11 -i 56 -z myDisk
./testifuncs11 -b -l 100,500 -L testVector12.rst myDisk <testVector12.cmd
