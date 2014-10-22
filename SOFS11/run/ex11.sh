#!/bin/bash

# This test vector deals mainly with operations write data clusters and clean inode.
# It defines a storage device with 1000 blocks and formats it with 247 data clusters.
# It starts by allocating an inode, then it proceeds by allocating 60 data clusters in all the reference areas (direct, single indirect and
# double indirect). This means that in fact 70 data clusters are allocated.
# Then all the data clusters are freed and the inode is freed and cleaned.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 1000
./mkfs_sofs11 -i 40 -z myDisk
./testifuncs11 -b -l 200,500 -L testVector11.rst myDisk <testVector11.cmd
