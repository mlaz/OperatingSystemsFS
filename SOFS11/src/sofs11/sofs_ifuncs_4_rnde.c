/**
 *  \file sofs_ifuncs_4_rnde.c (implementation file for function soRenameDirEntry)
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
 *  \author Gilberto Matos 27657 T6G2 - October 2011
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
#include "sofs_ifuncs_4.h"


/**
 *  \brief Rename an entry of a directory.
 *
 *  The directory entry whose name is <tt>oldName</tt> has its <em>name</em> field changed to <tt>newName</tt>. Thus,
 *  the inode associated to the directory must be in use and belong to the directory type.
 *
 *  Both the <tt>oldName</tt> and the <tt>newName</tt> must be <em>base names</em> and not <em>paths</em>, that is,
 *  they can not contain the character '/'. Besides an entry whose <em>name</em> field is <tt>oldName</tt> should exist
 *  in the directory and there should not be any entry in the directory whose <em>name</em> field is <tt>newName</tt>.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param oldName pointer to the string holding the name of the direntry to be renamed
 *  \param newName pointer to the string holding the new name
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or either one of the pointers to the strings are
 *                      \c NULL or the name strings do not describe file names
 *  \return -\c ENAMETOOLONG, if one of the name strings exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>oldName</tt> is found
 *  \return -\c EEXIST,  if an entry with the <tt>newName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
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

int soRenameDirEntry (uint32_t nInodeDir, const char *oldName, const char *newName)
{
  soProbe (115, "soRenameDirEntry (%"PRIu32", \"%s\", \"%s\")\n", nInodeDir, oldName, newName);

  /** Variables **/
  int error;
  uint32_t idx;
  SOSuperBlock *sb;
  SOInode InodeDir;
  SODirEntry dirEntry[DPC];
  
  /** Loading superblock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -ELIBBAD;

  /** Conformity check **/
  if((nInodeDir >= sb->itotal) || (oldName == NULL) || (newName == NULL))
    return -EINVAL;
  if ((strlen(oldName) > MAX_NAME) || (strlen(newName) > MAX_NAME))
    return -ENAMETOOLONG;
  /** Check if names describe filenames **/
  if(strchr(oldName, '/') != NULL)
    return -EINVAL;
  if(strchr(newName, '/') != NULL)
    return -EINVAL;

  /** Read inode **/
  if((error = soReadInode(&InodeDir, nInodeDir, IUIN)) != 0)
    return error;

  /** Check if inode is a directory **/
  if((InodeDir.mode & INODE_DIR) != INODE_DIR)
    return -ENOTDIR;

  /** Check directory consistency **/
  if((error = soQCheckDirCont(sb, &InodeDir)) != 0)
    return error;

  /** Check write and execution permissions on directory **/
  if((error = soAccessGranted(nInodeDir, X)) != 0)
    return error;
  if((error = soAccessGranted(nInodeDir, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }

  /** Check if oldName exists **/
  if((error = soGetDirEntryByName(nInodeDir, oldName, NULL, &idx)) != 0)
    return error;

  /** Check if newName exists **/
  if(soGetDirEntryByName(nInodeDir, newName, NULL, NULL) == 0)
    return -EEXIST;

  /** Read cluster refering to oldName **/
  if((error = soReadFileCluster(nInodeDir, idx / DPC, &dirEntry)) != 0)
    return error;

  /** Rename entry **/
  strncpy ((char *)dirEntry[idx % DPC].name, newName, MAX_NAME+1);

  /** Write updated cluster **/
  if((error = soWriteFileCluster(nInodeDir, idx / DPC, &dirEntry)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
