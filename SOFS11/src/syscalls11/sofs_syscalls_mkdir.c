/**
 *  \file sofs_syscalls_mkdir.c (implementation file for syscall soMkdir)
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
 *  \author André Garcia   35853 T6G2 - November 2011
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

/*Allusion to internal functions*/
static void getMode(uint32_t *newMode, mode_t mode);

/**
 *  \brief Create a directory.
 *
 *  It tries to emulate <em>mkdir</em> system call.
 *
 *  \param ePath path to the file
 *  \param mode type and permissions to be set:
 *                    a bitwise combination of S_ISVTX, S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH,
 *                    S_IWOTH, S_IXOTH
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path or
 *                      no mode of the defined class is described
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt> is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt>, but the last one, is
 *                      found
 *  \return -\c EEXIST, if a file described by <tt>ePath</tt> already exists or the last component is a symbolic link
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory that will hold
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soMkdir (const char *ePath, mode_t mode)
{
  soProbe (81, "soMkdir (\"%s\", %u)\n", ePath, mode);

  /** Variables **/
  int32_t error;
  int32_t error2;
  uint32_t nInodeDir;
  uint32_t nInodeEnt;
  uint32_t newMode;

  SOInode InodeEnt;

  char auxPath[MAX_PATH + 1];
  char dirPath[MAX_PATH + 1];
  char entName[MAX_NAME + 1];

  /** Parameter check **/
  /*ePath validation*/
  if((ePath == NULL) || (strncmp("/", ePath, 1) != 0))
    return -EINVAL;
  /*mode validation*/
  /*NOTE: S_ISVTX isn't checked because it doesn't exist in SOFS11*/
  switch(mode)
  {
    case 0:
    case S_ISVTX:
      return -EINVAL;
    default:
      break;
  }

  /** Conformity check **/
  if(strlen(ePath) > MAX_PATH)
    return -ENAMETOOLONG;

  /** Get parent directory path and new directory name **/
  strcpy((char *) auxPath, ePath);
  strcpy(dirPath, dirname(auxPath));
  strcpy((char *) auxPath, ePath);
  strcpy(entName, basename(auxPath));

  /** entName conformity check **/
  if(strlen(entName) > MAX_NAME)
    return -ENAMETOOLONG;

  /** Get parent directory inode number **/
  if((error = soGetDirEntryByPath(dirPath, NULL, &nInodeDir)) != 0)
    return error;

  /** Check if a file described by ePath already exists **/
  if((error = soGetDirEntryByName(nInodeDir, entName, NULL, NULL)) == 0)
    return -EEXIST;
  else if (error != (-ENOENT))
    return error;

  /** Check if process has write permission on parent directory **/
  if((error = soAccessGranted(nInodeDir, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }

  /** Allocate an inode for the new directory **/
  if((error = soAllocInode(INODE_DIR, &nInodeEnt)) != 0)
    return error;

  /** Read new directory's inode **/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
  {
    if((error2 = soFreeInode(nInodeEnt)) != 0)
      return error2;
    if((error2 = soCleanInode(nInodeEnt)) != 0)
      return error2;
    return error;
  }

  /** Update mode **/
  getMode(&newMode, mode);
  InodeEnt.mode = InodeEnt.mode | newMode;

  /** Write new directory updated inode **/
  if((error = soWriteInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
  {
    if((error2 = soFreeInode(nInodeEnt)) != 0)
      return error2;
    if((error2 = soCleanInode(nInodeEnt)) != 0)
      return error2;
    return error;
  }

  /** Add new directory direntry **/
  if((error = soAddDirEntry(nInodeDir, entName, nInodeEnt)) != 0)
  {
    if((error2 = soFreeInode(nInodeEnt)) != 0)
      return error2;
    if((error2 = soCleanInode(nInodeEnt)) != 0)
      return error2;
    return error;
  }

  /** Operation successful **/
  return 0;
}


/*Auxiliary function to obtain mode*/
static void getMode(uint32_t *newMode, mode_t mode){

  /*Set all mode bits to zero*/
  *newMode = 0x0000;

  /*User modes*/
  if((mode & S_IRUSR) == S_IRUSR)
    *newMode = *newMode | INODE_RD_USR;
  if((mode & S_IWUSR) == S_IWUSR)
    *newMode = *newMode | INODE_WR_USR;
  if((mode & S_IXUSR) == S_IXUSR)
    *newMode = *newMode | INODE_EX_USR;
  /*Group modes*/
  if((mode & S_IRGRP) == S_IRGRP)
    *newMode = *newMode | INODE_RD_GRP;
  if((mode & S_IWGRP) == S_IWGRP)
    *newMode = *newMode | INODE_WR_GRP;
  if((mode & S_IXGRP) == S_IXGRP)
    *newMode = *newMode | INODE_EX_GRP;
  /*Other modes*/
  if((mode & S_IROTH) == S_IROTH)
    *newMode = *newMode | INODE_RD_OTH;
  if((mode & S_IWOTH) == S_IWOTH)
    *newMode = *newMode | INODE_WR_OTH;
  if((mode & S_IXOTH) == S_IXOTH)
    *newMode = *newMode | INODE_EX_OTH;

}
