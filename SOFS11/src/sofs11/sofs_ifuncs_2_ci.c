/**
 *  \file sofs_ifuncs_2_ci.c (implementation file for function soCleanInode)
 *
 *  \brief Set of operations to manage inodes: level 2 of the internal file system organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li read specific inode data from the table of inodes
 *      \li write specific inode data to the table of inodes
 *      \li clean an inode
 *      \li check the inode access permissions against a given operation.
 *
 *  \author Artur Carneiro Pereira September 2008
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - September 2010 / September 2011
 *  \author Rui Neto       42520 T6G2 - October 2011
 *  \author Monica Feitosa 48613 T6G2 - October 2011
 *  \author Gilberto Matos 27657 T6G2 - October 2011
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
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"


/**
 *  \brief Clean an inode.
 *
 *  The inode must be free in the dirty state.
 *  The inode is supposed to be associated to a file, a directory, or a symbolic link which was previously deleted.
 *
 *  This function cleans the list of data cluster references.
 *
 *  Notice that the inode 0, supposed to belong to the file system root directory, can not be cleaned.
 *
 *  \param nInode number of the inode
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soCleanInode (uint32_t nInode)
{
  soProbe (313, "soCleanInode (%"PRIu32")\n", nInode);
  
  /** Variables **/
  int error;
  uint32_t DClustIdx;
  uint32_t IClustIdx;
  uint32_t physDClust;
  uint32_t physIClust;
  uint32_t nBlock;
  uint32_t nOffset;
  SOSuperBlock *sb;
  SODataClust *DClust;
  SODataClust *IClust;
  SOInode *Inode;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Conformity chech **/
  if((nInode == 0) || (nInode >= sb->itotal))
    return -EINVAL;

  /** Read inode **/
  if((error = soConvertRefInT(nInode, &nBlock, &nOffset)) != 0)
    return error;
  if((error = soLoadBlockInT(nBlock)) != 0)
    return error;
  if((Inode = soGetBlockInT()) == NULL)
    return -ELIBBAD;

  /** Check if inode is free in the dirty state **/
  if((error = soQCheckFDInode(sb, &Inode[nOffset])) != 0)
    return error;

  /** Clean inode **/

  /*Double Indirect References*/
  if(Inode[nOffset].i2 != NULL_CLUSTER)
  {
    /*Load double indirect references cluster*/
    physIClust = Inode[nOffset].i2 * BLOCKS_PER_CLUSTER + sb->dzone_start;
    if((error = soLoadSngIndRefClust(physIClust)) != 0)
      return error;
    if((IClust = soGetSngIndRefClust()) == NULL)
      return -ELIBBAD;
    /*Load each single indirect reference cluster, and update it*/
    for(IClustIdx = 0; IClustIdx < RPC; IClustIdx++)
    {
      if(IClust->info.ref[IClustIdx] != NULL_CLUSTER)
      {
        /*Load Indirect references cluster*/
        physDClust = IClust->info.ref[IClustIdx] * BLOCKS_PER_CLUSTER + sb->dzone_start;
        if((error = soLoadDirRefClust(physDClust)) != 0)
          return error;
        if((DClust = soGetDirRefClust()) == NULL)
          return -ELIBBAD;
        /*Clean Indirect references cluster*/
        for(DClustIdx = 0; DClustIdx < RPC; DClustIdx++)
          DClust->info.ref[DClustIdx] = NULL_CLUSTER;
        /*Store Indirect references cluster*/
        DClust->stat = NULL_INODE;
        if((error = soStoreDirRefClust()) != 0)
          return error;
        /*Free Indirect references cluster*/
        if((error = soFreeDataCluster(IClust->info.ref[IClustIdx])) != 0)
          return error;
        /*Clean Double Indirect reference*/
        IClust->info.ref[IClustIdx] = NULL_CLUSTER;
      }
    }
    /*Store double indirect references cluster*/
    IClust->stat = NULL_INODE;
    if((error = soStoreSngIndRefClust()) != 0)
      return error;
    /*Free Double Indirect reference cluster*/
    if((error = soFreeDataCluster(Inode[nOffset].i2)) != 0)
      return error;
    /*Update inode's double indirect references cluster*/
    Inode[nOffset].i2 = NULL_CLUSTER;
  }

  /*Single Indirect references*/
  if(Inode[nOffset].i1 != NULL_CLUSTER)
  {
    /*Load indirect references cluster*/
    physDClust = Inode[nOffset].i1 * BLOCKS_PER_CLUSTER + sb->dzone_start;
    if((error = soLoadDirRefClust(physDClust)) != 0)
      return error;
    if((DClust = soGetDirRefClust()) == NULL)
      return -ELIBBAD;
    /*Clean indirect references cluster*/
    for(DClustIdx = 0; DClustIdx < RPC; DClustIdx++)
      DClust->info.ref[DClustIdx] = NULL_CLUSTER;
    /*Store indirect references cluster*/
    DClust->stat = NULL_INODE;
    if((error = soStoreDirRefClust()) != 0)
      return error;
    /*Free indirect reference cluster*/
    if((error = soFreeDataCluster(Inode[nOffset].i1)) != 0)
      return error;
    /*Update inode's indirect cluster reference*/
    Inode[nOffset].i1 = NULL_CLUSTER;
  }

  /*Direct references*/
  for(DClustIdx = 0; DClustIdx < N_DIRECT; DClustIdx++)
    Inode[nOffset].d[DClustIdx] = NULL_CLUSTER;

  /*Update remaining inode fields - next and prev stay untouched*/
  Inode[nOffset].refcount = 0;
  Inode[nOffset].size = 0;
  Inode[nOffset].clucount = 0;

  /** Store Block that contains the update inode information **/
  if((error = soStoreBlockInT()) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
