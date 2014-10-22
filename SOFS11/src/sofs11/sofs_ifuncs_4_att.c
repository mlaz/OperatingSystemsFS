/**
 *  \file sofs_ifuncs_4_att.c (implementation file for function soAttachDirectory)
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
 *  \brief Attach a directory entry to a directory.
 *
 *  A new directory entry whose name is <tt>eName</tt> and whose inode number is <tt>nInodeDirSub</tt> is attached to
 *  the directory, the <em>base directory</em>, associated to the inode whose number is <tt>nInodeDirBase</tt>. The
 *  entry to be attached is supposed to represent itself a fully organized directory, the <em>subsidiary directory</em>.
 *  Thus, both inodes must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should not already be any entry in the base directory whose <em>name</em> field is
 *  <tt>eName</tt>.
 *
 *  The <em>refcount</em> field of both inodes are updated. The <em>size</em> field associated to the base directory
 *  may also be updated.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDirBase number of the inode associated to the base directory
 *  \param eName pointer to the string holding the name of the subsidiary directory
 *  \param nInodeDirSub number of the inode associated to the subsidiary directory
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the <em>inode numbers</em> are out of range or the pointer to the string is \c NULL
 *                      or the name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inodes do not describe directories
 *  \return -\c EEXIST, if an entry with the <tt>eName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the base directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the base directory
 *  \return -\c EMLINK, if the maximum number of hardlinks in either one of inodes has already been attained
 *  \return -\c EFBIG, if the base directory has already grown to its maximum size
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

int soAttachDirectory (uint32_t nInodeDirBase, const char *eName, uint32_t nInodeDirSub)
{
  soProbe (117, "soAttachDirectory_bin (%"PRIu32", \"%s\", %"PRIu32")\n", nInodeDirBase, eName, nInodeDirSub);

  /* Substitute by your code */
  int soAttachDirectory_bin (uint32_t nInodeDirBase, const char *eName, uint32_t nInodeDirSub);
  return soAttachDirectory_bin(nInodeDirBase, eName, nInodeDirSub);
}
