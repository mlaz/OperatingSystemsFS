/**
 *  \file sofs_ifuncs_2_wi.c (implementation file for function soWriteInode)
 *
 *  \brief Set of operations to manage inodes: level 2 of the internal file system organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li read specific inode data from the table of inodes
 *      \li write specific inode data to the table of inodes
 *      \li clean an inode
 *      \li check the inode access permissions against a given operation.
 *
 *  \author Artur Carneiro Pereira September 2008
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - September 2010 / September 2011
 *  \author Miguel Azevedo 38569 T6G2 - October 2011
 *  \author Gilberto Matos 27657 T6G2 - October 2011
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
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_2.h"

/**
 *  \brief Write specific inode data to the table of inodes.
 *
 *  The inode must be in use and belong to one of the legal file types.
 *  Upon writing, the <em>time of last file modification</em> and <em>time of last file access</em> fields are set to
 *  current time, if the inode is in use.
 *
 *  \param p_inode pointer to the buffer containing the data to be written from
 *  \param nInode number of the inode to be written into
 *  \param status inode status (in use / free in the dirty state)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>buffer pointer</em> is \c NULL or the <em>inode number</em> is out of range or the
 *                      inode status is invalid
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soWriteInode (SOInode *p_inode, uint32_t nInode, uint32_t status)
{
  soProbe (312, "soWriteInode (%p, %"PRIu32", %"PRIu32")\n", p_inode, nInode, status);

  int stat; /* status variable for error checking */
  SOInode *inodeTblk; /* pointer to the block where the inode table's segment
                         which is going to be used belongs */
  uint32_t nBlk; /* block number */
  uint32_t offset; /* offset withing the fetched block*/
  SOSuperBlock* p_sb;

  /* checking if the given inode pointer is NULL */
  if (p_inode == NULL)
    return -EINVAL;

  /* getting superblock */
  if ( (stat = soLoadSuperBlock() != 0) )
    return stat;

  p_sb = soGetSuperBlock();

  /* checking if the given inode number is in range */
  if (nInode >= p_sb->itotal)
    return -EINVAL;

  /* checking status parameter integrity */
  switch (status)
    {
    case IUIN:
      if ( (stat = soQCheckInodeIU(p_sb, p_inode)) != 0 )
        return stat;
      break;

    case FDIN:
      if ( (stat = soQCheckFDInode(p_sb, p_inode)) != 0 )
        return stat;
      break;

    default:
      return -EINVAL;
    }

  /* checking if the given inode belongs to a legal file type */
  switch (p_inode->mode & INODE_TYPE_MASK)
    {
    case INODE_DIR:
      break;
    case INODE_FILE:
      break;
    case INODE_SYMLINK:
      break;
    default:
      if (status == IUIN)
        return -EIUININVAL;
      return -EFDININVAL;
    }

  /* getting inode from the inode table */
  if ( (stat = soConvertRefInT(nInode, &nBlk, &offset)) != 0 )
    return stat;

  if ( (stat = soLoadBlockInT(nBlk)) != 0 )
    return stat;

  inodeTblk = soGetBlockInT();
  if (inodeTblk == NULL)
    return -ELIBBAD;

  /* writing into inode table */
  inodeTblk[offset] = *p_inode;

  if(status == IUIN)
  {
    inodeTblk[offset].vD1.atime = time(NULL);
    inodeTblk[offset].vD2.mtime = inodeTblk[offset].vD1.atime;
  }

  if ( (stat = soStoreBlockInT()) != 0 )
    return stat;

  return 0;
}
