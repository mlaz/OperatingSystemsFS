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
  uint32_t error;
  SOInode inode;
  SOSuperBlock *sb;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) < 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Conformity check **/
  if(nInode >= sb->itotal || clustInd >= MAX_FILE_CLUSTERS)
    return -EINVAL;
  if((op != GET) && (op != ALLOC) && (op != FREE) && (op != FREE_CLEAN) && (op != CLEAN))
    return -EINVAL;
  if((op == GET || op == ALLOC) && (p_outVal == NULL))
    return -EINVAL;

  /** Consistency check **/
  /*Read inode*/
  if(op == CLEAN){
    if((error = soReadInode(&inode, nInode, FDIN)) != 0)
      return error;
  }
  else{
    if((error = soReadInode(&inode, nInode, IUIN)) != 0)
      return error;
  }

  /** Check reference zone **/
  /*Direct references zone*/
  if(clustInd < N_DIRECT){
    if((error = soHandleDirect(sb, nInode, &inode, clustInd, op, p_outVal)) != 0)
      return error;
  }
  /*Single indirect references zone*/
  else if(clustInd < N_DIRECT + RPC){
    if((error = soHandleSIndirect(sb, nInode, &inode, clustInd, op, p_outVal)) != 0)
      return error;
  }
  /*Double indirect references zone*/
  else
    if((error = soHandleDIndirect(sb, nInode, &inode, clustInd, op, p_outVal)) != 0)
      return error;

  /** Write inode **/
  if(op != GET && op != FREE)
    if((error = soWriteInode(&inode, nInode, IUIN)) != 0)
      return error;

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
  }

  /** Operation successful - unreachable code **/
  return 0;
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
  /* insert your code here */
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
  /* insert your code here */
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
  memset(&(cleanCluster.info), 0, sizeof(cleanCluster.info));
  cleanCluster.stat = NULL_INODE;

  /** Write cluster **/
  if((error = soWriteCacheCluster(physCluster, &cleanCluster)) != 0)
    return error;

  /** Operation successful **/
  return 0;
}
