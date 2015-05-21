/**
 *  \file sofs_ifuncs_1_fi.c (implementation file of function soFreeInode)
 *
 *  \author Artur Carneiro Pereira - September 2008
 *  \author António Rui Borges - September 2010 / September 2011
 *  \author Miguel Oliveira e Silva - September 2011
 *  \author Grupo 2 P6 - September 2011
 *
 *  \remarks In case an error occurs, all functions return a negative value which is the symmetric of the system error
 *           or the local error that better represents the error cause. Local errors are out of the range of the
 *           system errors.
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"


/** binary implementation prototype */
int soFreeInode_bin (uint32_t nInode);

/**
 *  \brief Free the referenced inode.
 *
 *  The inode must be in use, belong to one of the legal file types and have no directory entries associated with it
 *  (refcount = 0).
 *  The inode is marked free in the dirty state and inserted in the list of free inodes.
 *
 *  Notice that the inode 0, supposed to belong to the file system root directory, can not be freed.
 *
 *  The only affected fields are:
 *     \li the free flag of mode field, which is set
 *     \li the <em>time of last file modification</em> and <em>time of last file access</em> fields, which change their
 *         meaning: they are replaced by the <em>prev</em> and <em>next</em> pointers in the double-linked list of free
 *         inodes.
 * *
 *  \param nInode number of the inode to be freed
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soFreeInode (uint32_t nInode)
{
  soProbe (412, "soFreeInode (%"PRIu32")\n", nInode);

  /** Variables **/
  int status;
  uint32_t freeBlock;
  uint32_t tailBlock;
  uint32_t freeOffset;
  uint32_t tailOffset;
  uint32_t tailInodeNumber;
  SOInode *freeInode;
  SOInode *tailInode;
  SOSuperBlock *sb;

  /** Loading SuperBlock **/
  if((status = soLoadSuperBlock()) < 0)
    return status;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;
  
  /** Parameter validation **/
  if(nInode <= 0 || nInode >= sb->itotal)
    return -EINVAL;

  /** Inode table consistency check **/
  if((status = soQCheckInT(sb)) < 0)
    return status;

  /** Read inode to be freed **/
  if((status = soConvertRefInT(nInode, &freeBlock, &freeOffset)) < 0)
    return status;
  if((status = soLoadBlockInT(freeBlock)) < 0)
    return status;
  if((freeInode = soGetBlockInT()) == NULL)
    return -ELIBBAD;

  /** Check if inode is in use **/
  if((status = soQCheckInodeIU(sb, &freeInode[freeOffset])) < 0)
    return status;

  /** Check if inode belongs one of the legal file types  **/
  if((freeInode[freeOffset].mode & INODE_DIR) != INODE_DIR)
    if((freeInode[freeOffset].mode & INODE_FILE) != INODE_FILE)
      if((freeInode[freeOffset].mode & INODE_SYMLINK) != INODE_SYMLINK)
        return -ELIBBAD;

  /** Check if there are no entries associated with the inode **/
  if(freeInode[freeOffset].refcount != 0)
    return -ELIBBAD;

  /** Free inode **/
  freeInode[freeOffset].mode = freeInode[freeOffset].mode | INODE_FREE;
  freeInode[freeOffset].vD1.next = NULL_INODE;
  /*Insert inode in the double-linked list of free inodes*/
  if(sb->itail == NULL_INODE){
    sb->ihead = nInode;
    freeInode[freeOffset].vD2.prev = NULL_INODE;
  }
  else
    freeInode[freeOffset].vD2.prev = sb->itail;
  /*Write updated inode to disk*/
  if((status = soStoreBlockInT()) < 0)
    return status;

  /** Update previous tail inode **/
  if(sb->itail != NULL_INODE){
    if((status = soConvertRefInT(sb->itail, &tailBlock, &tailOffset)) < 0)
      return status;
    if((status = soLoadBlockInT(tailBlock)) < 0)
      return status;
    if((tailInode = soGetBlockInT()) == NULL)
      return -ELIBBAD;
    tailInode[tailOffset].vD1.next = nInode;
    if((status = soStoreBlockInT()) < 0)
      return status;
  }

  /** Update SuperBlock **/
  sb->itail = nInode;
  sb->ifree++;
  if((status = soStoreSuperBlock()) < 0)
    return status;

  /** Operation successful **/
  return 0;
}

/*
 *  The only affected fields are:
 *     \li the free flag of mode field, which is set
 *     \li the <em>time of last file modification</em> and <em>time of last file access</em> fields, which change their
 *         meaning: they are replaced by the <em>prev</em> and <em>next</em> pointers in the double-linked list of free
 *         inodes.
*/

