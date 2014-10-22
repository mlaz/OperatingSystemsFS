/**
 *  \file sofs_syscalls_link.c (implementation file for syscall soLink)
 *
 *  \brief Set of operations to manage system calls.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li mount the SOFS10 file system
 *      \li unmount the SOFS10 file system
 *      \li get file system statistics
 *      \li get file status
 *      \li check real user's permissions for a file
 *      \li change permissions of a file
 *      \li change the ownership of a file
 *      \li make a new name for a file
 *      \li delete the name of a file from a directory and possibly the file it refers to from the file system
 *      \li change the name or the location of a file in the directory hierarchy of the file system
 *      \li create a regular file with size 0
 *      \li open a regular file
 *      \li close a regular file
 *      \li read data from an open regular file
 *      \li write data into an open regular file
 *      \li synchronize a file's in-core state with storage device
 *      \li create a directory
 *      \li delete a directory
 *      \li open a directory for reading
 *      \li read a direntry from a directory
 *      \li close a directory
 *      \li make a new name for a regular file or a directory
 *      \li read the value of a symbolic link.
 *
 *  \author Artur Carneiro Pereira September 2007
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - October 2010 / October 2011
 *  \author Monica Feitosa 48613 T6G2 - November 2011
 *  \author Gilberto Matos 27657 T6G2 - November 2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_const.h"
#include "sofs_rawdisk.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"
#include "sofs_ifuncs_4.h"


/**
 *  \brief Make a new name for a file.
 *
 *  It tries to emulate <em>link</em> system call.
 *
 *  \param oldPath path to an existing file
 *  \param newPath new path to the same file
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to either of the strings are \c NULL or the path strings do not describe absolute
 *                      paths
 *  \return -\c ENAMETOOLONG, if the paths names or any of their components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of both paths, but the last one, are not directories
 *  \return -\c ELOOP, if either path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>oldPath</tt>, or to any of the
 *                       components of <tt>newPath</tt>, but the last one, is found
 *  \return -\c EEXIST, if a file described by <tt>newPath</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of both paths, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory where
 *                     <tt>newPath</tt> entry is to be added, or <tt>oldPath</tt> represents a directory
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soLink (const char *oldPath, const char *newPath)
{
  soProbe (74, "soLink (\"%s\", \"%s\")\n", oldPath, newPath);

  /** Variables **/
  int32_t error;
  uint32_t nInodeNewDir;
  uint32_t nInodeOldDir;
  uint32_t nInodeEnt;

  SOInode InodeEnt;

  char auxPath[MAX_PATH + 1];
  char oldDir[MAX_PATH + 1];
  char newDir[MAX_PATH + 1];
  char oldEnt[MAX_NAME + 1];
  char newEnt[MAX_NAME + 1];

  /** Parameter check **/
  /*Check if either of the pointers are NULL*/
  if(oldPath == NULL || newPath == NULL)
    return -EINVAL;
  /*Check if paths describe absolute paths*/
  if((strncmp("/", oldPath, 1) != 0) || (strncmp("/", newPath, 1) != 0))
    return -EINVAL;
  /*Check if path names exceed max length*/
  if((strlen(oldPath) > MAX_PATH) || (strlen(newPath) > MAX_PATH))
    return -ENAMETOOLONG;

  /** Get old and new directory names and respective entrys **/
  strcpy((char *) auxPath, oldPath);
  strcpy(oldDir, dirname(auxPath));
  strcpy((char *) auxPath, oldPath);
  strcpy(oldEnt, basename(auxPath));

  strcpy((char *) auxPath, newPath);
  strcpy(newDir, dirname(auxPath));
  strcpy((char *) auxPath, newPath);
  strcpy(newEnt, basename(auxPath));

  /** Get old directory and old entry inode numbers **/
  if((error = soGetDirEntryByPath(oldDir, NULL, &nInodeOldDir)) != 0)
    return error;
  if((error = soGetDirEntryByName(nInodeOldDir, oldEnt, &nInodeEnt, NULL)) != 0)
    return error;

  /** Read old entry inode **/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Check if oldPath is a directory **/
  if((InodeEnt.mode & INODE_TYPE_MASK) == INODE_DIR)
    return -EPERM;

  /** Get new directory inode number **/
  if((error = soGetDirEntryByPath(newDir, NULL, &nInodeNewDir)) != 0)
    return error;

  /** Check if a file described by newPath already exists **/
  if((error = soGetDirEntryByName(nInodeNewDir, newEnt, NULL, NULL)) != (-ENOENT))
  {
    if(error == 0) return -EEXIST;
    else return error;
  }

  /** Check if process has write permission on newDir **/
  if((error = soAccessGranted(nInodeNewDir, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }

  /** Add new entry to new directory **/
  if((error = soAddDirEntry(nInodeNewDir, newEnt, nInodeEnt)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
