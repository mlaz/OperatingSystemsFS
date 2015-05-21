/**
 *  \file sofs_syscalls_rename.c (implementation file for syscall soRename)
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
static void soUniqueName(const char *name, char *uniqueName);

/**
 *  \brief Change the name or the location of a file in the directory hierarchy of the file system.
 *
 *  It tries to emulate <em>rename</em> system call.
 *
 *  \param oldPath path to an existing file
 *  \param newPath new path to the same file in replacement of the old one
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to either of the strings is \c NULL, or the path strings do not describe
 *                      absolute paths, or <tt>oldPath</tt> describes a directory and is a substring of <tt>newPath</tt>
 *                      (attempt to make a directory a subdirectory of itself)
 *  \return -\c ENAMETOOLONG, if the paths names or any of their components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of both paths, but the last one, are not directories, or
 *                       <tt>oldPath</tt> describes a directory and <tt>newPath</tt>, although it exists, does not
 *  \return -\c EISDIR, if <tt>newPath</tt> describes a directory and <tt>oldPath</tt> does not
 *  \return -\c ELOOP, if either path resolves to more than one symbolic link
 *  \return -\c EMLINK, if <tt>oldPath</tt> is a directory and the directory containing <tt>newPath</tt> has already
 *                      the maximum number of links, or <tt>oldPath</tt> has already the maximum number of links and
 *                      is not contained in the same directory that will contain <tt>newPath</tt>
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>oldPath</tt>, or to any of the
 *                      components of <tt>newPath</tt>, but the last one, is found
 *  \return -\c ENOTEMPTY, if both <tt>oldPath</tt> and <tt>newPath</tt> describe directories and <tt>newPath</tt> is
 *                         not empty
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of both paths, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directories where
 *                     <tt>newPath</tt> entry is to be added and <tt>oldPath</tt> is to be detached
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soRename (const char *oldPath, const char *newPath)
{
  soProbe (76, "soRename (\"%s\", \"%s\")\n", oldPath, newPath);

  /** Variables **/
  int error;
  int newExists = 0;

  uint32_t nInodeOldDir;
  uint32_t nInodeNewDir;
  uint32_t nInodeNewEnt;
  uint32_t nInodeEnt;

  SOInode InodeEnt;
  SOInode InodeNewEnt;

  char auxPath[MAX_PATH + 1];
  char oldDir[MAX_PATH + 1];
  char newDir[MAX_PATH + 1];
  char oldName[MAX_NAME + 1];
  char newName[MAX_NAME + 1];
  char uniqueName[MAX_NAME + 1];

  /** Parameter check **/
  if(oldPath == NULL || newPath == NULL)
    return -EINVAL;
  if((strlen(oldPath) > MAX_PATH) || (strlen(newPath) > MAX_PATH))
    return -ENAMETOOLONG;

  /** Check if paths describe absolute paths **/
  if((strncmp("/", oldPath, 1) != 0) || (strncmp("/", newPath, 1) != 0))
    return -EINVAL;

  /** Get directory and entry names of oldPath **/
  strcpy((char *) auxPath, oldPath);
  strcpy(oldDir, dirname(auxPath));
  strcpy((char *) auxPath, oldPath);
  strcpy(oldName, basename(auxPath));

  /** Get directory and entry names of newPath **/
  strcpy((char *) auxPath, newPath);
  strcpy(newDir, dirname(auxPath));
  strcpy((char *) auxPath, newPath);
  strcpy(newName, basename(auxPath));

  /** Check old and new names length **/
  if((strlen(oldName) > MAX_NAME) || (strlen(newName) > MAX_NAME))
    return -ENAMETOOLONG;

  /** Get old directory and entry inode numbers **/
  if((error = soGetDirEntryByPath(oldDir, NULL, &nInodeOldDir)) != 0)
    return error;
  if((error = soGetDirEntryByName(nInodeOldDir, oldName, &nInodeEnt, NULL)) != 0)
    return error;

  /** Read entry inode **/
  if((error = soReadInode(&InodeEnt, nInodeEnt, IUIN)) != 0)
    return error;

  /** Check if oldPath describes a directory and is a substring of newPath **/
  if((InodeEnt.mode & INODE_TYPE_MASK) == INODE_DIR)
    if(strncmp(oldPath, newPath, strlen(newPath)) == 0)
      return -EINVAL;

  /** Get new directory inode number **/
  if((error = soGetDirEntryByPath(newDir, NULL, &nInodeNewDir)) != 0)
    return error;

  /** Check if newPath exists **/
  if((error = soGetDirEntryByName(nInodeNewDir, newName, &nInodeNewEnt, NULL)) != 0)
  {
    if(error != (-ENOENT))
      return error;
  }
  else
    newExists = 1;

  /** If newPath exists, check if is of the same type as oldPath **/
  if(newExists)
  {
    /* Read inode of the entry that will be removed */
    if((error = soReadInode(&InodeNewEnt, nInodeNewEnt, IUIN)) != 0)
      return error;

    /* Check if oldPath and newPath are the same type */
    if((InodeNewEnt.mode & INODE_TYPE_MASK) != INODE_DIR)
    {
      if((InodeEnt.mode & INODE_TYPE_MASK) == INODE_DIR)
        return -ENOTDIR;
    }
    else
    {
      if((InodeEnt.mode & INODE_TYPE_MASK) != INODE_DIR)
        return -EISDIR;
      /*if both paths describe directories, newPath must be empty*/
      if((error = soCheckDirectoryEmptiness(nInodeNewEnt)) != 0)
        return error;
    }
  }

  /** Check if the process that calls the operation has write permission on the
  directories where newPath entry is to be added and oldPath is to be detached **/
  if((error = soAccessGranted(nInodeOldDir, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }
  if((error = soAccessGranted(nInodeNewDir, W)) != 0)
  {
    if(error == (-EACCES)) return -EPERM;
    else return error;
  }


  /** Rename **/

  /*If newPath exists, needs to be renamed first*/
  if(newExists)
  {
    /*Create unique name*/
    soUniqueName(newName, uniqueName);
    /*Rename newPath entry*/
    if((error = soRenameDirEntry(nInodeNewDir, newName, uniqueName)) != 0)
      return error;
  }

  /*If oldPath and newPath entries are in the same dir. - Rename operation*/
  if(strcmp(oldDir, newDir) == 0)
  {
    /*Rename entry*/
    if((error = soRenameDirEntry (nInodeOldDir, oldName, newName)) != 0)
    {
      if(newExists)
        soRenameDirEntry(nInodeNewDir, uniqueName, newName);
      return error;
    }
  }
  /* Diferent directories - move operation */
  else
  {
    /*Move directory*/
    if((InodeEnt.mode & INODE_TYPE_MASK) == INODE_DIR)
    {
      if((error = soAttachDirectory(nInodeNewDir, newName, nInodeEnt)) != 0)
      {
        if(newExists)
          soRenameDirEntry(nInodeNewDir, uniqueName, newName);
        return error;
      }
    }
    /*Move file*/
    else if((error = soAddDirEntry(nInodeNewDir, newName, nInodeEnt)) != 0)
    {
      if(newExists)
        soRenameDirEntry(nInodeNewDir, uniqueName, newName);
      return error;
    }

    /*Detach entry from old directory*/
    if((error = soDetachDirEntry(nInodeOldDir, oldName)) != 0)
    { /*Undo move operation*/
      soDetachDirEntry(nInodeNewDir, newName);
      if(newExists)
        soRenameDirEntry(nInodeNewDir, uniqueName, newName);
      return error;
    }
  }

  /** Delete newPath, if exists **/
  if(newExists)
    soRemoveDirEntry(nInodeNewDir, uniqueName);

  /** Operation successful **/
  return 0;
}


/**
 *  \brief Create an unique name from the given name.
 *
 *  Auxiliary function to create an unique name. It does the following changes
 *  in the creation process:
 *    - upper case characters are changed into lower case characters
 *    - lower case characters are changed into upper case characters
 *    - characters '.' are changed into '_'
 *    - characters '_' are changed into '.'
 *    - name is reversed
 *
 *  The unique name is created this way so that, in case of undelete operation,
 *  it is possible to retrieve the original name by applying the same process.
 *
 *  \param name original name of the file
 *  \param uniqueName pointer to string where unique name is to be written.
 */

static void soUniqueName(const char *name, char *uniqueName)
{
  /*Auxiliary variables*/
  int32_t idx = 0;
  int32_t idx2 = 0;
  char aux;

  /** Create an unique from the given name **/

  /*Copy name into unique name*/
  strncpy(uniqueName, name, MAX_NAME + 1);

  /*"Transform" into unique name*/
  while (uniqueName[idx] != '\0')
  {
    /*Transform upper case letters into lower case letters*/
    if ((uniqueName[idx] >= 'A') && (uniqueName[idx] <= 'Z'))
      uniqueName[idx] = uniqueName[idx] - 'A' + 'a';
    /*Transform lower case letters into upper case letters*/
    else if ((uniqueName[idx] >= 'a') && (uniqueName[idx] <= 'z'))
      uniqueName[idx] = uniqueName[idx] - 'a' + 'A';
    /*Transfrom '.' into '_'*/
    else if (uniqueName[idx] == '.')
      uniqueName[idx] = '_';
    /*Transfrom '_' into '.'*/
    else if (uniqueName[idx] == '_')
      uniqueName[idx] = '.';
    /*Remaining characters aren't changed*/

    idx++;
  }

  /*Reverse string*/
  idx--;
  while(idx2 < idx)
  {
    aux = uniqueName[idx2];
    uniqueName[idx2] = uniqueName[idx];
    uniqueName[idx] = aux;

    idx--;
    idx2++;
  }
}
