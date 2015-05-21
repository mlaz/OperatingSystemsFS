/**
 *  \file sofs_ifuncs_1_adb.c (implementation file of function soAllocDataCluster)
 *
 *  \author Artur Carneiro Pereira - September 2008 / September 2011
 *  \author António Rui Borges - September 2010 / September 2011
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
#include <stdlib.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_3.h"

/** Auxiliary functions */
static int soReplenish(SOSuperBlock *sb);
static int soDeplete(SOSuperBlock *sb);

/**
 *  \brief Allocate a free data cluster and associate it to an inode.
 *
 *  The inode is supposed to be associated to a file (a regular file, a directory or a symbolic link), but the only
 *  consistency check at this stage should be to check the inode is not free.
 *
 *  The cluster is retrieved from the retrieval cache of free data cluster references. If the cache is empty, it has to
 *  be replenished before the retrieval may take place. If the data cluster is in the dirty state, it has to be cleaned
 *  first. The header fields of the allocated cluster should be all filled in: <tt>prev</tt> and <tt>next</tt> should be
 *  set to \c NULL_CLUSTER and <tt>stat</tt> to the given inode number.
 *
 *  \param nInode number of the inode the data cluster should be associated to
 *  \param p_nClust pointer to the location where the logical number of the allocated data cluster is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, the <em>inode number</em> is out of range or the <em>pointer to the logical data cluster
 *                      number</em> is \c NULL
 *  \return -\c ENOSPC, if there are no free data clusters
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAllocDataCluster (uint32_t nInode, uint32_t *p_nClust)
{
  soProbe (413, "soAllocDataCluster(%"PRIu32", %p)\n", nInode, p_nClust);
  
  /*Variables*/
  SOInode *inode;
  SODataClust allocCluster;
  SOSuperBlock *sb;
  int status;
  uint32_t offset;				/*Inode offset in the corresponding block of the inode table*/
  uint32_t nBlock;				/*Physical number of the block where the inode is stored*/
  uint32_t physCluster;				/*Physical number of the cluster that will be allocated*/
  uint32_t logiCluster;				/*Logical number of the cluster that will be allocated*/
  uint32_t AllocStatus;				/*Aux. variable to store allocation status of the cluster*/

  /** Loading SuperBlock **/
  if((status = soLoadSuperBlock()) != 0)
    return status;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Parameter validation **/
  if(nInode >= sb->itotal || p_nClust == NULL)
    return -EINVAL;

  /** Free data clusters check **/
  if(sb->dzone_free == 0)
    return -ENOSPC;

  /** Inode consistency check **/
  /*Inode's block and offset calculation*/
  if((status = soConvertRefInT(nInode, &nBlock, &offset)) != 0)
    return status;
  /*Load corresponding block*/
  if((status = soLoadBlockInT(nBlock)) != 0)
    return status;
  if((inode = soGetBlockInT()) == NULL)
    return -EIO;
  /*Check if inode is free*/
  if((soQCheckFInode(&inode[offset])) == 0)
    return -EIUININVAL;

  /** Retrieve cluster from the retrieval cache **/
  /*Check if Retrieval cache is empty*/
  if(sb->dzone_retriev.cache_idx == DZONE_CACHE_SIZE)
    if((status = soReplenish(sb)) != 0)
      return status;
  /* Check if cluster is not allocated */
  logiCluster = sb->dzone_retriev.cache[sb->dzone_retriev.cache_idx];
  if((status = soQCheckStatDC(sb, logiCluster, &AllocStatus)) != 0)
    return status;
  if(AllocStatus != FREE_CLT)
    return -EDCINVAL;
  /*Retrieve cluster*/
  physCluster = logiCluster * BLOCKS_PER_CLUSTER + sb->dzone_start;
  if((status = soReadCacheCluster(physCluster, &allocCluster)) != 0)
    return status;

  /** Update superblock **/
  sb->dzone_free--;
  sb->dzone_retriev.cache_idx++;
  if((status = soStoreSuperBlock()) != 0)
    return status;

  /** Check if data cluster is in dirty state **/
  if(allocCluster.stat != NULL_INODE)
  {
    if((status = soCleanDataCluster(allocCluster.stat, logiCluster)) != 0)
      return status;
  }

  /** Allocate Cluster **/
  allocCluster.stat = nInode;
  if((status = soWriteCacheCluster(physCluster, &allocCluster)) != 0)
    return status;

  /** Update p_nClust with the logical number of the allocated cluster **/
  *p_nClust = logiCluster;

  /** Operation successful **/
  return 0;
}

static int soReplenish(SOSuperBlock *sb)
{

  /** Variables **/
  int n;			/*number of retrieved clusters*/
  int status;				/*Operation status variable*/
  uint32_t currPhysical, nextPhysical;  /*Physical number of the current and next clusters*/
  uint32_t *auxArray;			/*Auxiliary array to store the logical number of the clusters*/
  SODataClust currCluster;		/*Current or Head cluster*/
  SODataClust nextCluster;		/*Next cluster in the gen. repository of free data clusters*/

  /** Parameter validation **/
  if(sb == NULL)
    return -EINVAL;

  /** Free data clusters check **/
  if(sb->dzone_free == 0)
    return -ENOSPC;

  /** General repository of free data clusters check **/
  if(sb->dhead == NULL_CLUSTER)
    if((status = soDeplete(sb)) != 0)
      return status;

  /** Allocate memomry for the auxiliary array **/
  if((auxArray = (uint32_t *) calloc (DZONE_CACHE_SIZE, sizeof(uint32_t))) == NULL)
    return -ELIBBAD;

  n = 0;
  /** Fill retrieval cache **/
  /*Remove clusters from general repository of free data clusters*/
  while((n != DZONE_CACHE_SIZE) && (sb->dhead != NULL_CLUSTER))
  {
    /*Read head clusrer*/
    currPhysical = sb->dzone_start + sb->dhead * BLOCKS_PER_CLUSTER;
    if((status = soReadCacheCluster(currPhysical, &currCluster)) != 0)
      return status;
    /*Update next cluster, if exists*/
    if(currCluster.next != NULL_CLUSTER)
    {
      nextPhysical = currCluster.next * BLOCKS_PER_CLUSTER + sb->dzone_start;
      if((status = soReadCacheCluster(nextPhysical, &nextCluster)) != 0)
        return status;
      nextCluster.prev = NULL_CLUSTER;
      if((status = soWriteCacheCluster(nextPhysical, &nextCluster)) != 0)
        return status;
    }
    /*Insert cluster in auxiliary array*/
    auxArray[n] = sb->dhead; n++;
    sb->dhead = currCluster.next;
    /*Update current cluster*/
    currCluster.next = NULL_CLUSTER;
    if((status = soWriteCacheCluster(currPhysical, &currCluster)) != 0)
      return status;
    /*Check general repository consistency*/
    if(sb->dhead == NULL_CLUSTER)
      sb->dtail = NULL_CLUSTER;
    /*Check if insertion cache needs to be depleted*/
    if((n != DZONE_CACHE_SIZE) && sb->dhead == NULL_CLUSTER)
      if((DZONE_CACHE_SIZE - n) < sb->dzone_free)
        if((status = soDeplete(sb)) != 0)
          return status;
  }

  /*Update Retrieval Cache*/
  while(n > 0)
    sb->dzone_retriev.cache[sb->dzone_retriev.cache_idx-- - 1] = auxArray[n-- - 1];

  /** Update superblock information **/
  if((status = soStoreSuperBlock()) != 0)
    return status;

  /** Free auxiliary array **/
  free(auxArray);

  /** Operation successful **/
  return 0;
}

static int soDeplete(SOSuperBlock *sb)
{
  /** Variables **/
  int status;
  uint32_t index;
  uint32_t tailPhysical;
  uint32_t insertPhysical;
  SODataClust tailCluster;
  SODataClust insertCluster;

  /** Parameter check **/
  if(sb == NULL)
    return -EINVAL;

  /** Insertion cache check **/
  if(sb->dzone_insert.cache_idx == 0)
    return 0; /*Empty cache is not an error*/

  /** Double-linked list of free data clusters check **/
  /*If linked list is empty, "deplete" first cluster from insertion cache*/
  index = 0;
  if(sb->dhead == NULL_CLUSTER)
  {
    sb->dhead = sb->dzone_insert.cache[index];
    sb->dtail = sb->dhead;
    index++;
  }

  /** Deplete insertion cache **/
  while(index < sb->dzone_insert.cache_idx)
  {
    /*Get physical numbers of the tail and insertion clusters*/
    tailPhysical = sb->dtail * BLOCKS_PER_CLUSTER + sb->dzone_start;
    insertPhysical = sb->dzone_insert.cache[index] * BLOCKS_PER_CLUSTER + sb->dzone_start;
    /*Read data clusters*/
    if((status = soReadCacheCluster(tailPhysical, &tailCluster)) != 0)
      return status;
    if((status = soReadCacheCluster(insertPhysical, &insertCluster)) != 0)
      return status;
    /*Update clusters information*/
    tailCluster.next = sb->dzone_insert.cache[index];
    insertCluster.prev = sb->dtail;
    insertCluster.next = NULL_CLUSTER; /*Unnecessary*/
    /*Write data clusters*/
    if((status = soWriteCacheCluster(tailPhysical, &tailCluster)) != 0)
      return status;
    if((status = soWriteCacheCluster(insertPhysical, &insertCluster)) != 0)
      return status;
    /*Update superblock*/
    sb->dtail = sb->dzone_insert.cache[index];
    /*Update index*/
    index++;
  }

  /** Write updated superblock information to disk **/
  sb->dzone_insert.cache_idx = 0;
  if((status = soStoreSuperBlock()) != 0)
    return status;

  /** Operation successful **/
  return 0;
}
