#!/bin/bash

# This test vector deals mainly with operations alloc / free data clusters through the operation handle file cluster which is always used.
# It defines a storage device with 100 blocks and formats it with 23 data clusters.
# It starts by allocating an inode, then it proceeds by allocating 13 data clusters in all the reference areas (direct, single indirect and
# double indirect). This means that in fact 20 data clusters are allocated.
# Then all data clusters are freed in reverse order and the inode is also freed, leaving it in the dirty state.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs11 -i 40 -z myDisk
./testifuncs11 -b -l 200,500 -L testVector7.rst myDisk <testVector7.cmd
