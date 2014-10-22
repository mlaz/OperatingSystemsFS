/**
 *  \file sofs_syscalls_truncate.c (implementation file for syscall soTruncate)
 *
 *  \brief Set of operations to manage system calls.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li mount the SOFS10 file system
 *      \li unmount the SOFS10 file system
 *      \li get file system statistics
 *      \li get file status
 *      \li check real user's permissions for a file
 *      \li change permissions of a file
 *      \li change the ownership of a file
 *      \li make a new name for a file
 *      \li delete the name of a file from a directory and possibly the file it refers to from the file system
 *      \li change the name or the location of a file in the directory hierarchy of the file system
 *      \li create a regular file with size 0
 *      \li open a regular file
 *      \li close a regular file
 *      \li read data from an open regular file
 *      \li write data into an open regular file
 *      \li synchronize a file's in-core state with storage device
 *      \li create a directory
 *      \li delete a directory
 *      \li open a directory for reading
 *      \li read a direntry from a directory
 *      \li close a directory
 *      \li make a new name for a regular file or a directory
 *      \li read the value of a symbolic link.
 *
 *  \author Artur Carneiro Pereira September 2007
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - October 2010 / October 2011
 *  \author Miguel Azevedo 38569 T6G2 - November 2011
 *  \author Gilberto Matos 27657 T6G2 - November 2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <libgen.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_const.h"
#include "sofs_rawdisk.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"
#include "sofs_ifuncs_3.h"
#include "sofs_ifuncs_4.h"


/**
 *  \brief Truncate a regular file to a specified length.
 *
 *  It tries to emulate <em>truncate</em> system call.
 *
 *  \param ePath path to the file
 *  \param length new size for the regular size
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c EISDIR, if <tt>ePath</tt> describes a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EFBIG, if the file may grow passing its maximum size
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the file described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
/*int soTruncate_bin (const char *ePath, off_t length);
int soTruncate_our (const char *ePath, off_t length);

int soTruncate (const char *ePath, off_t length){
  return soTruncate_bin (ePath, length);
}*/


int soTruncate (const char *ePath, off_t length)
{
  soProbe (80, "soTruncate (\"%s\", %u)\n", ePath, length);

  /** Variables **/
  int32_t error;
  uint32_t nInodeDir;
  uint32_t nInodeEnt;
  uint32_t ClusterIdx;
  uint32_t ClusterOff;

  SOInode InodeEnt;

  unsigned char Data[BSLPC];


  /** Parameter check **/
  if((ePath == NULL) || (strncmp("/", ePath, 1) != 0))
    return -EINVAL;

  /** Conformity check **/
  if(strlen(ePath) > MAX_PATH)
    return -ENAMETOOLONG;
  if(length > MAX_FILE_SIZE)
    return -EFBIG;

  /** Get parent directory and entry's inode numbers **/
  if((error = soGetDirEntryByPath(ePath, &nInodeDir, &nInodeEnt)) != 0)
    return error;

  /** Check if process has write permission on the file**/
  if((error = soAccessGranted(nInodeEnt, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  } 

  /** Read entry inode **/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Check if ePath does not describe a directory **/
  if((InodeEnt.mode & INODE_TYPE_MASK) == INODE_DIR)
    return -EISDIR;

  /** Truncate **/
  /*Inode size needs to grow*/
  if(InodeEnt.size < (uint32_t) length)
  {
    /*Convert new length into cluster index*/
    if((error = soConvertBPIDC((uint32_t) length - 1, &ClusterIdx, &ClusterOff)) != 0)
      return error;
    /*Write new cluster in inode*/
    if((error = soReadFileCluster(nInodeEnt, ClusterIdx, &Data)) != 0)
      return error;
    if((error = soWriteFileCluster(nInodeEnt, ClusterIdx, &Data)) != 0)
      return error;
    /*Read updated inode and update size field*/
    if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
      return error;
    InodeEnt.size = (uint32_t) length;
    if((error = soWriteInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
      return error;
  }
  /*Inode size needs to shrunk*/
  else if(InodeEnt.size > (uint32_t) length)
  {
    /*Convert new length into cluster index and offset*/
    if((error = soConvertBPIDC((uint32_t) length, &ClusterIdx, &ClusterOff)) != 0)
      return error;

    /*Check if new size at the end of the cluster*/
    if(ClusterOff != BSLPC)
    {
      /*Read cluster*/
      if((error = soReadFileCluster(nInodeEnt, ClusterIdx, &Data)) != 0)
        return error;
      /*Update cluster*/
      memset(&Data[ClusterOff], 0, BSLPC - ClusterOff);
      /*Write cluster*/
      if((error = soWriteFileCluster(nInodeEnt, ClusterIdx, &Data)) != 0)
        return error;
    }

    /*Free and clean remaining clusters*/
    if((error = soHandleFileClusters(nInodeEnt, ClusterIdx + 1, FREE_CLEAN)) != 0)
      return error;

    /*Read updated inode and update size field*/
    if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
      return error;
    InodeEnt.size = (uint32_t) length;
    if((error = soWriteInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
      return error;
  }

  /** Operation successful **/
  return 0;
}
