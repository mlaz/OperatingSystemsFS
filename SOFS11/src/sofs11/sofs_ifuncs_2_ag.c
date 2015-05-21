/**
 *  \file sofs_ifuncs_2_ag.c (implementation file for function soAccessGranted)
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
 *  \author André Garcia   35853 T6G2 - October 2011
 *  \author Gilberto Matos 27657 T6G2 - October 2011
 *  \author Miguel Azevedo 38569 T6G2 - October 2011
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
 *  \brief Check the inode access rights against a given operation.
 *
 *  The inode must to be in use and belong to one of the legal file types.
 *  It checks if the inode mask permissions allow a given operation to be performed.
 *
 *  When the calling process is <em>root</em>, access to reading and/or writing is always allowed and access to
 *  execution is allowed provided that either <em>user</em>, <em>group</em> or <em>other</em> have got execution
 *  permission.
 *
 *  \param nInode number of the inode
 *  \param opRequested operation to be performed:
 *                    a bitwise combination of R, W, and X
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if <em>buffer pointer</em> is \c NULL or no operation of the defined class is described
 *  \return -\c EACCES, ifint soReadInode (SOInode *p_inode, uint32_t nInode, uint32_t status) the operation is denied
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAccessGranted (uint32_t nInode, uint32_t opRequested)
{
  soProbe (314, "soAccessGranted (%"PRIu32", %"PRIu32")\n", nInode, opRequested);

  /** Variables **/
  int status;
  uint32_t mask;
  SOInode inode;
  SOSuperBlock *sb;

  /* Load SuperBlock */
  if ((status = soLoadSuperBlock()) != 0)
    return status;
  if ((sb = soGetSuperBlock()) == NULL)
    return -ELIBBAD;

  /* Parameter validation */
  if (nInode >= sb->itotal)
    return -EINVAL;
  if ((opRequested > 7) || (opRequested < 1))
    return -EINVAL;

  /* Read inode */
  if ((status = soReadInode(&inode, nInode, IUIN)) != 0)
    return status;

  /* Check if inode is in use */
  if ((status = soQCheckInodeIU(sb, &inode)) != 0)
    return status;

  /** Check if inode belongs one of the legal file types  **/
  switch (inode.mode & INODE_TYPE_MASK)
    {
    case INODE_DIR:
      break;
    case INODE_FILE:
      break;
    case INODE_SYMLINK:
      break;
    default:
      return -EIUININVAL;
    }

  uint32_t group = inode.group;
  uint32_t owner = inode.owner;
  uint32_t mode =  inode.mode;

  /**Check if current user is root**/
  if (getuid() == 0)
  {
    if ((opRequested & X) == X)
    {
      if (((mode & INODE_EX_USR) == INODE_EX_USR) || ((mode & INODE_EX_GRP) == INODE_EX_GRP) || ((mode & INODE_EX_OTH) == INODE_EX_OTH))
        return 0;
      else
        return -EACCES;
    }
    else
      return 0;
  }

  mask = opRequested;
  /** Check Other permissions **/
  if ( (mode & mask) == opRequested )
    return 0;

  mask = mask << 3;
  /** Check Group permissions **/
  if ( ((mode & mask) >> 3) == opRequested )
    if (getgid() == group)
      return 0;

  mask = mask << 3;
  /** Check user permissions **/
  if ( ((mode & mask) >> 6) == opRequested )
    if (getuid() == owner)
      return 0;

  /** Access not granted **/
  return -EACCES;
}
