/**
 *  \file sofs_ifuncs_3_rfc (implementation file for function soReadFileCluster)
 *
 *  \brief Set of operations to manage data clusters: level 3 of the internal file system organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li read a specific data cluster
 *      \li write to a specific data cluster
 *      \li handle a file data cluster
 *      \li free and clean all data clusters from the list of references starting at a given point.
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
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"


/* Allusion to internal functions */
int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal);

/**
 *  \brief Read a specific data cluster.
 *
 *  Data is read from a specific data cluster which is supposed to belong to an inode associated to a file (a regular
 *  file, a directory or a symbolic link). Thus, the inode must be in use and belong to one of the legal file types.
 *
 *  If the cluster has not been allocated yet, the returned data will consist of a cluster whose byte stream contents
 *  is filled with the character null (ascii code 0).
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode where data is to be read from
 *  \param buff pointer to the buffer where data must be read into
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the <em>pointer to the buffer area</em> is \c NULL
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReadFileCluster (uint32_t nInode, uint32_t clustInd, void *buff)
{
  soProbe (211, "soReadFileCluster (%"PRIu32", %"PRIu32", %p)\n", nInode, clustInd, buff);

  /** Variables **/
  int error;
  uint32_t phyClustNum;
  uint32_t logClustNum;
  SOSuperBlock *sb;
  SODataClust Cluster;
  SOInode inode;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Parameter check **/
  if((nInode >= sb->itotal) || (clustInd >= MAX_FILE_CLUSTERS) || (buff == NULL))
    return -EINVAL;

  /** Read inode **/
  if((error = soReadInode(&inode, nInode, IUIN)) != 0)
    return error;
  /*Check if inode belongs to one the legal filetypes*/
  switch (inode.mode & INODE_TYPE_MASK)
  {
    case INODE_DIR:
    case INODE_FILE:
    case INODE_SYMLINK:
      break;
    default:
      return -EIUININVAL;
  }

  /** Get cluster logical number **/
  if((error = soHandleFileCluster(nInode, clustInd, GET, &logClustNum)) != 0)
    return error;

  /** Update buff with requested information **/
  if(logClustNum == NULL_CLUSTER)
    memset(buff, 0, BSLPC);
  else
  {
    phyClustNum = logClustNum * BLOCKS_PER_CLUSTER + sb->dzone_start;
    if((error = soReadCacheCluster(phyClustNum, &Cluster)) != 0)
      return error;
    memcpy(buff, &Cluster.info, BSLPC);
  }

  /*Operation successful*/
  return 0;
}

