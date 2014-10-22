#!/bin/bash

# This test vector checks the use of symbolic links in the file system.
# System calls developed by the students which are involved: readdir, mknode, read, write, mkdir, symlink and readlink.

echo -e '\n**** Creating the storage device.****\n'
./createEmptyFile myDisk 200
echo -e '\n**** Converting the storage device into a SOFS11 file system.****\n'
./mkfs_sofs11_bin -i 56 -z myDisk
echo -e '\n**** Mounting the storage device as a SOFS11 file system.****\n'
./mount_sofs11_bin myDisk mnt
echo -e '\n**** Creating the directory hierarchy.****\n'
mkdir mnt/ex
mkdir mnt/testVec
mkdir mnt/new
mkdir mnt/new/newAgain
cp ex*.sh mnt/ex
cp testVector*.cmd mnt/testVec
ln mnt/ex/ex10.sh mnt/new/newAgain/sameAsEx10.sh
ln -s ../../ex/ex5.sh mnt/new/newAgain/symlink1
ln -s ex/ex1.sh mnt/symlink2
sleep 1
echo -e '\n**** Listing the directories where symbolic links were introduced.****\n'
ls -la mnt
echo -e '\n********\n'
ls -la mnt/new/newAgain
echo -e '\n**** Displaying the file contents through a symbolic link which was purposefully created.****\n'
cat mnt/new/newAgain/symlink1
echo -e '\n**** Getting the hard link attributes.****\n'
stat mnt/new/newAgain/symlink1
echo -e '\n**** Displaying the file contents through a symbolic link which was purposefully created.****\n'
cat mnt/symlink2
echo -e '\n**** Getting the hard link attributes.****\n'
stat mnt/symlink2
echo -e '\n**** Unmounting the storage device.****\n'
sleep 1
fusermount -u mnt
