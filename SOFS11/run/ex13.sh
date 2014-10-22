#!/bin/bash

# This test vector deals mainly with operations add / remove directory entries.
# It defines a storage device with 100 blocks and formats it with 56 inodes.
# It starts by allocating thirty two inodes, associated to regular files and directories, and organize them as entries in the root directory. It
# ends by removing all of them. The major aim is to check the growth of the root directory.
# The showblock_sofs11 application should be used in the end to check metadata.

./createEmptyFile myDisk 100
./mkfs_sofs11 -i 56 -z myDisk
./testifuncs11 -b -l 100,500 -L testVector13.rst myDisk <testVector13.cmd
