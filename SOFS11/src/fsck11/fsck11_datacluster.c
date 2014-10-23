#include "fsck_sofs11.h"

#include "sofs_buffercache.h"
#include "sofs_basicconsist.h"
#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_datacluster.h"


int fsckCltCheckCaches (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  uint32_t cnt;
  uint32_t logic_clt; //current cluster logical number
  uint32_t  phys_clt; //current cluster pyhsical number
  SODataClust current_clt;

  /* Checking retrieval cache integrity */
  if (p_sb->dzone_retriev.cache_idx < DZONE_CACHE_SIZE)
    return -ERCACHEIDX;

  for (cnt = sb->dzone_retriev.cache_idx; cnt < DZONE_CACHE_SIZE; cnt++)
    {
      /* Checking cache reference integrity */
      if (p_sb->dzone_retriev.cache[cnt] == NULL_CLUSTER)
        return -ERCACHEIDX;

      /* Fetching the data cluster */
      logic_clt = sb->dzone_retriev.cache[cnt];
      phys_clt = logic_clt * BLOCKS_PER_CLUSTER + sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
        return status;

      /* Checking whether the cluster is free */
      if ( (current_clt.prev != NULL_CLUSTER) ||
           (current_clt.next != NULL_CLUSTER) )
        return -ERCACHEREF;

      /* Checking whether the cluster is clean */
      if (current_clt.stat != NULL_INODE)
        return -ERCLTREF;

    }

  /* Checking insertion cache integrity */
  if (p_sb->dzone_insert.cache_idx < DZONE_CACHE_SIZE)
    return -EICACHEIDX;

  for (cnt = 0; cnt < sb->dzone_insert.cache_idx; cnt++)
    {
      /* Checking cache reference integrity */
      if (p_sb->dzone_insert.cache[cnt] == NULL_CLUSTER)
        return -EICACHEIDX;

      /* Fetching the data cluster */
      logic_clt = sb->dzone_insert.cache[cnt];
      phys_clt = logic_clt * BLOCKS_PER_CLUSTER + sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
        return status;

      /* Checking whether the cluster is free */
      if ( (current_clt.prev != NULL_CLUSTER) ||
           (current_clt.next != NULL_CLUSTER) )
        return -EICACHEREF;
    }

  return FSCKOK;
}

int fsckCheckDataZone (SOSuperBlock *p_sb)
{

  if (p_sb == NULL)
    return -EINVAL;

  SODataClust current_clt;
  uint32_t logic_clt; //current cluster logical number
  uint32_t  phys_clt; //current cluster pyhsical number
  uint32_t tail_found = 0; //boolean: true = 1; false = 0;
  uint32_t head_found = 0; //boolean: true = 1; false = 0;
  uint32_t retriev_count = 0; //clusters supposed to belong in the retrieval cache
  uint32_t status;


  for (logic_clt = 0; logic_clt < p_sb->dzone_total; logic_clt++)
    {
      /* Fetching the data cluster */
      phys_clt = logic_clt * BLOCKS_PER_CLUSTER + sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
        return status;

      /* Checking whether the cluster is free */
      if ( (current_clt.prev == NULL_CLUSTER) ||
           (current_clt.next == NULL_CLUSTER) )
        {
          /* Checking whether the cluster is clean */
          if (current_clt.stat == NULL_INODE)
              retriev_count++; //this cluster is supposed to belong in the retrieval cache
        }

      else
        {
          //this cluster is supposed to belong in the linked list
          llist_count++;
          if (current_clt.prev == NULL_CLUSTER)
            {
              /* Checking if it is the head of the list */
              if (head_found)
                return -EDZBADHEAD;

              if (p_sb->dhead != logic_clt)
                return -EDZBADHEAD;

              head_found = 1;
            }
          /* Checking if is within its range */
          else if (current_clt.prev > dzone_total + 1)
            {
              return -EDZLLBADREF
            }

          if (current_clt.next == NULL_CLUSTER)
            {
              /* Checking if it is the tail of the list */
              if (tail_found)
                return -EDZBADTAIL;

              if (p_sb->dtail != logic_clt)
                return -EDZBADTAIL;

              tail_found = 1;
            }
          /* Checking if is within its range */
          else if (current_clt.next > dzone_total + 1)
            {
              return -EDZLLBADREF
            }
        }
    }

  //TODO I could do better, I could check if the clt exists on the caches
  /*
   * Checking if the retrieval cache comprises the same
   * number counted free clean data clusters
   **/
  if (p_sb->dzone_retriev.cache_idx != rertriev_count)
    return -ERMISSCLT;

  if ( p_sb->dzone_free != (retriev_count + llistcount + dzone_insert.cache_idx - DZONE_CACHE_SIZE) )
    return -EFREECLT;

  return FSCKOK;
}

int fsckCheckCltLList (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  uint32_t prev_cluster; //the next cluster logical number to be processed
  uint32_t next_cluster; //the next cluster logical number to be processed
  uint32_t phys_clt;
  SODataClust *current_cluster;
  uint32_t count = 0;

  prev_cluster = NULL_CLUSTER;
  next_cluster = p_sb->dhead;

  do
    {
      /* Fetching the next cluster */
      phys_clt = next_cluster * BLOCKS_PER_CLUSTER + sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_cluster)) != 0 )
        return status;

      count++;
      if (count > p_sb->ifree)
        return -EDZLLLOOP; //may we have a loop here?

      /* Checking for next reference integrity */
      if (current_cluster->prev != prev_cluster)
        return -EEDZLLBROKEN;

      prev_cluster = next_cluster;
      next_cluster = current_cluster->next;
    }
  while (next_cluster != NULL_CLUSTER);

  return FSCKOK;
}
