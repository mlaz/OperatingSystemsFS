#include "fsck_sofs11.h"

#include "sofs_const.h"
#include "sofs_superblock.h"

int fsckCheckSuperBlockHeader (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  int cnt = 0;

  /* Checking magic number */
  if (p_sb->magic != MAGIC_NUMBER)
    return -EMAGIC;

  /* Checking version number */
  if (p_sb->version != VERSION_NUMBER)
    return -EVERSION;

  /* Checking volume name string integriry */
  while ( (cnt <= PARTITION_NAME_SIZE) && (p_sb->name[cnt] != '\0') )
    cnt++;

  if ( cnt > (PARTITION_NAME_SIZE + 1) )
    return -ENAME;

  /* Checking mstat flag */
  if (p_sb->mstat != PRU)
    return -EMSTAT;

  return FSCKOK;
}

int fsckCheckSBInodeMetaData (SOSuperBlock *p_sb)
{
  if (p_sb == NULL)
    return -EINVAL;

  /* Checking if inode table starts at block 1 */
  if (p_sb->itable_start != 1)
    return -ESBISTART;

  /* Checking the inode table size */
  if (p_sb->itable_size != p_sb->itotal / IPB)
    return -ESBISIZE;

  /* /\* Checking the total number of inodes *\/ */
  /* if (p_sb->itotal <) */
  /*   return -ESBITOTAL; */

  /* Checking the total number of free inodes */
  if ( p_sb->ifree >= (p_sb->itotal - 1) )
    return -ESBIFREE;

  /* Checking if the ihead value is in range */
  if (p_sb->ihead >= p_sb->itotal)
    return -ESBIHEAD;

  /* Checking if the itail value is in range */
  if (p_sb->itail >= p_sb->itotal)
    return -ESBITAIL;

  return FSCKOK;
 }

int fsckCheckDZoneMetaData (SOSuperBlock *p_sb, uint32_t nclusttotal)
{
  if (p_sb == NULL)
    return -EINVAL;

  /* Checking the physical number of the block where the data zone starts */
  if ( p_sb->dzone_start != (p_sb->itable_start + p_sb->itable_size) )
    return -ESBDZSTART;

  /* Checking the total number of data clusters */
  if (p_sb->dzone_total != nclusttotal)
    return -ESBDZTOTAL;

  /* Checking the number of free data clusters - only 1 cluster in use by root directory */
  if (p_sb->dzone_free != nclusttotal - 1)
    return -ESBDZFREE;

  return FSCKOK;
}
