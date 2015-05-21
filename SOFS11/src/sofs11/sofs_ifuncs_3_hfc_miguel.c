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

/** \brief operation get the physical number of the referenced data cluster */
#define GET         0
/** \brief operation allocate a new data cluster and associate it to the inode which describes the file */
#define ALLOC       1
/** \brief operation free the referenced data cluster */
#define FREE        2
/** \brief operation free the referenced data cluster and dissociate it from the inode which describes the file */
#define FREE_CLEAN  3
/** \brief operation dissociate the referenced data cluster from the inode which describes the file */
#define CLEAN       4

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

  /* substitute by your code */
  int soHandleFileCluster_bin (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal);
  return soHandleFileCluster_bin(nInode, clustInd, op, p_outVal);
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
  /* insert your code here */
  return -EINVAL;
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
  int status; /* status veriable for error checking*/

  uint32_t logDRefClust = 0;

  uint32_t physDRefClust = 0; /* direct refrences table cluster's pysical
                                 number */

  uint32_t logFileClust = 0;/* file data cluster's logical number*/

  uint32_t dRefListIdx = (clustInd - N_DIRECT); /* calculating indirect
                                                   table entry position */

  SODataClust *dRefClust = NULL; /* pointer to the cluster which is being
                                    accessed by this routine */

  uint32_t dRListCurrP = 0; /* direct references list current position
                               index for iterating */


  if (p_inode->i1 == NULL_CLUSTER) {

    switch (op)
      {

      case GET:
        *p_outVal = NULL_CLUSTER;
        return 0;

      case ALLOC:
        /* allocating the direct references list cluster */
        if ( (status = soAllocDataCluster(nInode, &p_inode->i1)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* allocating the file data cluster */
        if ( (status = soAllocDataCluster(nInode, &logFileClust)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* initializing and updating direct references list */
        physDRefClust = p_inode->i1 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

        if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        memset(dRefClust->info.ref, NULL_CLUSTER, (sizeof(uint32_t) * RPC) );

        dRefClust->info.ref[dRefListIdx] = logFileClust;

        if ( (status = soStoreDirRefClust ()) != 0 )
          return status;

        *p_outVal = logFileClust;

        return 0;

      case FREE:
        return -EDCNOTIL;

      case FREE_CLEAN:
        return -EDCNOTIL;

      case CLEAN:
        return -EDCNOTIL;

      default:
        return -EINVAL;
      }
  }

  /* calculating direct references list cluster's pyisical number*/
  physDRefClust = p_inode->i1 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

  switch (op)
    {
    case GET:
      /* getting the file data cluster's logical number from the direct references
         list is to be stored */
      if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
        return status;

      dRefClust = soGetDirRefClust();
      if (dRefClust == NULL)
        return -ELIBBAD;

      *p_outVal = dRefClust->info.ref[dRefListIdx];
      return 0;

    case ALLOC:
      /* checking if the file datacluster is already in use*/
      if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
        return status;

      dRefClust = soGetDirRefClust();
      if (dRefClust == NULL)
        return -ELIBBAD;

      if (dRefClust->info.ref[dRefListIdx] != NULL_CLUSTER)
        return -EDCARDYIL;

      /* allocating the file data cluster */
      if ( (status = soAllocDataCluster(nInode, &logFileClust)) != 0 )
        return status;
      p_inode->clucount += 1;

      /* updating direct references list */
      if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
        return status;

      dRefClust = soGetDirRefClust();
      if (dRefClust == NULL)
        return -ELIBBAD;

      dRefClust->info.ref[dRefListIdx] = logFileClust;

      if ( (status = soStoreDirRefClust ()) != 0 )
        return status;

    case FREE:

    case FREE_CLEAN:

    case CLEAN:

      if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
        return status;

        dRefClust = soGetDirRefClust();
      if (dRefClust == NULL)
        return -ELIBBAD;

      logFileClust = dRefClust->info.ref[dRefListIdx];

      /* freeing the file datacluster*/
      if (op != CLEAN)
        if ( (status = soFreeDataCluster(logFileClust)) != 0 )
          return status;

      if (op != FREE) {
        /* cleaning the file data cluster */
        if ( (status = soCleanLogicalCluster(p_sb, nInode, logFileClust)) !=0 )
          return status;
        p_inode->clucount -= 1;

        /* updating inode and direct references list info*/
        if ( (status = soLoadDirRefClust (physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        dRefClust->info.ref[dRefListIdx] = NULL_CLUSTER;
        p_inode->clucount -= 1;

        /* checking if there is any other entry on the direct
           references list(i1), if not free that cluster as well */
        if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        while (dRListCurrP < RPC)
          {
            if (dRefClust->info.ref[dRListCurrP] != NULL_CLUSTER)
              break;
            dRListCurrP--;
          }

        if (dRListCurrP == RPC) {
          /* cleaning and freeing the direct references list data cluster */
          if ( (status = soFreeDataCluster(logDRefClust) ) != 0 )
            return status;

          if ( (status = soCleanLogicalCluster(p_sb, nInode, logDRefClust)) !=0 )
            return status;

          p_inode->i1 = NULL_CLUSTER;
          p_inode->clucount -= 1;
        }
      }
      return 0;

    default:
      return -EINVAL;
    }
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
  int32_t status; /* status veriable for error checking*/

  uint32_t physIRefClust = 0; /* direct refrences table cluster's pysical number */
  SODataClust *iRefClust = NULL; /* pointer to the cluster which is being accessed by this routine */
  SODataClust *dRefClust = NULL; /* pointer to the cluster which is being accessed by this routine */
  uint32_t logDRefClust = 0; /* direct refrences table cluster's logical number */
  uint32_t logIRefClust = 0; /* indirect refrences table cluster's logical number */
  uint32_t physDRefClust = 0; /* direct refrences table cluster's pysical number */
  uint32_t iRefListIdx = (clustInd - N_DIRECT - RPC) / RPC; /* TODO: calculating indirect table entry position */
  uint32_t logFileClust = 0;/* file data cluster's logical number*/
  uint32_t dRefListIdx = (clustInd - N_DIRECT - RPC); /* calculating indirect table entry position */
  uint32_t dRListCurrP = 0; /* direct references list current position index*/
  uint32_t iRListCurrP = 0; /* direct references list current position index */

  if (p_inode->i2 == NULL_CLUSTER) {

    switch (op)
      {
      case GET:
        *p_outVal = NULL_CLUSTER;
        return 0;

      case ALLOC:
        /* allocating the single indirect references list data cluster */
        if ( (status = soAllocDataCluster(nInode, &p_inode->i2)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* allocating the direct references list data cluster */
        if ( (status = soAllocDataCluster(nInode, &logDRefClust)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* allocating the file data cluster */
        if ( (status = soAllocDataCluster(nInode, &logFileClust)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* initializing and updating the direct references list data cluster */
        physDRefClust = logDRefClust * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

        if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        memset( dRefClust->info.ref, NULL_CLUSTER, ( sizeof(uint32_t) * RPC ) );

        iRefClust->info.ref[iRefListIdx] = logFileClust;

        if ( (status = soStoreDirRefClust ()) != 0 )
          return status;

        /* initializing and updating single indirect references list*/
        physIRefClust = p_inode->i2 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

        if ( (status = soLoadSngIndRefClust (physIRefClust)) != 0 )
          return status;

        iRefClust = soGetSngIndRefClust();
        if (iRefClust == NULL)
          return -ELIBBAD;

        memset( iRefClust->info.ref, NULL_CLUSTER, (sizeof(uint32_t) * RPC) );

        iRefClust->info.ref[iRefListIdx] = logDRefClust;

        if ( (status = soStoreSngIndRefClust ()) != 0 )
          return status;

        *p_outVal = logFileClust;
        return 0;

      case FREE:
        return -EDCNOTIL;

      case FREE_CLEAN:
        return -EDCNOTIL;

      case CLEAN:
        return -EDCNOTIL;

      default:
        return -EINVAL;
      }
  }

  /* checking if the direct references cluster is in use*/
  physIRefClust = p_inode->i2 * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

  if ( (status = soLoadSngIndRefClust (physIRefClust)) != 0 )
    return status;

  iRefClust = soGetSngIndRefClust();
  if (iRefClust == NULL)
    return -ELIBBAD;

  if (iRefClust->info.ref[iRefListIdx] == NULL_CLUSTER) {

    switch (op)
      {
      case GET:
        *p_outVal = NULL_CLUSTER;
        return 0;

      case ALLOC:
        /* allocating the direct references list data cluster */
        if ( (status = soAllocDataCluster(nInode, &logDRefClust)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* allocating the file data cluster */
        if ( (status = soAllocDataCluster(nInode, &logFileClust)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* updating the direct references list data cluster */
        physDRefClust = logDRefClust * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

        if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        memset( dRefClust->info.ref, NULL_CLUSTER, ( sizeof(uint32_t) * RPC ) );

        iRefClust->info.ref[iRefListIdx] = logFileClust;

        if ( (status = soStoreDirRefClust ()) != 0 )
          return status;

        /* updating single indirect references list*/
        if ( (status = soLoadSngIndRefClust (physIRefClust)) != 0 )
          return status;

        iRefClust = soGetSngIndRefClust();
        if (iRefClust == NULL)
          return -ELIBBAD;

        iRefClust->info.ref[iRefListIdx] = logDRefClust;

        if ( (status = soStoreSngIndRefClust ()) != 0 )
          return status;

        *p_outVal = logFileClust;
        return 0;

      case FREE:
        return -EDCNOTIL;

      case FREE_CLEAN:
        return -EDCNOTIL;

      case CLEAN:
        return -EDCNOTIL;

      default:
        return -EINVAL;
      }

  }
  /* checking if the referenced file data cluster is in use */
  logDRefClust = iRefClust->info.ref[iRefListIdx];

  physDRefClust = logDRefClust * BLOCKS_PER_CLUSTER + p_sb->dzone_start;

  if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
    return status;

  dRefClust = soGetDirRefClust();
  if (dRefClust == NULL)
    return -ELIBBAD;

  if (dRefClust->info.ref[dRefListIdx] == NULL_CLUSTER) {

    switch (op)
      {
      case GET:
        *p_outVal = NULL_CLUSTER;
        return 0;

      case ALLOC:
        /* allocating the file data cluster */
        if ( (status = soAllocDataCluster(nInode, &logFileClust)) != 0 )
          return status;
        p_inode->clucount += 1;

        /* updating the direct references list data cluster */
        if ( (status = soLoadDirRefClust(physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        iRefClust->info.ref[iRefListIdx] = logFileClust;

        if ( (status = soStoreDirRefClust ()) != 0 )
          return status;

        return 0;

      case FREE:
        return -EDCNOTIL;

      case FREE_CLEAN:
        return -EDCNOTIL;

      case CLEAN:
        return -EDCNOTIL;

      default:
        return -EINVAL;
      }

  }

  /* at this point it is known that the cluster is in use */
  logFileClust = dRefClust->info.ref[dRefListIdx];

  switch (op)
    {

    case GET:
      *p_outVal = logFileClust;
      return 0;

    case ALLOC:
      return -EDCARDYIL;

    case FREE:

    case FREE_CLEAN:

    case CLEAN:


      if (op != CLEAN) {
        /* freeing the file datacluster */
        if ( (status = soFreeDataCluster(logFileClust)) != 0 )
          return status;
      }

      if (op != FREE) {
        /* cleanind the file data cluster */
        if ( (status = soCleanLogicalCluster(p_sb, nInode, logFileClust)) !=0 )
          return status;
        p_inode->clucount -= 1;

        /* updating direct references list */
        if ( (status = soLoadDirRefClust (physDRefClust)) != 0 )
          return status;

        dRefClust = soGetDirRefClust();
        if (dRefClust == NULL)
          return -ELIBBAD;

        dRefClust->info.ref[dRefListIdx] = NULL_CLUSTER;

        /* checking if there is any other entry on the direct references list */
        while (dRListCurrP < RPC)
          {
            if (dRefClust->info.ref[dRListCurrP] != NULL_CLUSTER)
              break;
            dRListCurrP--;
          }

        if (dRListCurrP == RPC) {
          /* freeing and cleaning the direct references table data cluster */
          if ( (status = soFreeDataCluster(logDRefClust) ) != 0 )
            return status;

          if ( (status = soCleanLogicalCluster(p_sb, nInode, logDRefClust)) !=0 )
            return status;

          p_inode->clucount -= 1;

          /* checking if there is any other entry on the direct references list */
          if ( (status = soLoadSngIndRefClust (physIRefClust)) != 0 )
            return status;

          iRefClust = soGetSngIndRefClust();
          if (iRefClust == NULL)
            return -ELIBBAD;

          while (iRListCurrP < RPC)
            {
              if (iRefClust->info.ref[iRListCurrP] != NULL_CLUSTER)
                break;
              iRListCurrP--;
            }

          if (dRListCurrP == RPC) {
            /* freeing and cleaning the single indirect references table data cluster */
            if ( (status = soFreeDataCluster(logIRefClust) ) != 0 )
              return status;

            if ( (status = soCleanLogicalCluster(p_sb, nInode, logIRefClust)) !=0 )
              return status;

            p_inode->clucount -= 1;

            p_inode->i2 = NULL_CLUSTER;
          }
        }
      }

    default:
      return -EINVAL;
    }
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
  /* insert your code here */
  return -EINVAL;
}
