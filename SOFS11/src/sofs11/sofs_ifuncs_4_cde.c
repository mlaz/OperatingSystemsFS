/**
 *  \file sofs_ifuncs_4_cde.c (implementation file for function soCheckDirectoryEmptiness)
 *
 *  \brief Set of operations to manage directories and directory entries: level 4 of the internal file system
 *         organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li get an entry by path
 *      \li get an entry by name
 *      \li add an new entry to a directory
 *      \li remove an entry from a directory
 *      \li rename an entry of a directory
 *      \li check a directory status of emptiness
 *      \li attach a directory entry to a directory
 *      \li detach a directory entry from a directory.
 *
 *  \author Artur Carneiro Pereira September 2008
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - October 2010 / October 2011
 *  \author Monica Feitosa 48613 T6G2 - October 2011
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
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"
#include "sofs_ifuncs_4.h"


/**
 *  \brief Check a directory status of emptiness.
 *
 *  The directory contents is parsed to assert if all its entries, except for the first two, are free and are either
 *  in the clean or the dirty state. Thus, the inode associated to the directory must be in use and belong to the
 *  directory type.
 *
 *  The two first aforementioned entries must be in use and be named, respectively, "." and "..".
 *
 *  \param nInodeDir number of the inode associated to the directory
 *
 *  \return <tt>0 (zero)</tt>, if the directory is empty
 *  \return -\c ENOTEMPTY, if the directory is not empty
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c ENOTDIR, if the inode type is not a directory
 *  \return -\c ENOTEMPTY, if the directory is not empty
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soCheckDirectoryEmptiness (uint32_t nInodeDir)
{
  soProbe (116, "soCheckDirectoryEmptiness (%"PRIu32")\n", nInodeDir);

  /** Variables **/
  int error;
  uint32_t nDirClusters;
  uint32_t currCluster;
  uint32_t currEntry;
  SODirEntry dirCluster[DPC];
  SOInode InodeDir;
  SOSuperBlock *sb;

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Conformity check **/
  if(nInodeDir >= sb->itotal)
    return -EINVAL;

  /** Read inode */
  if((error = soReadInode(&InodeDir, nInodeDir, IUIN)) != 0)
    return error;

  /** Check if inode is a directory **/
  if((InodeDir.mode & INODE_DIR) != INODE_DIR)
    return -ENOTDIR;

  /** Check directory consistency **/
  if((error = soQCheckDirCont(sb, &InodeDir)) != 0)
    return error;

  /** Get number of clusters the directory uses **/
  nDirClusters = InodeDir.size / sizeof(dirCluster);

  /** Check directory emptiness **/
  for(currCluster = 0; currCluster < nDirClusters; currCluster++)
  {
    /*Read current cluster*/
    if((error = soReadFileCluster(nInodeDir, currCluster, &dirCluster)) != 0)
      return error;

    /*Check "." and ".." entrys*/
    if(currCluster == 0)
    {
      currEntry = 2;
      if(strncmp(".", (const char *)dirCluster[0].name, strlen((const char *)dirCluster[0].name)) != 0)
        return -EDIRINVAL;
      if(strncmp("..", (const char *)dirCluster[1].name, strlen((const char *)dirCluster[1].name)) != 0)
        return -EDIRINVAL;
    }
    else
      currEntry = 0;

    /*Check dir entry*/
    while(currEntry < DPC)
    {
      if(strncmp("\0", (const char *)dirCluster[currEntry].name, 1) != 0)
        return -ENOTEMPTY;
      currEntry++;
    }
  }

  /** Operation successful **/
  return 0;
}
