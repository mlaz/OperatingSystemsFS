#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "fsck_sofs11.h"


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

  if (p_sb->name[cnt] != '\0')
    return -EVNAME;

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

  /* Checking the total number of inodes */
  if (p_sb->itotal < (p_sb->itable_size * IPB) )
    return -ESBITOTAL;

  /* Checking the total number of free inodes */
  if ( p_sb->ifree > (p_sb->itotal - 1) )
    return -ESBIFREE;

  /* Checking if the ihead value is in range */
  if (p_sb->ihead >= p_sb->itotal)
    return -EIBADHEAD;

  /* Checking if the itail value is in range */
  if (p_sb->itail >= p_sb->itotal)
    return -EIBADTAIL;

  return FSCKOK;
 }

 int fsckCheckDZoneMetaData (SOSuperBlock *p_sb, uint32_t ntotal)
{
  uint32_t iblktotal;   /* number of blocks of the inode table */
  uint32_t nclusttotal; /* total number of clusters */

  if (p_sb == NULL)
    return -EINVAL;

  /* printf("\t[ERROR]\n"); */
  /* fflush(stdout); */

  /* Checking the physical number of the block where the data zone starts */
  if ( p_sb->dzone_start != (p_sb->itable_start + p_sb->itable_size) )
    return -ESBDZSTART;

  //----------------------------------------------------------
  /* printf("\t[PHYS]\n"); */
  /* fflush(stdout); */

  /* printf("\t[dztotal = %d]\n", p_sb->dzone_total); */
  /* fflush(stdout); */

  /* printf("\t[nctotal = %d]\n", nclusttotal); */
  /* fflush(stdout); */
  //----------------------------------------------------------

  /*
   * Computing the number of dataclusters for the storage device
   *
   * nblocktotal = (dev_size / BLOCK_SIZE)
   * nclusttotal = nblocktotal / BPC
   **/

    //if (itotal == 0) itotal = ntotal >> 3;

  if ((p_sb->itotal % IPB) == 0)
    iblktotal = p_sb->itotal / IPB;
  else iblktotal = p_sb->itotal / IPB + 1;
  nclusttotal = (ntotal - 1 - iblktotal) / BLOCKS_PER_CLUSTER;
  /* final adjustment */
  iblktotal = ntotal - 1 - nclusttotal * BLOCKS_PER_CLUSTER;


  /* printf("\nDevice size (in clusters): %d.\n", nclusttotal); */


  /* Checking the total number of data clusters */
  if (p_sb->dzone_total != nclusttotal)
    return -ESBDZTOTAL;


  return FSCKOK;
}
