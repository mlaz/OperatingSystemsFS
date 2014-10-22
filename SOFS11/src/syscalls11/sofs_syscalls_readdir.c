/**
 *  \file sofs_syscalls_readdir.c (implementation file for syscall soReaddir)
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
 *  \author Monica Feitosa 48613 T6G2 - November 2011
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
 *  \brief Read a direntry from a directory.
 *
 *  It tries to emulate <em>getdents</em> system call, but it reads a single direntry in use at a time.
 *
 *  \remark The returned value is the number of bytes read from the directory in order to get the next nonempty
 *          direntry. So, skipped empty direntries must be accounted for. The point is that the system (through FUSE)
 *          uses the returned value to update file position.
 *
 *  \param ePath path to the file
 *  \param buff pointer to the buffer where data to be read is to be stored
 *  \param pos starting [byte] position in the file data continuum where data is to be read from
 *
 *  \return <em>number of bytes effectively read to get a direntry in use (0, if the end is reached)</em>, on success
 *  \return -\c EINVAL, if either of the pointers are \c NULL or the path string does not describe an absolute path or
 *                      <em>pos</em> value is not a multiple of the size of a <em>directory entry</em>
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt> is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not read permission on the directory described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReaddir (const char *ePath, void *buff, int32_t pos)
{
  soProbe (83, "soReaddir (\"%s\", %p, %u)\n", ePath, buff, pos);
 
  /** Variables **/
  int32_t error;
  int32_t endReached = 0;
  uint32_t nInodeDir;
  uint32_t nInodeEnt;
  uint32_t clustInd;
  uint32_t clustOffset;
  uint32_t nClusters;
  uint32_t dirEntryIdx;
  uint32_t readEntrys = 0;

  SOInode InodeEnt;
  SODirEntry dirEntry[DPC];

  char auxPath[MAX_PATH + 1];
  char dirPath[MAX_PATH + 1];
  char entName[MAX_NAME + 1];

  /** Parameter check **/
  /*Check if either of the pointers are NULL*/
  if((ePath == NULL) || (buff == NULL))
    return -EINVAL;
  /*Check if the path string does describe an absolute path*/
  if(strncmp("/", ePath, 1) != 0)
    return -EINVAL;
  /*Check if pos value is a multiple of the size of a directory entry*/
  if((pos % sizeof(SODirEntry)) != 0)
    return -EINVAL;
  if(pos < 0)
    return -EINVAL;

  /** Conformity check **/
  if(strlen(ePath) > MAX_PATH)
    return -ENAMETOOLONG;

  /** Get directory path and entry name **/
  strcpy((char *) auxPath, ePath);
  strcpy(dirPath, dirname(auxPath));
  strcpy((char *) auxPath, ePath);
  strcpy(entName, basename(auxPath));
  /*Check entry name consistency*/
  if(strcmp(entName, "/") == 0)
    strncpy(entName, ".", MAX_NAME  + 1);

  /** Get directory inode number **/
  if((error = soGetDirEntryByPath(dirPath, NULL, &nInodeDir)) != 0)
    return error;

  /** Get entry inode number **/
  if((error = soGetDirEntryByName(nInodeDir, entName, &nInodeEnt, NULL)) != 0)
    return error;

  /** Read entry inode **/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Check if ePath is indeed a directory **/
  if((InodeEnt.mode & INODE_TYPE_MASK) != INODE_DIR)
    return -ENOTDIR;

  /** Check if the process that calls the operation has read permission **/
  if((error = soAccessGranted(nInodeEnt, R)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }

  /** Convert pos into cluster index and cluster offset **/
  clustInd = pos / (sizeof(SODirEntry) * DPC);
  clustOffset = pos % (sizeof(SODirEntry) * DPC);

  /** Get numbers of clusters directory has **/
  nClusters = InodeEnt.clucount;

  /** If end of file hasn't been reached, read direntry **/
  if(clustInd < nClusters)
  {
    /*Read cluster that contains current direntry*/
    if((error = soReadFileCluster(nInodeEnt, clustInd, &dirEntry)) != 0)
      return error;

    /*Convert cluster offset into direntry index - offset is given in bytes*/
    dirEntryIdx = clustOffset / sizeof(SODirEntry);

    /*Get index of next nonempty dirEntry*/
    while((!endReached) &&
          (dirEntry[dirEntryIdx].nInode == NULL_INODE || dirEntry[dirEntryIdx].name[0] == 0))
    {
      readEntrys += 1;
      dirEntryIdx += 1;
      /*Check if end of cluster reached*/
      if(dirEntryIdx == DPC)
      {
        clustInd += 1;
        /*If end of directory not reached, read next cluster*/
        if(clustInd < nClusters)
        {
          dirEntryIdx = 0;
          if((error = soReadFileCluster(nInodeEnt, clustInd, &dirEntry)) != 0)
            return error;
        }
        else
        {
          endReached = 1;
          readEntrys = 0;
        }
      }
    }

    /*If end of directory not reached, read direntry*/
    if(!endReached)
    {
      readEntrys += 1;
      strncpy(buff, (const char *) dirEntry[dirEntryIdx].name, MAX_NAME + 1);
    }
  }

  /** Operation successful **/
  return readEntrys * sizeof(SODirEntry);
}

