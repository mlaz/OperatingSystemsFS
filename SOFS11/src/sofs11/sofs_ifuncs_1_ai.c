/**
 *  \file sofs_ifuncs_1_ai.c (implementation file of function soAllocheadInode)
 *
 *  \author Artur Carneiro Pereira - September 2008
 *  \author António Rui Borges - September 2010 / September 2011
 *  \author Miguel Oliveira e Silva - September 2011
 *  \author Rui Neto       42520 T6G2 - September 2011
 *  \author Monica Feitosa 48613 T6G2 - September 2011
 *  \author Gilberto Matos 27657 T6G2 - September 2011
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
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_2.h"



/**
 *  \brief Allocate a free Inode.
 *
 *  The Inode is retrieved from the list of free Inodes, marked in use, associated to the legal file type passed as
 *  a parameter and generally initialized. It must be free and if is free in the dirty state, it has to be cleaned
 *  first.
 *
 *  Upon initialization, the new headInode has:
 *     \li the field mode set to the given type, while the free flag and the permissions are reset
 *     \li the owner and group fields set to current userid and groupid
 *     \li the <em>prev</em> and <em>next</em> fields, pointers in the double-linked list of free Inodes, change their
 *         meaning: they are replaced by the <em>time of last file modification</em> and <em>time of last file
 *         access</em> which are set to current time
 *     \li the reference fields set to NULL_CLUSTER
 *     \li all other fields reset.

 *  \param type the inode type (it must represent either a file, or a directory, or a symbolic link)
 *  \param p_nInode pointer to the location where the number of the just allocated inode is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>type</em> is illegal or the <em>pointer to inode number</em> is \c NULL
 *  \return -\c ENOSPC, if the list of free inodes is empty
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -\c EFININVAL, if the free inode is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAllocInode (uint32_t type, uint32_t* p_nInode)
{
  soProbe (411, "soAllocInode (%"PRIu32", %p)\n", type, p_nInode);
  
  /** Variables **/
  int status;
  SOSuperBlock *sb;
  SOInode *headInode;
  SOInode *nextInode;
  uint32_t headBlock;
  uint32_t nextBlock;
  uint32_t headOffset;
  uint32_t nextOffset;
  uint32_t nInode;

  /** Parameter check **/
  if((type != INODE_DIR) && (type != INODE_FILE) && (type != INODE_SYMLINK))
    return -EINVAL;
  if(p_nInode == NULL)
    return -EINVAL;

  /** Loading SuperBlock **/
  if((status = soLoadSuperBlock()) != 0)
    return status;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Inode table consistency check **/
  if((status = soQCheckInT(sb)) != 0)
    return status;

  /** Check if there are free inodes **/
  if(sb->ifree == 0)
    return -ENOSPC;

  /** Obtain Block number and inode offset **/
  if((status = soConvertRefInT(sb->ihead, &headBlock, &headOffset)) != 0)
    return status;
  /** Read head inode from inode table **/
  if((status = soLoadBlockInT(headBlock)) != 0)
    return status;
  headInode = soGetBlockInT();

  /** Check if inode is free **/
  if((status = soQCheckFInode(&headInode[headOffset])) != 0)
    return status;

  /** Allocated inode number **/
  nInode = sb->ihead;  

  /**Update Superblock reference to head of double-linked list of free headInodes**/
  if(sb->ifree == 1)
  {
    sb->ihead = NULL_INODE;
    sb->itail = NULL_INODE;
  }
  else
    sb->ihead = headInode[headOffset].vD1.next;

  /** Update next inode information **/
  if(sb->ihead != NULL_INODE)
  {
    if((status = soConvertRefInT(sb->ihead, &nextBlock, &nextOffset)) != 0)
      return status;
    if((status = soLoadBlockInT(nextBlock)) != 0)
      return status;
    nextInode = soGetBlockInT();
    nextInode[nextOffset].vD2.prev = NULL_INODE;
    if((status = soStoreBlockInT()) != 0)
      return status;
  }

  /** Update and write superblock **/
  sb->ifree--;
  if((status = soStoreSuperBlock()) != 0)
    return status;

  /** "Re-Obtain" Block number and inode offset **/
  if((status = soConvertRefInT(nInode, &headBlock, &headOffset)) != 0)
    return status;
  /** "Re-Read" head inode from inode table **/
  if((status = soLoadBlockInT(headBlock)) != 0)
    return status;
  headInode = soGetBlockInT();

  /** Check if headInode is in dirty state **/
  if((status = soQCheckFCInode(&headInode[headOffset])) != 0)
  {
    if((status = soCleanInode(headBlock * IPB + headOffset)) != 0)
      return status;
    /*Since inode was updated, it will be readed again*/
    if((status = soConvertRefInT(nInode, &headBlock, &headOffset)) != 0)
      return status;
    /*Read head inode from inode table*/
    if((status = soLoadBlockInT(headBlock)) != 0)
      return status;
    headInode = soGetBlockInT();
  }

  /** Allocate headInode **/
  headInode[headOffset].mode = type;
  headInode[headOffset].refcount = 0;
  headInode[headOffset].owner = getuid();
  headInode[headOffset].group = getgid();
  headInode[headOffset].size = 0;
  headInode[headOffset].clucount = 0;
  headInode[headOffset].vD1.atime = time(NULL);
  headInode[headOffset].vD2.mtime = headInode[headOffset].vD1.atime;

  /*Write updated inode information to disk*/
  if((status = soStoreBlockInT()) != 0)
    return status;
  /*Consistency check*/
  if((status = soQCheckInodeIU(sb, &headInode[headOffset])) != 0)
    return status;

  /** Operation successful **/
  *p_nInode = nInode;
  return 0;
}

