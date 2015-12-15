#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "sofs_buffercache.h"
#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "fsck_sofs11.h"
#include "fsck11_stack.h"

static int checkDirRec (FSCKStack *stack, uint32_t dzone_start, uint8_t *inode_tbl);

int fsckCheckDirTree (SOSuperBlock *p_sb, uint8_t *inode_tbl)
{
  if (p_sb == NULL)
    return -EINVAL;

  int ret;
  FSCKStack *stack = newStack();
git   stackIn(stack, 0, 0);
  ret = checkDirRec(stack, p_sb->dzone_start, inode_tbl);
  destroyStack(stack);
  return ret;
}

static int checkDirRec (FSCKStack *stack, uint32_t dzone_start, uint8_t *inode_tbl)
{
  if (isEmpty(stack))
    return FSCKOK;

  uint32_t parent = getNextParent(stack);
  uint32_t current_inode = stackOut(stack);
  uint32_t block_num;
  uint32_t inode_offset;
  uint32_t status;
  SOInode *inode_block;

  uint32_t logic_clt;
  uint32_t phys_clt;
  SODataClust current_clt;
  int cnt;


  /* printf("current_inode:%d\n", current_inode); */
  /* printf("parent_inode:%d\n", parent); */

  /* Reading the next inode from inode table */
  if ( (status = soConvertRefInT(current_inode, &block_num, &inode_offset)) < 0 )
    return status;

  if ( (status = soLoadBlockInT(block_num)) < 0 )
    return status;
  inode_block = soGetBlockInT();

  /* Checking whether it is a directory or not */
  if ((inode_block[inode_offset].mode & INODE_DIR) != INODE_DIR)
    goto end;

  /* Checking whether this directory has already been visited*/
  if (inode_tbl[current_inode] != INOD_UNCHECK)
    {
      inode_tbl[current_inode] |= INOD_CHECK;
      inode_tbl[current_inode] |= INOD_LOOP;
      return -EDIRLOOP;
    }
  inode_tbl[current_inode] |= INOD_CHECK;

  /* Getting it's dir entrytable */
  logic_clt = inode_block[inode_offset].d[0];

  // check whether the cluster is "problem free", check this out on the table
  phys_clt = logic_clt * BLOCKS_PER_CLUSTER + dzone_start;
  /* printf("Phys:%d\n", phys_clt); */

  /* Fetching the datacluster */
  if ( (status = soReadCacheCluster(phys_clt, &current_clt)) != 0 )
    return status;

  /* Checking if "." entru points to itself */
  if (current_clt.info.de[0].nInode != current_inode) {
    /* printf("curr-ninode:%d\n", current_clt.info.de[0].nInode); */
    /* printf("parent:%d\n", current_clt.info.de[1].nInode); */
    /* printf("curr_inode:%d\n", current_inode); */
    inode_tbl[current_inode] |= INOD_REF_ERR;
  }

  /* Checking if the ".." entry points to its parent */
  if (current_clt.info.de[1].nInode != parent)
    inode_tbl[current_inode] |= INOD_PARENT_ERR;

  for (cnt = 2; cnt < DPC; cnt++)
    {
      if ((current_clt.info.de[cnt].nInode != NULL_INODE)
          && (current_clt.info.de[cnt].name[0] != "\0")) {
        stackIn(stack, current_clt.info.de[cnt].nInode, current_inode);
        /* printf("inode:%d\n", current_clt.info.de[cnt].nInode); */
        /* printf("name: %s\n", current_clt.info.de[cnt].name); */
      }
    }

 end:
  return checkDirRec (stack, dzone_start, inode_tbl);
}
