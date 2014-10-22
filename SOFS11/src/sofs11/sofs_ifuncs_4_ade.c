/**
 *  \file sofs_ifuncs_4_ade.c (implementation file for function soAddDirEntry)
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
 *  \author Rui Neto       42520 T6G2 - October 2011
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
 *  \brief Add an new entry to a directory.
 *
 *  A new entry whose name is <tt>eName</tt> and whose inode number is <tt>nInodeEnt</tt> is added to the
 *  directory associated with the inode whose number is <tt>nInodeDir</tt>. Thus, both inodes must be in use and belong
 *  to a legal type, the former, and to the directory type, the latter.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should not already be any entry in the directory whose <em>name</em> field is
 *  <tt>eName</tt>.
 *
 *  Whenever the type of the inode associated to the entry to be added is of directory type, the directory is
 *  initialized by setting its contents to represent an empty directory.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be added and, when required, of the inode
 *  associated to the directory are updated. This may also happen to the <em>size</em> field of either or both inodes.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be added
 *  \param nInodeEnt number of the inode associated to the directory entry to be added
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the <em>inode numbers</em> are out of range or the pointer to the string is \c NULL
 *                      or the name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> is not a directory
 *  \return -\c EEXIST, if an entry with the <tt>eName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
 *  \return -\c EMLINK, if the maximum number of hardlinks in either one of inodes has already been attained
 *  \return -\c EFBIG, if the directory has already grown to its maximum size
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

int soAddDirEntry (uint32_t nInodeDir, const char *eName, uint32_t nInodeEnt)
{
  soProbe (113, "soAddDirEntry (%"PRIu32", \"%s\", %"PRIu32")\n", nInodeDir, eName, nInodeEnt);

  /** Variables **/
  int error;
  uint32_t newEntryIdx;
  uint32_t currEntryIdx;
  uint32_t clusterNumber; /*Directory size in clusters*/
  uint32_t clusterOffset;
  SOSuperBlock *sb;
  SODirEntry dirEntry[DPC];
  SOInode InodeDir;
  SOInode InodeEnt;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -ELIBBAD;

  /** Conformity check **/
  /*Check inodes numbers range and pointer to string*/
  if((nInodeDir >= sb->itotal) || (nInodeEnt >= sb->itotal) || (eName == NULL))
    return -EINVAL;
  /*Check if name string describes a file name*/
  if(strchr(eName, '/') != NULL)
    return -EINVAL;
  /*Check if name string doesn't exceed maximum allowed length*/
  if(strlen(eName) > MAX_NAME)
    return -ENAMETOOLONG;

  /** Read directory inode **/
  if((error = soReadInode(&InodeDir, nInodeDir, IUIN)) != 0)
    return error;

  /** Check if the inode type whose number is nInodeDir is a directory **/
  if((InodeDir.mode & INODE_TYPE_MASK) != INODE_DIR)
    return -ENOTDIR;

  /** Check InodeDir consistency **/
  if((error = soQCheckDirCont(sb, &InodeDir)) != 0)
    return error;

  /** Check if an entry with eName already exists **/
  if((error = soGetDirEntryByName(nInodeDir, eName, NULL, &newEntryIdx)) == 0)
    return -EEXIST;
  if(error != (-ENOENT))
    return error;

  /** Check directory size **/
  if(newEntryIdx >= (DPC * MAX_FILE_CLUSTERS))
    return -EFBIG;

  /** Check write and execution permissions on directory **/
  if((error = soAccessGranted(nInodeDir, X)) != 0)
    return error;
  if((error = soAccessGranted(nInodeDir, W)) != 0)
  {
    if(error == (-EACCES))
      return -EPERM;
    else
      return error;
  }

  /** Read entry inode **/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Check number of hardlinks of entry inode **/
  if(InodeEnt.refcount == 0xFFFF)
    return -EMLINK;

  /** Check if entry is a directory **/
  if((InodeEnt.mode & INODE_TYPE_MASK) == INODE_DIR)
  {
    /*Check parent directory hardlinks*/
    if(InodeDir.refcount == 0xFFFF)
      return -EMLINK;

    /*Initialize "." entry*/
    dirEntry[0].nInode = nInodeEnt;
    strncpy((char *) dirEntry[0].name, ".", MAX_NAME + 1);
    /*Initialize ".." entry*/
    dirEntry[1].nInode = nInodeDir;
    strncpy((char *) dirEntry[1].name, "..", MAX_NAME + 1);
  
    /*Initialize remaining directory entrys*/
    for(currEntryIdx = 2; currEntryIdx < DPC; currEntryIdx++)
    {
      dirEntry[currEntryIdx].nInode = NULL_INODE;
      memset(&dirEntry[currEntryIdx].name, '\0', MAX_NAME + 1);
    }

    /*Update cluster - WriteFileCluster allocates a cluster if needed*/
    if((error = soWriteFileCluster(nInodeEnt, 0, dirEntry)) != 0)
      return error;

    /*Read updated entry inode*/
    if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
      return error;

    /*Update entry and directory inodes*/
    InodeEnt.size = sizeof(SODirEntry) * DPC;
    InodeEnt.refcount += 1;
    InodeDir.refcount += 1;

    /*Update inodes*/
    if((error = soWriteInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
      return error;
    if((error = soWriteInode(&InodeDir, nInodeDir, IUIN)) != 0)
      return error;
  }

  /*Get cluster number and offset of first free directory entry*/
  clusterNumber = newEntryIdx / DPC;
  clusterOffset = newEntryIdx % DPC;

  /**Check if new entry index will be in a new cluster**/
  if(clusterOffset == 0)
  {
    /*Read cluster - read file cluster will return a buffer filled with 0*/
    if((error = soReadFileCluster(nInodeDir, clusterNumber, dirEntry)) != 0)
      return error;

    /*Update first entry*/
    strncpy((char *) dirEntry[0].name, eName, MAX_NAME + 1);
    dirEntry[0].nInode = nInodeEnt;

    /*Update remaining entrys*/
    for(currEntryIdx = 1; currEntryIdx < DPC; currEntryIdx++)
    {
      dirEntry[currEntryIdx].nInode = NULL_INODE;
      strncpy((char *) dirEntry[currEntryIdx].name, "\0", MAX_NAME + 1);
    }

    /*Update cluster*/
    if((error = soWriteFileCluster(nInodeDir, clusterNumber, dirEntry)) != 0)
      return error;

    /*Read updated directory inode*/
    if((error = soReadInode(&InodeDir, nInodeDir, IUIN)) != 0)
      return error;

    /*Update and write directory inode*/
    InodeDir.size += sizeof(SODirEntry) * DPC;
    if((error = soWriteInode(&InodeDir, nInodeDir, IUIN)) != 0)
      return error;
  }
  else
  {
    /*Read cluster*/
    if((error = soReadFileCluster(nInodeDir, clusterNumber, dirEntry)) != 0)
      return error;

    /*Update entry*/
    dirEntry[clusterOffset].nInode = nInodeEnt;
    strncpy((char *) dirEntry[clusterOffset].name, eName, MAX_NAME + 1);

    /*Update cluster*/
    if((error = soWriteFileCluster(nInodeDir, clusterNumber, dirEntry)) != 0)
      return error;
  }

  /*Read entry inode*/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Update entry inode **/
  InodeEnt.refcount += 1;

  /** Write entry inode **/
  if((error = soWriteInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
