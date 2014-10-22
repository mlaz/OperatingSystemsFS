/**
 *  \file sofs_ifuncs_2_ri.c (implementation file for function soReadInode)
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
 *  \brief Read specific inode data from the table of inodes.
 *
 *  The inode may be either in use and belong to one of the legal file types or be free in the dirty state.
 *  Upon reading, the <em>time of last file access</em> field is set to current time, if the inode is in use.
 *
 *  \param p_inode pointer to the buffer where inode data must be read into
 *  \param nInode number of the inode to be read from
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

int soReadInode (SOInode *p_inode, uint32_t nInode, uint32_t status)
{
  soProbe (311, "soReadInode (%p, %"PRIu32", %"PRIu32")\n", p_inode, nInode, status);

  /** Variables **/
  int error;
  uint32_t nBlock, Offset;
  SOInode *readInode;
  SOSuperBlock *sb;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Parameter validation **/
  if((nInode >= sb->itotal) || (p_inode == NULL))
    return -EINVAL;
  if((status != IUIN) && (status != FDIN))
    return -EINVAL;

  /** Read inode **/
  if((error = soConvertRefInT(nInode, &nBlock, &Offset)) != 0)
    return error;
  if((error = soLoadBlockInT(nBlock)) != 0)
    return error;
  if((readInode = soGetBlockInT()) == NULL)
    return -ELIBBAD;

  /** Consistency check **/
  if(status == IUIN)
  {
    if((error = soQCheckInodeIU(sb, &readInode[Offset])) != 0)
      return error;
    /*If Inode in use, update time of last file access*/
    readInode[Offset].vD1.atime = time(NULL);
    if((error = soStoreBlockInT()) != 0)
      return error;
    /** Read inode **/
    if((error = soConvertRefInT(nInode, &nBlock, &Offset)) != 0)
      return error;
    if((error = soLoadBlockInT(nBlock)) != 0)
      return error;
    if((readInode = soGetBlockInT()) == NULL)
      return -ELIBBAD;
  }
  else
    if((error = soQCheckFDInode(sb, &readInode[Offset])) != 0)
      return error;

  *p_inode = readInode[Offset];

  /** Operation successful **/
  return 0;
}
