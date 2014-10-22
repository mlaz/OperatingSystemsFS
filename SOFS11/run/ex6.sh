#!/bin/bash

# This test vector deals with operations read / write inode, alloc / free inode, access granted and parameter test for clean inode.
# It defines a storage device with 100 blocks and formats it with an inode table of 8 inodes.
# It starts by allocating two inodes, freeing one of them and setting their permissions. Several parameter combinations are tested.
# Then it tests different combinations of requested access operations.
# Finally, it tests parameter combinations for clean inode (its functionality can not be tested yet).
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs11 -i 8 -z myDisk
./testifuncs11 -b -l 200,500 -L testVector6.rst myDisk <testVector6.cmd
