/**
 *  \file sofs_ifuncs_3_hfc (implementation file for function soHandleFileCluster)
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
 *  \author Gilberto Matos 27657 T6G2 - October 2011
 *  \author Miguel Azevedo 38569 T6G2 - October 2011
 *
 *  \remarks In case an error occurs, all functions return a negative value which is the symmetric of the system error
 *           or the local error that better represents the error cause. Local errors are out of the range of the
 *           system errors.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

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

static int soHandleDirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t nClust, uint32_t op,
                           uint32_t *p_outVal);
static int soHandleSIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t nClust, uint32_t op,
                              uint32_t *p_outVal);
static int soHandleDIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t nClust, uint32_t op,
                              uint32_t *p_outVal);
static int soCleanLogicalCluster (SOSuperBlock *p_sb, uint32_t nInode, uint32_t nLClust);


/**
 *  \brief Handle of a file data cluster.
 *
 *  The file (a regular file, a directory or a symlink) is described by the inode it is associated to.
 *
 *  Several operations are available and can be applied to the file data cluster whose logical number is given.
 *
 *  The list of valid operations is
 *
 *    \li GET:        get the physical number of the referenced data cluster
 *    \li ALLOC:      allocate a new data cluster and associate it to the inode which describes the file
 *    \li FREE:       free the referenced data cluster
 *    \li FREE_CLEAN: free the referenced data cluster and dissociate it from the inode which describes the file
 *    \li CLEAN:      dissociate the referenced data cluster from the inode which describes the file.
 *
 *  Depending on the operation, the field <em>clucount</em> and the lists of direct references, single indirect
 *  references and double indirect references to data clusters of the inode associated to the file are updated.
 *
 *  Thus, the inode must be in use and belong to one of the legal file types for the operations GET, ALLOC, FREE and
 *  FREE_CLEAN and must be free in the dirty state for the operation CLEAN.
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE / FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the requested operation is invalid or the <em>pointer to outVal</em> is \c NULL when it
 *                      should not be (GET / ALLOC)
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal)
{
  soProbe (213, "soHandleFileCluster (%"PRIu32", %"PRIu32", %"PRIu32", %p)\n",
                nInode, clustInd, op, p_outVal);

  /** Variables **/
  int error;
  SOInode inode;
  SOSuperBlock *sb;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Conformity check **/
  if(nInode >= sb->itotal || clustInd >= MAX_FILE_CLUSTERS)
    return -EINVAL;
  /*Operation check*/
  switch(op)
  {
    case GET:
    case ALLOC:
      if(p_outVal == NULL)
        return -EINVAL;
      break;
    case FREE:
    case FREE_CLEAN:
    case CLEAN:
      break;
    default:
      return -EINVAL;
  }

  /** Consistency check **/
  /*Read inode*/
  if(op == CLEAN)
  {
    if((error = soReadInode(&inode, nInode, FDIN)) != 0)
      return error;
  }
  else
  {
    if((error = soReadInode(&inode, nInode, IUIN)) != 0)
      return error;
  }

  /** Check reference zone **/
  /*Direct references zone*/
  if(clustInd < N_DIRECT)
  {
    if((error = soHandleDirect(sb, nInode, &inode, clustInd, op, p_outVal)) != 0)
      return error;
  }
  /*Single indirect references zone*/ 
  else if(clustInd < N_DIRECT + RPC)
  {
    if((error = soHandleSIndirect(sb, nInode, &inode, clustInd, op, p_outVal)) != 0)
      return error;
  }
  /*Double indirect references zone*/
  else
    if((error = soHandleDIndirect(sb, nInode, &inode, clustInd, op, p_outVal)) != 0)
      return error;

  /** Write inode **/
  if(op == CLEAN)
  {
    if((error = soWriteInode(&inode, nInode, FDIN)) != 0)
      return error;
  }
  else
  {
    if((error = soWriteInode(&inode, nInode, IUIN)) != 0)
      return error;
  }
  /** Operation successful **/
  return 0;
}

/**
 *  \brief Handle of a file data cluster which belongs to the direct references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE /FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soHandleDirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t clustInd, uint32_t op,
                           uint32_t *p_outVal)
{
  /** Variables **/
  uint32_t error;

  /** Operation validation **/
  /*if((op < GET) || (op > CLEAN))*/
  if(op != GET && op != ALLOC && op != FREE && op != FREE_CLEAN && op != CLEAN)
    return -EINVAL;

  /** Direct Reference zone operations **/
  switch(op)
  {
    /*Get the logical number of the referenced data cluster*/
    case GET:
      *p_outVal = p_inode->d[clustInd];
      return 0;

    /*Allocate cluster*/
    case ALLOC:
      /*check if cluster isn't already allocated*/
      if(p_inode->d[clustInd] != NULL_CLUSTER)
        return -EDCARDYIL;
      /*allocate a data cluster*/
      if((error = soAllocDataCluster(nInode, p_outVal)) != 0)
        return error;
      /*update inode information*/
      p_inode->d[clustInd] = *p_outVal;
      p_inode->clucount += 1;
      /*operation successful*/
      return 0;

    /*Free, free and clean or clean cluster*/
    case FREE:
    case FREE_CLEAN:
    case CLEAN:
      if(p_inode->d[clustInd] == NULL_CLUSTER)
        return -EDCNOTIL;
      /*free data cluster (if needed)*/
      if(op != CLEAN)
        if((error = soFreeDataCluster(p_inode->d[clustInd])) != 0)
          return error;
      /*if operation is FREE, then operation is complete*/
      if(op == FREE)
        return 0;
      /*clean data cluster*/
      if((error = soCleanLogicalCluster(p_sb, nInode, p_inode->d[clustInd])) != 0)
        return error;
      /*update inode information*/
      p_inode->d[clustInd] = NULL_CLUSTER;
      p_inode->clucount -= 1;
      /*operation successful*/
      return 0;

    default:
      return -EINVAL;
  }

  /* Unreachable code - if it does reach => program malfunction */
  return -EINVAL;;
}


/**
 *  \brief Handle of a file data cluster which belongs to the single indirect references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE / FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soHandleSIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t clustInd, uint32_t op,
                              uint32_t *p_outVal)
{
  /** Variables **/
  int error;
  uint32_t clustIdx;
  uint32_t auxIdx;
  uint32_t physCluster;
  SODataClust *ClustInd; /*pointer to Indirect references cluster*/


  clustIdx = clustInd - N_DIRECT;  

  /** Check if inode has indirect references **/
  if(p_inode->i1 == NULL_CLUSTER)
  {
    switch(op)
    {
      case GET:
        *p_outVal = NULL_CLUSTER;
        return 0;

      case ALLOC:
        /*Allocate data cluster for Indirect references*/
        if((error = soAllocDataCluster(nInode, &p_inode->i1)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;

        /*Allocate data cluster for "clustInd"*/
        if((error = soAllocDataCluster(nInode, p_outVal)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;

        /*Read cluster that has indirect references information*/
        physCluster = p_inode->i1 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
        if((error = soLoadDirRefClust(physCluster)) != 0)
          return error;
        if((ClustInd = soGetDirRefClust()) == NULL)
          return -ELIBBAD;

        /*Update cluster that contains indirect references*/
        memset(ClustInd->info.ref, NULL_CLUSTER, (sizeof(uint32_t) * RPC) );
        ClustInd->info.ref[clustIdx] = *p_outVal;

        /*Update cluster*/
        if((error = soStoreDirRefClust()) != 0)
          return error;

        /*Operation successful*/
        return 0;

      /*Free, free and clean or clean cluster*/
      case FREE:
      case FREE_CLEAN:
      case CLEAN:
        return -EDCNOTIL;

      default:
        return -EINVAL;
    }
  }
  else
  {
    /** Read cluster that has indirect references information **/
    physCluster = p_inode->i1 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
    if((error = soLoadDirRefClust(physCluster)) != 0)
      return error;
    if((ClustInd = soGetDirRefClust()) == NULL)
      return -ELIBBAD;

    /** Handle requested operation **/
    switch(op)
    {
      case GET:
        *p_outVal = ClustInd->info.ref[clustIdx];
        return 0;

      case ALLOC:
        /*Check if cluster isn't already allocated*/
        if(ClustInd->info.ref[clustIdx] != NULL_CLUSTER)
          return -EDCARDYIL;
        /*Allocate data cluster*/
        if((error = soAllocDataCluster(nInode, p_outVal)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;

        /*Re-read cluster that has indirect references information*/
        if((error = soLoadDirRefClust(physCluster)) != 0)
          return error;
        if((ClustInd = soGetDirRefClust()) == NULL)
          return error;
        /*Update indirect references cluster*/
        ClustInd->info.ref[clustIdx] = *p_outVal;
        if((error = soStoreDirRefClust()) != 0)
          return error;
        /*Operation successful*/
        return 0;

      /*Free, free and clean or clean cluster*/
      case FREE:
      case FREE_CLEAN:
      case CLEAN:
        /*Check if cluster is allocated*/
        if(ClustInd->info.ref[clustIdx] == NULL_CLUSTER)
          return -EDCNOTIL;
        /*Free data cluster, if needed*/
        if(op != CLEAN)
          if((error = soFreeDataCluster(ClustInd->info.ref[clustIdx])) != 0)
            return error;
        /*If requested operation is FREE, operation is complete*/
        if(op == FREE)
          return 0;

        /*Clean data cluster*/
        if((error = soCleanLogicalCluster(p_sb, nInode, ClustInd->info.ref[clustIdx])) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount -= 1;
        /*Update indirect references cluster*/
        if((error = soLoadDirRefClust(physCluster)) != 0)
          return error;
        if((ClustInd = soGetDirRefClust()) == NULL)
          return -ELIBBAD;
        ClustInd->info.ref[clustIdx] = NULL_CLUSTER;
        if((error = soStoreDirRefClust()) != 0)
          return error;

        /*Check if list of indirect references is empty*/
        if((error = soLoadDirRefClust(physCluster)) != 0)
          return error;
        if((ClustInd = soGetDirRefClust()) == NULL)
          return -ELIBBAD;

        auxIdx = 0;
        while((auxIdx < RPC) && (ClustInd->info.ref[auxIdx] == NULL_CLUSTER))
          auxIdx++;
        if(auxIdx == RPC)
        {
          ClustInd->stat = NULL_INODE;
          if((error = soStoreDirRefClust()) != 0)
            return error;
          /*Free indirect references cluster*/
          if((error = soFreeDataCluster(p_inode->i1)) != 0)
            return error;

         /*Updatde inode information*/
          p_inode->i1 = NULL_CLUSTER;
          p_inode->clucount -= 1; 
        }

        /*Operation successful*/
        return 0;

      default:
        return -EINVAL;
    }
  }

  /* Unreachable code - if it does reach => program malfunction */
  return -EINVAL;
}

/**
 *  \brief Handle of a file data cluster which belongs to the double indirect references list.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param p_inode pointer to a buffer which stores the inode contents
 *  \param clustInd index to the list of direct references belonging to the inode which is referred
 *  \param op operation to be performed (GET, ALLOC, FREE, FREE AND CLEAN, CLEAN)
 *  \param p_outVal pointer to a location where the physical number of the data cluster is to be stored (GET / ALLOC);
 *                  in the other cases (FREE / FREE AND CLEAN / CLEAN) it is not used (in these cases, it should be set
 *                  to \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the requested operation is invalid
 *  \return -\c EDCARDYIL, if the referenced data cluster is already in the list of direct references (ALLOC)
 *  \return -\c EDCNOTIL, if the referenced data cluster is not in the list of direct references
 *              (FREE / FREE AND CLEAN / CLEAN)
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soHandleDIndirect (SOSuperBlock *p_sb, uint32_t nInode, SOInode *p_inode, uint32_t clustInd, uint32_t op,
                              uint32_t *p_outVal)
{
  /** Variables **/
  int error;
  uint32_t logDRefClust; /*Direct references cluster's logical number*/
  uint32_t logIRefClust;
  uint32_t phyDRefClust; /*Direct references cluster's physical number*/
  uint32_t phyIRefClust; /*Indirect references cluster's physical number*/

  uint32_t DClustIdx;	  /*Direct reference cluster's index*/
  uint32_t IClustIdx;     /*Indirect references cluster's index*/
  uint32_t auxIdx;        /*Auxiliary index*/

  SODataClust *IndClust;  /*Pointer to indirect references cluster*/
  SODataClust *DirClust;  /*Pointer to direct references cluster*/

  IClustIdx = (clustInd - N_DIRECT - RPC) / RPC;
  DClustIdx = (clustInd - N_DIRECT - RPC) % RPC;


  /** Check if inode has double indirect references **/
  if(p_inode->i2 == NULL_CLUSTER)
  {
    switch(op)
    {
      case GET:
        *p_outVal = NULL_CLUSTER;
        return 0;

      case ALLOC:
        /*Allocate Indirect references cluster*/
        if((error = soAllocDataCluster(nInode, &p_inode->i2)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;

        /*Allocate Direct references cluster*/
        if((error = soAllocDataCluster(nInode, &logDRefClust)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;

        /*Read Indirect references cluster*/
        phyIRefClust = p_inode->i2 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
        if((error = soLoadSngIndRefClust(phyIRefClust)) != 0)
          return error;
        if((IndClust = soGetSngIndRefClust()) == NULL)
          return -ELIBBAD;
        /*Update and write cluster*/
        memset(IndClust->info.ref, NULL_CLUSTER, (sizeof(uint32_t) * RPC) );
        IndClust->info.ref[IClustIdx] = logDRefClust;
        if((error = soStoreSngIndRefClust()) != 0)
          return error;
 
        /*Allocate File cluster*/
        if((error = soAllocDataCluster(nInode, p_outVal)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;

        /*Re-Read indirect references cluster*/
        if((error = soLoadSngIndRefClust(phyIRefClust)) != 0)
          return error;
        if((IndClust = soGetSngIndRefClust()) == NULL)
          return -ELIBBAD;
        /*Read Direct References cluster*/
        phyDRefClust = logDRefClust * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
        if((error = soLoadDirRefClust(phyDRefClust)) != 0)
          return error;
        if((DirClust = soGetDirRefClust()) == NULL)
          return -ELIBBAD;
        /*Update and write cluster*/
        memset(DirClust->info.ref, NULL_CLUSTER, sizeof(uint32_t) * RPC);
        DirClust->info.ref[DClustIdx] = *p_outVal;
        if((error = soStoreDirRefClust()) != 0)
          return error;

        return 0;

      case FREE:
      case FREE_CLEAN:
      case CLEAN:
        return -EDCNOTIL;

      default:
        return -EINVAL;
    }
  }
  else
  {
    /*Read Indirect references cluster*/
    phyIRefClust = p_inode->i2 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
    if((error = soLoadSngIndRefClust(phyIRefClust)) != 0)
      return error;
    if((IndClust = soGetSngIndRefClust()) == NULL)
      return -ELIBBAD;

    /*Handle requested operation*/
    switch(op)
    {
      case GET:
        if(IndClust->info.ref[IClustIdx] == NULL_CLUSTER)
        {
          *p_outVal = NULL_CLUSTER;
          return 0;
        }
        /*Read Direct references cluster*/
        phyDRefClust = IndClust->info.ref[IClustIdx] * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
        if((error = soLoadDirRefClust(phyDRefClust)) != 0)
          return error;
        if((DirClust = soGetDirRefClust()) == NULL)
          return -ELIBBAD;

        *p_outVal = DirClust->info.ref[DClustIdx];
        return 0;

      case ALLOC:
        /*Allocate Direct references cluster, if needed*/
        if(IndClust->info.ref[IClustIdx] == NULL_CLUSTER)
        {
          if((error = soAllocDataCluster(nInode, &logIRefClust)) != 0)
            return error;

          /*Update inode information*/
          p_inode->clucount += 1;

          /*Read cluster again*/
          if((error = soLoadSngIndRefClust(phyIRefClust)) != 0)
            return error;
          if((IndClust = soGetSngIndRefClust()) == NULL)
            return -ELIBBAD;

          IndClust->info.ref[IClustIdx] = logIRefClust;

          /*Update and write indirect ref. cluster information*/
          if((error = soStoreSngIndRefClust()) != 0)
            return error;

          /*Read, update and write direct ref. cluster*/
          phyDRefClust = IndClust->info.ref[IClustIdx] * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
          if((error = soLoadDirRefClust(phyDRefClust)) != 0)
            return error;
          if((DirClust = soGetDirRefClust()) == NULL)
            return -ELIBBAD;
          memset(DirClust->info.ref, NULL_CLUSTER, sizeof(uint32_t) * RPC);
          if((error = soStoreDirRefClust()) != 0)
            return error;
        }

        /*Read Direct references cluster*/
        phyDRefClust = IndClust->info.ref[IClustIdx] * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
        if((error = soLoadDirRefClust(phyDRefClust)) != 0)
          return error;
        if((DirClust = soGetDirRefClust()) == NULL)
          return -ELIBBAD;

        /*Check if file cluster can be allocated*/
        if(DirClust->info.ref[DClustIdx] != NULL_CLUSTER)
          return -EDCARDYIL;

        /*Allocate file cluster*/
        if((error = soAllocDataCluster(nInode, p_outVal)) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount += 1;
        /*Update direct references cluster*/
        DirClust->info.ref[DClustIdx] = *p_outVal;
        if((error = soStoreDirRefClust()) != 0)
          return error;
        /*Operation successful*/
        return 0;

      case FREE:
      case FREE_CLEAN:
      case CLEAN:
        /*Check if direct references cluster exists*/
        if(IndClust->info.ref[IClustIdx] == NULL_CLUSTER)
          return -EDCNOTIL;

        /*Read direct references cluster*/
        phyDRefClust = IndClust->info.ref[IClustIdx] * BLOCKS_PER_CLUSTER + p_sb->dzone_start;
        if((error = soLoadDirRefClust(phyDRefClust)) != 0)
          return error;
        if((DirClust = soGetDirRefClust()) == NULL)
          return -ELIBBAD;
        /*Check if file cluster is allocated*/
        if(DirClust->info.ref[DClustIdx] == NULL_CLUSTER)
          return -EDCNOTIL;

        /*Free data cluster, if operation requests so*/
        if(op != CLEAN)
          if((error = soFreeDataCluster(DirClust->info.ref[DClustIdx])) != 0)
            return error;
        /*If requested operation is FREE -> all done*/
        if(op == FREE)
          return 0;

        /*Clean cluster*/
        if((error = soCleanLogicalCluster(p_sb, nInode, DirClust->info.ref[DClustIdx])) != 0)
          return error;
        /*Update direct cluster information*/
        DirClust->info.ref[DClustIdx] = NULL_CLUSTER;
        if((error = soStoreDirRefClust()) != 0)
          return error;
        /*Update inode information*/
        p_inode->clucount -= 1;

        /*Check if Direct references cluster is empty*/
        auxIdx = 0;
        while((auxIdx < RPC) && (DirClust->info.ref[auxIdx] == NULL_CLUSTER))
          auxIdx++;
        if(auxIdx == RPC)
        {
          DirClust->stat = NULL_INODE;
          if((error = soStoreDirRefClust()) != 0)
            return error;
          /*Free Direct references cluster*/
          if((error = soFreeDataCluster(IndClust->info.ref[IClustIdx])) != 0)
            return error;
          /*Update indirect references cluster*/
          IndClust->info.ref[IClustIdx] = NULL_CLUSTER;
          if((error = soStoreSngIndRefClust()) != 0)
            return error;
          /*Update inode information*/
          p_inode->clucount -= 1;

          /*Check if indirect references cluster is empty*/
          auxIdx = 0;
          while((auxIdx < RPC) && (IndClust->info.ref[auxIdx] == NULL_CLUSTER))
            auxIdx++;
          if(auxIdx == RPC){
            IndClust->stat = NULL_INODE;
            if((error = soStoreSngIndRefClust()) != 0)
              return error;
            /*Free indirect references cluster*/
            if((error = soFreeDataCluster(p_inode->i2)) != 0)
              return error;

            /*Update inode information*/
            p_inode->i2 = NULL_CLUSTER;
            p_inode->clucount -= 1;
          }
        }

        /*Operation successful*/
        return 0;

      default:
        return -EINVAL;
    }
  }


  /* Unreachable code - if it does reach => program malfunction */
  return -EINVAL;

}

/**
 *  \brief Clean a file data cluster whose logical number is known.
 *
 *  \param p_sb pointer to a buffer where the superblock data is stored
 *  \param nInode number of the inode associated to the file
 *  \param nLClust logical number of the data cluster
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

static int soCleanLogicalCluster (SOSuperBlock *p_sb, uint32_t nInode, uint32_t nLClust)
{
  /** Variables **/
  uint32_t error;
  uint32_t physCluster; /*physical number of the data cluster*/
  SODataClust cleanCluster;

  physCluster = p_sb->dzone_start + nLClust * BLOCKS_PER_CLUSTER;

  /** Read cluster **/
  if((error = soReadCacheCluster(physCluster, &cleanCluster)) != 0)
    return error;

  /** Inode check **/
  if(cleanCluster.stat != nInode)
    return -EWGINODENB;

  /** Clean cluster **/
  memset(&(cleanCluster.info), 0, BSLPC);
  cleanCluster.stat = NULL_INODE;

  /** Write cluster **/
  if((error = soWriteCacheCluster(physCluster, &cleanCluster)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
