/**
 *  \file sofs_ifuncs_4_det.c (implementation file for function soDetachDirEntry)
 *
 *  \brief Set of operations to manage directories and directory entries: level 4 of the internal file system
 *         organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li get an entry by path
 *      \li get an entry by name
 *      \li add an new entry to a directory
 *      \li remove an entry from a directory
 *      \li rename an entry of a directory
 *      \li check a directory status of emptiness
 *      \li attach a directory entry to a directory
 *      \li detach a directory entry from a directory.
 *
 *  \author Artur Carneiro Pereira September 2008
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - October 2010 / October 2011
 *
 *  \remarks In case an error occurs, all functions return a negative value which is the symmetric of the system error
 *           or the local error that better represents the error cause. Local errors are out of the range of the
 *           system errors.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"

/**
 *  \brief Detach an entry from a directory.
 *
 *  The entry whose name is <tt>eName</tt> is detached from the directory associated with the inode whose number is
 *  <tt>nInodeDir</tt>. Thus, the inode must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should exist an entry in the directory whose <em>name</em> field is <tt>eName</tt>.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be detached and, when required, of the inode
 *  associated to the directory are updated. The entry is cleaned (that is, it becomes free in the clean state).
 *  The file described by the inode associated to the entry to be detached is only erased from the file system if the
 *  <em>refcount</em> field becomes zero (there are no more hard links associated to it). In this case, the data
 *  clusters that store the file contents must be freed and cleaned and the inode itself must be freed.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be detached
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or the pointer to the string is \c NULL or the
 *                      name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>eName</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
 *  \return -\c ENOTEMPTY, if the entry with <tt>eName</tt> describes a non-empty directory
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soDetachDirEntry (uint32_t nInodeDir, const char *eName)
{
  soProbe (118, "soDetachDirectory_bin (%"PRIu32", \"%s\")\n", nInodeDir, eName);

  /* Substitute by your code */
  int soDetachDirEntry_bin (uint32_t nInodeDir, const char *eName);
  return soDetachDirEntry_bin(nInodeDir, eName);
}
