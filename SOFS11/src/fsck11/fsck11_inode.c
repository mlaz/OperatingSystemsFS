#include <errno.h>
#include <unistd.h>


#include "sofs_buffercache.h"
#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "fsck_sofs11.h"

int fsckCheckInodeTable (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  SOInode *inode_block; //the block being processed
  uint32_t curr_block = 0; //current blocknumber within the inode table
  uint32_t curr_inode; //current inode within the block
  uint32_t tail_found = 0; //boolean: true = 1; false = 0;
  uint32_t head_found = 0; //boolean: true = 1; false = 0;
  uint32_t freecount = 0; //free (clean + dirty) inode count
  uint32_t status;

  while (curr_block < p_sb->itable_size)
    {
      /* Loading block */
      status = soLoadBlockInT(curr_block);
      if (status)
        return status;

      inode_block = soGetBlockInT();

      /* Processing inodes for the current block */
      for (curr_inode = 0; curr_inode <= IPB; curr_inode++)
        {
          /* Checking if the inode is free */
          if (inode_block[curr_inode].mode & INODE_FREE)
            {
              freecount++;

              /* Checking vD2.prev reference intgrity */
              if (inode_block[curr_inode].vD2.prev == NULL_INODE)
                {
                  /* Checking if it is the head of the list */
                  if (head_found)
                    return -EIBADHEAD;

                  if ( p_sb->ihead != ((IPB * curr_block) + curr_inode) )
                    return -EIBADHEAD;

                  head_found = 1;
                }
              /* Checking if is within its range */
              else if ( inode_block[curr_inode].vD2.prev > (p_sb->itotal + 1) )
                {
                  return -EIBADINODEREF;
                }

              /* Checking vD1.next reference intgrity */
              if (inode_block[curr_inode].vD1.next == NULL_INODE)
                {
                  /* Checking if it is the tail of the list */
                  if (tail_found)
                    return -EIBADTAIL;

                  if ( p_sb->itail != ((IPB * curr_block) + curr_inode) )
                    return -EIBADTAIL;

                  tail_found = 1;
                }
              /* Checking if is within its range */
              else if ( inode_block[curr_inode].vD1.next > (p_sb->itotal + 1) )
                {
                  return -EIBADINODEREF;
                }

            }
        }
      curr_block++;
    }


  if (p_sb->ifree != freecount)
    return -EBADFREECOUNT;

  return FSCKOK;
}

int fsckCheckInodeList (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  uint32_t prev_inode; //the next inode number to be processed
  uint32_t next_inode; //the next inode number to be processed
  uint32_t block_num;
  uint32_t inode_offset;
  uint32_t status;
  SOInode *inode_block;
  uint32_t count = 0;

  prev_inode = NULL_INODE;
  next_inode = p_sb->ihead;

  do
    {
      /* Reading the next inode from inode table */
      if ( (status = soConvertRefInT(next_inode, &block_num, &inode_offset)) < 0 )
        return status;

      if ( (status = soLoadBlockInT(block_num)) < 0 )
        return status;
      inode_block = soGetBlockInT();

      /* Checking if the inode is free */
      if ( (inode_block[inode_offset].mode & INODE_FREE) == 0 )
        return -EILLNOTFREE;

      count++;
      if (count > p_sb->ifree)
        return -EILLLOOP; //may we have a loop here?

      /* Checking for vD2.prev integrity */
      if (inode_block[inode_offset].vD2.prev != prev_inode)
        return -EILLBROKEN;

      prev_inode = next_inode;
      next_inode = inode_block[inode_offset].vD1.next;
    }
  while (next_inode != NULL_INODE);

  if (p_sb->ifree != count)
    return -EBADFREECOUNT;

  return FSCKOK;
}
