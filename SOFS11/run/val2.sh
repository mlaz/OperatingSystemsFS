#!/bin/bash

# This test vector checks if a short text file can be copied to the root directory.
# System calls developed by the students which are involved: readdir, mknode, read and write.

echo -e '\n**** Creating the storage device.****\n'
./createEmptyFile myDisk 100
echo -e '\n**** Converting the storage device into a SOFS11 file system.****\n'
./mkfs_sofs11_bin -i 56 -z myDisk
echo -e '\n**** Mounting the storage device as a SOFS11 file system.****\n'
./mount_sofs11_bin myDisk mnt
echo -e '\n**** Copying the text file.****\n'
cp val2.sh mnt
echo -e '\n**** Listing the root directory.****\n'
ls -la mnt
echo -e '\n**** Getting the file attributes.****\n'
stat mnt/val2.sh
echo -e '\n**** Displaying the file contents.****\n'
cat mnt/val2.sh
echo -e '\n**** Unmounting the storage device.****\n'
fusermount -u mnt
