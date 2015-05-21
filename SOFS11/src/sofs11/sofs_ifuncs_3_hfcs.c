/**
 *  \file sofs_ifuncs_3_hfcs (implementation file for function soHandleFileClusters)
 *
 *  \brief Set of operations to manage data clusters: level 3 of the internal file system organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li read a specific data cluster
 *      \li write to a specific data cluster
 *      \li handle a file data cluster
 *      \li free and clean all data clusters from the list of references starting at a given point.
 *
 *  \author Artur Carneiro Pereira September 2008
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - September 2010 / September 2011
 *  \author Rui Neto       42520 T6G2 - October 2011
 *  \author Gilberto Matos 27657 T6G2 - October 2011
 *
 *  \remarks In case an error occurs, all functions return a negative value which is the symmetric of the system error
 *           or the local error that better represents the error cause. Local errors are out of the range of the
 *           system errors.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"

/* Allusion to internal functions */
int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal);


/**
 *  \brief Handle all data clusters from the list of references starting at a given point.
 *
 *  The file (a regular file, a directory or a symlink) is described by the inode it is associated to.
 *
 *  Several operations are available and can be applied to the file data clusters starting from the index to the list of
 *  direct references which is given.
 *
 *  The list of valid operations is
 *
 *    \li FREE:       free all data clusters starting from the referenced data cluster
 *    \li FREE_CLEAN: free all data clusters starting from the referenced data cluster and dissociate them from the
 *                    inode which describes the file
 *    \li CLEAN:      dissociate all data clusters starting from the referenced data cluster from the inode which
 *                    describes the file.
 *
 *  Depending on the operation, the field <em>clucount</em> and the lists of direct references, single indirect
 *  references and double indirect references to data clusters of the inode associated to the file are updated.
 *
 *  Thus, the inode must be in use and belong to one of the legal file types for the operations FREE and FREE_CLEAN and
 *  must be free in the dirty state for the operation CLEAN.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustIndIn index to the list of direct references belonging to the inode which is referred (it contains the
 *                    index of the first data cluster to be processed)
 *  \param op operation to be performed (FREE, FREE AND CLEAN, CLEAN)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the requested operation is invalid
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
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

int soHandleFileClusters (uint32_t nInode, uint32_t clustIndIn, uint32_t op)
{
  soProbe (213, "soHandleFileClusters (%"PRIu32", %"PRIu32", %"PRIu32")\n", nInode, clustIndIn, op);

  /** Variables **/
  int error;
  uint32_t clustIdx;
  uint32_t IClustIdx;
  uint32_t DClustIdx;
  uint32_t i1;
  uint32_t i2;
  SOSuperBlock *sb;
  SOInode Inode;
  SODataClust *DClust;
  SODataClust *IClust;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Parameter check **/
  if((nInode >= sb->itotal) || (clustIndIn >= MAX_FILE_CLUSTERS))
    return -EINVAL;
  switch(op)
  {
    case FREE:
    case FREE_CLEAN:
    case CLEAN:
      break;
    default:
      return -EINVAL;
  }

  /** Read inode **/
  if(op == CLEAN)
  {
    if((error = soReadInode(&Inode, nInode, FDIN)) != 0)
      return error;
  } 
  else
  {
    if((error = soReadInode(&Inode, nInode, IUIN)) != 0)
      return error;
  }

  i1 = Inode.i1;
  i2 = Inode.i2;

  /** Handle clusters **/

  /*Double indirect references*/
  /*Obtain index for double indirect zone*/
  if(clustIndIn <= N_DIRECT + RPC)
    clustIdx = N_DIRECT + RPC;
  else
    clustIdx = clustIndIn;
  /*Handle double indirect zone*/
  if(i2 != NULL_CLUSTER)
  {
    while((clustIdx < MAX_FILE_CLUSTERS) && (Inode.i2 != NULL_CLUSTER))
    {
      IClustIdx = (clustIdx - N_DIRECT - RPC) / RPC;
      DClustIdx = (clustIdx - N_DIRECT - RPC) % RPC;
      /*Read indirect references cluster*/
      if((error = soLoadSngIndRefClust(i2 * BLOCKS_PER_CLUSTER + sb->dzone_start)) != 0)
        return error;
      if((IClust = soGetSngIndRefClust()) == NULL)
        return -ELIBBAD;
      /*Read direct references cluster*/
      if(IClust->info.ref[IClustIdx] != NULL_CLUSTER)
      {
        if((error = soLoadDirRefClust(IClust->info.ref[IClustIdx] * BLOCKS_PER_CLUSTER + sb->dzone_start)) != 0)
          return error;
        if((DClust = soGetDirRefClust()) == NULL)
          return -ELIBBAD;
        if(DClust->info.ref[DClustIdx] != NULL_CLUSTER)
        {
          if((error = soHandleFileCluster(nInode, clustIdx, op, NULL)) != 0)
            return error;
        }
      }

      clustIdx += 1;
    }
  }

  /*Obtain index for single indirect zone*/
  if(clustIndIn <= N_DIRECT)
    clustIdx = N_DIRECT;
  else
    clustIdx = clustIndIn;
  /*Simple indirect references*/
  if(i1 != NULL_CLUSTER)
  {
    while((clustIdx < (N_DIRECT + RPC)) && (Inode.i1 != NULL_CLUSTER))
    {
      /*Read Indirect references cluster - needs to be read every time
         because HandleFileCluster may change cluster information*/
      DClustIdx = clustIdx - N_DIRECT;
      if((error = soLoadDirRefClust(i1 * BLOCKS_PER_CLUSTER + sb->dzone_start)) != 0)
        return error;
      if((DClust = soGetDirRefClust()) == NULL)
        return -ELIBBAD;
      /*Check if current reference exists, and handle it if so*/
      if(DClust->info.ref[DClustIdx] != NULL_CLUSTER){
        if((error = soHandleFileCluster(nInode, clustIdx, op, NULL)) != 0)
          return error;
      }
      clustIdx += 1; /*update reference index*/
    }
  }

  /*Obtain index for direct zone*/
  clustIdx = clustIndIn;

  /*Direct references cluster*/
  while(clustIdx < N_DIRECT)
  {
    if(Inode.d[clustIdx] != NULL_CLUSTER)
    {
      if((error = soHandleFileCluster(nInode, clustIdx, op, NULL)) != 0)
        return error;
    }
    clustIdx += 1; /*update reference index*/
  }


  /** Operation successful **/
  return 0;
}

