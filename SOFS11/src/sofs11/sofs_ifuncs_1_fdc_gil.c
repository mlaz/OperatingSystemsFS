/**
 *  \file sofs_ifuncs_1_fdb.c (implementation file of function soFreeDataCluster)
 *
 *  \author Artur Carneiro Pereira - September 2008 / September 2011
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - September 2010 / September 2011
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
int soFreeDataCluster_bin (uint32_t nClust);

static int soDeplete (SOSuperBlock *p_sb);

/**
 *  \brief Free the referenced data cluster.
 *
 *  The cluster is inserted into the insertion cache of free data cluster references. If the cache is full, it has to be
 *  depleted before the insertion may take place. The data cluster should be put in the dirty state (the <tt>stat</tt>
 *  of the header should remain as it is), the other fields of the header, <tt>prev</tt> and <tt>next</tt>, should be
 *  put to NULL_CLUSTER. The only consistency check to carry out at this stage is to check if the data cluster was
 *  allocated.
 *
 *  Notice that the first data cluster, supposed to belong to the file system root directory, can never be freed.
 *
 *  \param nClust logical number of the data cluster
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, the <em>data cluster number</em> is out of range or the data cluster is not allocated
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */


int soFreeDataCluster (uint32_t nClust)
{
  soProbe (414, "soFreeDataCluster(%"PRIu32")\n", nClust);
  
  /** Variables **/
  int status;
  uint32_t physCluster;
  uint32_t stat;
  SODataClust freeCluster;
  SOSuperBlock *sb; 

  /** Loading SuperBlock **/
  if((status = soLoadSuperBlock()) < 0)
    return status;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Parameter validation **/
  if((nClust <= 0) || (nClust >= sb->dzone_total))
    return -EINVAL;

  /** Check if inode is allocated **/
  if((status = soQCheckStatDC(sb, nClust, &stat)) < 0)
    return status;
  if(stat != ALLOC_CLT)
    return -EINVAL;

  /** Check if insertion cache needs to be depleted **/
  if(sb->dzone_insert.cache_idx == DZONE_CACHE_SIZE)
    if((status = soDeplete(sb)) < 0)
      return status;
  
  /** Free Cluster **/
  physCluster = nClust * BLOCKS_PER_CLUSTER + sb->dzone_start;
  if((status = soReadCacheCluster(physCluster, &freeCluster)) < 0)
    return status;
  freeCluster.prev = NULL_CLUSTER;
  freeCluster.next = NULL_CLUSTER;
  if((status = soWriteCacheCluster(physCluster, &freeCluster)) < 0)
    return status;

  /** Update Superblock **/
  sb->dzone_insert.cache[sb->dzone_insert.cache_idx] = nClust;
  sb->dzone_insert.cache_idx++;
  sb->dzone_free++;
  if((status = soStoreSuperBlock()) < 0)
    return status;

  /** Operation successful **/
  return 0;
}

static int soDeplete(SOSuperBlock *sb){

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
    return 0; /*TODO MODIFY? EMPTY CACHE IS AN ERROR?*/

  /** Double-linked list of free data clusters check **/
  /*If linked list is empty, "deplete" first cluster from insertion cache*/
  index = 0;
  if(sb->dhead == NULL_CLUSTER){
    sb->dhead = sb->dzone_insert.cache[index];
    sb->dtail = sb->dhead;
    index++;
  }

  /** Deplete insertion cache **/
  while(index < sb->dzone_insert.cache_idx){
    /*Get physical numbers of the tail and insertion clusters*/
    tailPhysical = sb->dtail * BLOCKS_PER_CLUSTER + sb->dzone_start;
    insertPhysical = sb->dzone_insert.cache[index] * BLOCKS_PER_CLUSTER + sb->dzone_start;
    /*Read data clusters*/
    if((status = soReadCacheCluster(tailPhysical, &tailCluster)) < 0)
      return status;
    if((status = soReadCacheCluster(insertPhysical, &insertCluster)) < 0)
      return status;
    /*Update clusters information*/
    tailCluster.next = sb->dzone_insert.cache[index];
    insertCluster.prev = sb->dtail;
    insertCluster.next = NULL_CLUSTER; /*Unnecessary*/
    /*Write data clusters*/
    if((status = soWriteCacheCluster(tailPhysical, &tailCluster)) < 0)
      return status;
    if((status = soWriteCacheCluster(insertPhysical, &insertCluster)) < 0)
      return status;
    /*Update superblock*/
    sb->dtail = sb->dzone_insert.cache[index];
    /*Update index*/
    index++;
  }

  /** Write updated superblock information to disk **/
  sb->dzone_insert.cache_idx = 0;
  if((status = soStoreSuperBlock()) < 0)
    return status;

  /** Operation successful **/
  return 0;
}
