#include <errno.h>
#include <unistd.h>


#include "sofs_buffercache.h"
#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "fsck_sofs11.h"
#include "fsck11_stack.h"

static int checkDirRec (FSCKStack *stack, uint32_t dzone_start);

int fsckCheckDirectoryTree (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  int ret;
  FSCKStack *stack = newStack();
  stackIn(stack ,0);
  ret = checkDirRec(stack, p_sb->dzone_start);
  destroyStack(stack);
  return ret;
}

static int checkDirRec (FSCKStack *stack, uint32_t dzone_start)
{
  if (isEmpty(stack))
    return FSCKOK;

  uint32_t current_inode = stackOut(stack);
  uint32_t block_num;
  uint32_t inode_offset;
  uint32_t status;
  SOInode *inode_block;

  uint32_t logic_clt;
  uint32_t phys_clt;
  SODataClust current_clt;
  int cnt;

  /* Reading the next inode from inode table */
  if ( (status = soConvertRefInT(current_inode, &block_num, &inode_offset)) < 0 )
    return status;

  if ( (status = soLoadBlockInT(block_num)) < 0 )
    return status;
  inode_block = soGetBlockInT();

  if ((inode_block[inode_offset].mode & INODE_DIR) != INODE_DIR)
    return FSCKOK;

  /* this is a directory */
  //check if the .. points to its parent
  //check if . points to itself

  /* Getting the directory table */
  logic_clt = inode_block[inode_offset].d[0];
  // check is the clustea is "problem free", check this out on the table
  phys_clt = logic_clt * BLOCKS_PER_CLUSTER + dzone_start;

  if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
    return status;

  for (cnt = 2; cnt < DPC; cnt++)
    {
      if (current_clt.info.de[cnt].nInode != NULL_INODE)
        stackIn(stack, current_clt.info.de[cnt].nInode);
    }
  return checkDirRec (stack, dzone_start);
}
