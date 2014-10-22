/**
 *  \file sofs_syscalls_readlink.c (implementation file for syscall soReadlink)
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
 *  \brief Read the value of a symbolic link.
 *
 *  It tries to emulate <em>readlink</em> system call.
 *
 *  \param ePath path to the symbolic link
 *  \param buff pointer to the buffer where data to be read is to be stored
 *  \param size buffer size in bytes
 *
 *  \return <em>number of bytes effectively read</em>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path or
 *                      <tt>ePath</tt> does not represent a symbolic link
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length or the buffer
 *                            size is not large enough
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not read permission on the symbolic link described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */


int soReadlink (const char *ePath, char *buff, int32_t size)
{
  soProbe (85, "soReadlink (\"%s\", %p, %u)\n", ePath, buff, size);

  /** Variables **/
  int error;
  char auxPath[MAX_PATH + 1];
  char dirPath[MAX_PATH + 1];
  char symName[MAX_NAME + 1];
  uint32_t nInodeSym;
  uint32_t nInodeDir;
  SOInode InodeSym;

  /** Parameter check **/
  if(ePath == NULL || buff == NULL)
    return -EINVAL;
  if(strlen(ePath) > MAX_PATH)
    return -EINVAL;
  if(size < 0)
    return -ENAMETOOLONG;

  /** Check if ePath exists **/
  if((error = soGetDirEntryByPath(ePath, NULL, NULL)) != 0)
    return error;

  /** Get directory path and symbolic link name **/
  strcpy((char *) auxPath, ePath);
  strcpy(dirPath, dirname(auxPath));
  strcpy((char *) auxPath, ePath);
  strcpy(symName, basename(auxPath));

  /** Get directory inode number **/
  if((error = soGetDirEntryByPath(dirPath, NULL, &nInodeDir)) != 0)
    return error;

  /** Get symbolic link inode number **/
  if((error = soGetDirEntryByName(nInodeDir, symName, &nInodeSym, NULL)) != 0)
    return error;

  /** Read symbolic link inode **/
  if((error = soReadInode(&InodeSym, nInodeSym, IUIN)) != 0)
    return error;

  /** Check if ePath represents a symbolic link **/
  if((InodeSym.mode & INODE_TYPE_MASK) != INODE_SYMLINK)
    return error;

  /** Check if process has read permission on the symbolic link **/
  if((error = soAccessGranted(nInodeSym, R)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }

  /** Read the value of the symbolic link **/
  if((error = soReadFileCluster(nInodeSym, 0, buff)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
