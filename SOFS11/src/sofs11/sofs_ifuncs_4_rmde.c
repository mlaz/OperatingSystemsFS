/**
 *  \file sofs_ifuncs_4_rmde.c (implementation file for function soRemoveDirEntry)
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
 *  \author André Garcia   35853 T6G2 - October 2011
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
 *  \brief Remove an entry from a directory.
 *
 *  The entry whose name is <tt>eName</tt> is removed from the directory associated with the inode whose number is
 *  <tt>nInodeDir</tt>. Thus, the inode must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should exist an entry in the directory whose <em>name</em> field is <tt>eName</tt>.
 *
 *  Whenever the type of the inode associated to the entry to be removed is of directory type, the operation can only
 *  be carried out if the directory is empty.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be removed and, when required, of the inode
 *  associated to the directory are updated. The file described by the inode associated to the entry to be removed is
 *  only deleted from the file system if the <em>refcount</em> field becomes zero (there are no more hard links
 *  associated to it). In this case, the data clusters that store the file contents and the inode itself must be freed.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be removed
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

int soRemoveDirEntry (uint32_t nInodeDir, const char *eName)
{
  soProbe (114, "soRemoveDirEntry (%"PRIu32", \"%s\")\n", nInodeDir, eName);

  /** Variables **/
  int status;
  SOSuperBlock *sb;
  SOInode inodeDir;
  SOInode inodeEnt;
  SODirEntry dirEntry[DPC];
  uint32_t nInodeEnt, idx, clusterNumber, offset;

  /** Loading SuperBlock **/
  if ((status = soLoadSuperBlock()) != 0)
    return status;
  if ((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Parameter check **/
  if ((eName == NULL) || (nInodeDir >= sb->itotal))
    return -EINVAL;
  if ((strlen(eName)) > MAX_NAME) 
    return -ENAMETOOLONG;
  if ((strchr(eName, '/')) != NULL) 
    return -ENOENT;

  /** Read Directory **/
  if ((status = soReadInode(&inodeDir, nInodeDir, IUIN)) !=0) 
    return status;

  /** If inode is a directory **/
  if ((inodeDir.mode & INODE_DIR) != INODE_DIR)
    return -ENOTDIR;

  /** Check write and execution permissions on directory **/
  if((status = soAccessGranted(nInodeDir, X)) != 0)
    return status;
  if((status = soAccessGranted(nInodeDir, W)) != 0)
  {
    if(status == (-EACCES)) return -EPERM;
    else return status;
  }

  /** Check the dirEntry consistence **/
  if ((status = soQCheckDirCont(sb, &inodeDir)) !=0)
    return status;

  /** Getting nInodeEnt and idx of dirEntry **/
  if ((status = soGetDirEntryByName(nInodeDir, eName, &nInodeEnt, &idx)) !=0)
    return status;

  /** Read Entry inode **/
  if ((status = soReadInode(&inodeEnt, nInodeEnt, IUIN)) != 0) 
    return status;

  /** If entry is directory, check if it is empty **/
  if((inodeEnt.mode & INODE_DIR) == INODE_DIR)
    if((status = soCheckDirectoryEmptiness(nInodeEnt)) != 0)
      return status;
 
  clusterNumber = idx / DPC;
  offset = idx % DPC;
  
  /** Read entry file cluster **/
  if ((status = soReadFileCluster(nInodeDir, clusterNumber, &dirEntry)) !=0) 
    return status;

  dirEntry[offset].name[MAX_NAME] = dirEntry[offset].name[0];		
  dirEntry[offset].name[0] = '\0';				

  /** Write updated entry file cluster **/
  if ((status = soWriteFileCluster(nInodeDir, clusterNumber, &dirEntry)) !=0)
    return status;

  /** Re-read entry inode **/
  if((status = soReadInode(&inodeEnt, nInodeEnt, IUIN)) != 0)
    return status;

  /** The refcount field of the inode associated to the entry removed **/
  if((inodeEnt.mode & INODE_DIR) == INODE_DIR)
    inodeEnt.refcount -= 2;
  else
    inodeEnt.refcount -= 1;

  /** Write updated entry's inode **/
  if ((status = soWriteInode(&inodeEnt, nInodeEnt, IUIN)) != 0)
    return status;

  /** Inode associated to the entry removed is deleted if the refcount is zero **/
  if (inodeEnt.refcount == 0)
  {
    if ((status = soHandleFileClusters(nInodeEnt, 0, FREE)) != 0)
      return status;
    if ((status = soFreeInode(nInodeEnt)) != 0) 
      return status;
    /*If entry is a directory, parent dir needs to be updated*/
    if((inodeEnt.mode & INODE_DIR) == INODE_DIR)
      inodeDir.refcount -= 1;
  }

  /** Update parent directory inode **/
  if((status = soWriteInode(&inodeDir, nInodeDir, IUIN)) != 0)
    return status;

  /** Operation successful **/
  return 0;
}
