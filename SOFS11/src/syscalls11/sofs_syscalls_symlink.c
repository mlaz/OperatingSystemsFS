/**
 *  \file sofs_syscalls_symlink.c (implementation file for syscall soSymlink)
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
 *  \brief Make a new name for a regular file or a directory.
 *
 *  It tries to emulate <em>symlink</em> system call.
 *
 *  \remark The permissions set for the symbolic link should have read (r), write (w) and execution (x) permissions for
 *          both <em>user</em>, <em>group</em> and <em>other</em>.
 *
 *  \param effPath path to be stored in the symbolic link file
 *  \param ePath path to the symbolic link
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to either of the strings are \c NULL or the second string does not describe an
 *                      absolute path
 *  \return -\c ENAMETOOLONG, if the either path names or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of the second path, but the last one, is not a directory
 *  \return -\c ELOOP, if the second path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt>, but the last one, is
 *                      found
 *  \return -\c EEXIST, if a file described by <tt>ePath</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of the second path, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory where
 *                     <tt>ePath</tt> entry is to be added
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soSymlink (const char *effPath, const char *ePath)
{
  soProbe (84, "soSymlink (\"%s\", \"%s\")\n", effPath, ePath);

  /** Variables **/
  int error;

  char buffer[CLUSTER_SIZE - 3 * sizeof(uint32_t)];
  char auxPath[MAX_PATH + 1];
  char dirPath[MAX_PATH + 1];
  char symName[MAX_NAME + 1];

  uint32_t nInodeDir;
  uint32_t nInodeSym;

  SOInode InodeDir;
  SOInode InodeSym;


  /** Parameter check **/
  if((effPath == NULL) || (ePath == NULL))
    return -EINVAL;
  if(strncmp("/", ePath, 1) != 0)
    return -EINVAL;

  /** Consistency check **/
  if((strlen(effPath) > MAX_PATH) || (strlen(ePath) > MAX_PATH))
    return -ENAMETOOLONG;

  /** Get directory path and symbolic link name **/
  strcpy((char *) auxPath, ePath);
  strcpy(dirPath, dirname(auxPath));
  strcpy((char *) auxPath, ePath);
  strcpy(symName, basename(auxPath));

  /** Consistency check - symbolic link name length **/
  if(strlen(symName) > MAX_NAME)
    return -ENAMETOOLONG;

  /** Check if symbolic link leads to an infinite loop **/
  if((strcmp(effPath, dirPath)) == 0)
    return -ELOOP;

  /** Check if no entry with symName exists **/
  if((error = soGetDirEntryByPath(ePath, NULL, NULL)) == 0)
    return -EEXIST;
  if(error != (-ENOENT))
    return error;

  /** Get parent directory inode number **/
  if((error = soGetDirEntryByPath(dirPath, NULL, &nInodeDir)) != 0)
    return error;

  /** Read parent directory inode **/
  if((error = soReadInode(&InodeDir, nInodeDir, IUIN)) != 0)
    return error;

  /** Check for write and execution permissions on parent directory **/
  if((error = soAccessGranted(nInodeDir, X)) != 0)
    return error;
  if((error = soAccessGranted(nInodeDir, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }

  /** Allocate symbolic link inode **/
  if((error = soAllocInode(INODE_SYMLINK, &nInodeSym)) != 0)
    return error;

  /** Read allocated inode **/
  if((error = soReadInode(&InodeSym, nInodeSym, IUIN)) != 0)
    return error;

  /** Update inode permissions **/
  InodeSym.mode = InodeSym.mode | INODE_RD_USR | INODE_WR_USR | INODE_EX_USR;
  InodeSym.mode = InodeSym.mode | INODE_RD_GRP | INODE_WR_GRP | INODE_EX_GRP;
  InodeSym.mode = InodeSym.mode | INODE_RD_OTH | INODE_WR_OTH | INODE_EX_OTH;

  /** Write updated inode **/
  if((error = soWriteInode(&InodeSym, nInodeSym, IUIN)) != 0)
    return error;

  /** Write effPath in symbolic link **/
  /*Prepare buffer*/
  memset(buffer, '\0', sizeof(buffer));
  strcpy(buffer, effPath);
  /*Write buffer in symbolic link's 0 cluster*/
  if((error = soWriteFileCluster(nInodeSym, 0, buffer)) != 0)
    return error;

  /** Read symbolic link inode (previously changed by soWriteFileCluster) **/
  if((error = soReadInode(&InodeSym, nInodeSym, IUIN)) != 0)
    return error;

  /** Update and write inode **/
  InodeSym.size = strlen(effPath);
  if((error = soWriteInode(&InodeSym, nInodeSym, IUIN)) != 0)
    return error;

  /** Add symbolic link entry to directory **/
  if((error = soAddDirEntry(nInodeDir, symName, nInodeSym)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
