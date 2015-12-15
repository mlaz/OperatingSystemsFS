#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "fsck_sofs11.h"

#include "sofs_buffercache.h"
#include "sofs_basicconsist.h"
#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_datacluster.h"


int fsckCheckCltCaches (SOSuperBlock *p_sb, uint8_t *clt_table)
{
  if (p_sb == NULL)
    return -EINVAL;

  uint32_t cnt;
  uint32_t logic_clt; //current cluster logical number
  uint32_t  phys_clt; //current cluster pyhsical number
  SODataClust current_clt;
  uint32_t status;

  /* Checking retrieval cache integrity */
  if (p_sb->dzone_retriev.cache_idx > DZONE_CACHE_SIZE)
    return -ERCACHEIDX;

  for (cnt = p_sb->dzone_retriev.cache_idx; cnt < DZONE_CACHE_SIZE; cnt++)
    {
      /* Checking cache reference integrity */
      if (p_sb->dzone_retriev.cache[cnt] == NULL_CLUSTER)
        return -ERCACHEIDX;

      /* Fetching the data cluster */
      logic_clt = p_sb->dzone_retriev.cache[cnt];
      phys_clt = logic_clt * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
        return status;

      /* Checking whether the cluster is on the general repository */
      if ( (current_clt.prev != NULL_CLUSTER) ||
           (current_clt.next != NULL_CLUSTER) )
        return -ERCACHEREF;

      /* Tagging the cluster as free */
      clt_table[logic_clt] |= CLT_FREE;

      /* Checking whether the cluster is clean */
      /* if (current_clt.stat != NULL_INODE) */
      /*   return -ERCLTREF; */

    }

  /* Checking insertion cache integrity */
  if (p_sb->dzone_insert.cache_idx > DZONE_CACHE_SIZE)
    return -EICACHEIDX;

  for (cnt = 0; cnt < p_sb->dzone_insert.cache_idx; cnt++)
    {
      /* Checking cache reference integrity */
      if (p_sb->dzone_insert.cache[cnt] == NULL_CLUSTER)
        return -EICACHEIDX;

      /* Fetching the data cluster */
      logic_clt = p_sb->dzone_insert.cache[cnt];
      phys_clt = logic_clt * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
        return status;

      /* Checking whether the cluster is free */
      if ( (current_clt.prev != NULL_CLUSTER) ||
           (current_clt.next != NULL_CLUSTER) )
        return -EICACHEREF;

      /* Tagging the cluster as free */
      clt_table[logic_clt] |= CLT_FREE;
    }

  return FSCKOK;
}

int fsckCheckDataZone (SOSuperBlock *p_sb, uint8_t *clt_table)
{

  if (p_sb == NULL)
    return -EINVAL;

  if (clt_table == NULL)
    return -EINVAL;

  SODataClust current_clt;
  uint32_t logic_clt; //current cluster logical number
  uint32_t phys_clt;  //current cluster pyhsical number
  uint32_t tail_found = 0; //boolean: true = 1; false = 0;
  uint32_t head_found = 0; //boolean: true = 1; false = 0;
  /* uint32_t retriev_count = 0; //clusters supposed to belong in the retrieval cache */
  uint32_t llist_count = 0;
  uint32_t status;


  for (logic_clt = 0; logic_clt < p_sb->dzone_total; logic_clt++)
    {
      /* Fetching the data cluster */
      phys_clt = logic_clt * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
        return status;


      /* Checking whether the cluster is clean */
      if ( (current_clt.prev != NULL_CLUSTER) ||
           (current_clt.next != NULL_CLUSTER) )
        {
          //this cluster is supposed to belong in the linked list
          clt_table[logic_clt] |= CLT_FREE;
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
          else if (current_clt.prev > p_sb->dzone_total + 1)
            {
              return -EDZLLBADREF;
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
          else if (current_clt.next > p_sb->dzone_total + 1)
            {
              return -EDZLLBADREF;
            }
        }

      if (current_clt.stat == NULL_INODE)
        {
          clt_table[logic_clt] |= CLT_CLEAN;
        }
      /* printf("logic_clt:%d\n", logic_clt); */
      /* printf("clt:%d\n", clt_table[logic_clt]); */

   }

  fflush(stdout);

  /* TODO I could do better, I could check if the clt exists on the caches */
  /* Checking if the retrieval cache comprises the same */
  /* number counted free clean data clusters */
  /* printf ("retriev_count:%d", retriev_count); */
  /* if (DZONE_CACHE_SIZE - p_sb->dzone_retriev.cache_idx != retriev_count) */
  /*   return -ERMISSCLT; */

  fflush(stdout);

  /* if ( p_sb->dzone_free != (retriev_count + llist_count + p_sb->dzone_insert.cache_idx) ) */
  /*   return -EFREECLT; */

  fflush(stdout);
  return FSCKOK;
}

int fsckCheckCltLList (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  uint32_t prev_cluster; //the next cluster logical number to be processed
  uint32_t next_cluster; //the next cluster logical number to be processed
  uint32_t phys_clt;
  SODataClust current_cluster;
  uint32_t count = 0;
  uint32_t status;

  next_cluster = p_sb->dhead;
  prev_cluster = NULL_CLUSTER;
  /* printf("DHEAD: %u\n", p_sb->dhead);         */

  while (next_cluster != NULL_CLUSTER)
    {
      /* Fetching the prev cluster */
      phys_clt = next_cluster * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
      if ( (status = soReadCacheCluster(phys_clt, &current_cluster)) != 0 )
        return status;

      count++;

      if (count > p_sb->ifree)
        return -EDZLLLOOP; //may we have a loop here?

      /* Checking for prev reference integrity */
      if (current_cluster.prev != prev_cluster)
          return -EDZLLBROKEN;

      prev_cluster = next_cluster;
      next_cluster = current_cluster.next;
    }
  /* printf("count: %u\n", count); */
  return FSCKOK;
}

int fsckCheckInodeClusters (SOSuperBlock *p_sb, uint8_t *clt_table, uint8_t *inode_table)
{
  if (p_sb == NULL)
    return -EINVAL;

  SOInode *inode_block; //the block being processed
  uint32_t curr_block = 0; //current blocknumber with in the inode table
  uint32_t curr_inode; //current inode within the block

  uint32_t logic_clt; //logical numer of the cluster being processed
  SODataClust *current_clt_i1;
  SODataClust *current_clt_i2;
  uint32_t phys_clt;
  uint32_t idx;
  uint32_t idx_i2;
  uint32_t status;
  uint32_t inode_num = 0;

  while (curr_block < p_sb->itable_size)
    {
      /* Loading block */
      status = soLoadBlockInT(curr_block);
      if (status)
        return status;

      inode_block = soGetBlockInT();
      /* Processing inodes for the current block */
      for (curr_inode = 0; curr_inode < IPB; curr_inode++)
        {

          /* Checking direct data cluster references */
          for (idx = 0; idx < N_DIRECT; idx++)
            {
              logic_clt = inode_block[curr_inode].d[idx];
              if (logic_clt == NULL_CLUSTER)
                continue;

              /* Checking whether the cluster was already referenced */
              if (clt_table[logic_clt] & CLT_REF){
                //printf("\n\tERROR: inode:%d is referencing already
                //referenced data cluster %d.\n", (curr_block * IPB)
                //+curr_inode, logic_clt);
                inode_num = (curr_block * IPB) + curr_inode;
                inode_table[inode_num] |= INOD_DOUB_REF;
                clt_table[logic_clt] |= CLT_REF_ERR;
              }
              else
                clt_table[logic_clt] |= CLT_REF;
            }

          /* Checking simple indirect data cluster references */
          if (inode_block[curr_inode].i1 != NULL_CLUSTER)
            {

              logic_clt = inode_block[curr_inode].i1;
              /* Checking whether the i1 cluster was already referenced */
              if (clt_table[logic_clt] & CLT_REF){
                printf("2curr_inode:%d\n", (curr_block * IPB) +curr_inode);
                clt_table[logic_clt] |= CLT_REF_ERR;
              }
              else
                clt_table[logic_clt] |= CLT_REF;

              phys_clt = logic_clt * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
              if ( (status = soReadCacheCluster(phys_clt, &current_clt_i1)) != 0 )
                return status;

              for (idx = 0; idx < RPC; idx++)
                {
                  logic_clt = current_clt_i1->info.ref[idx];

                  if (logic_clt == NULL_CLUSTER)
                    continue;

                  /* Checking whether the cluster was already referenced */
                  if (clt_table[logic_clt] & CLT_REF)
                    clt_table[logic_clt] |= CLT_REF_ERR;
                  else
                    clt_table[logic_clt] |= CLT_REF;
                }
            }


          /* Checking double indirect data cluster references */
          if (inode_block[curr_inode].i2 != NULL_CLUSTER)
            {

              logic_clt = inode_block[curr_inode].i2;
              /* Checking whether the i2 cluster was already referenced */
              if (clt_table[logic_clt] & CLT_REF)
                clt_table[logic_clt] |= CLT_REF_ERR;
              else
                clt_table[logic_clt] |= CLT_REF;

              phys_clt = logic_clt * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
              if ( (status = soReadCacheCluster(phys_clt, &current_clt_i2)) != 0 )
                return status;

              for (idx = 0; idx < RPC; idx++)
                {
                  logic_clt = current_clt_i2->info.ref[idx];
                  if (logic_clt == NULL_CLUSTER)
                    continue;

                  /* Checking whether the cluster was already referenced */
                  if (clt_table[logic_clt] & CLT_REF)
                    clt_table[logic_clt] |= CLT_REF_ERR;
                  else
                    clt_table[logic_clt] |= CLT_REF;

                  phys_clt = logic_clt * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
                  if ( (status = soReadCacheCluster(phys_clt, &current_clt_i1)) != 0 )
                    return status;

                  for (idx_i2 = 0; idx_i2 < RPC; idx_i2++)
                    {
                      logic_clt = current_clt_i1->info.ref[idx_i2];
                      if (logic_clt == NULL_CLUSTER)
                        continue;

                      /* Checking whether the cluster was already referenced */
                      if (clt_table[logic_clt] & CLT_REF)
                        clt_table[logic_clt] |= CLT_REF_ERR;
                      else
                        clt_table[logic_clt] |= CLT_REF;
                    }
                }
            }
        }
      curr_block++;
    }

  return FSCKOK;
}
