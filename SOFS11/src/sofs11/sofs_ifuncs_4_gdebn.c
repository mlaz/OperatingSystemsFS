/**
 *  \file sofs_ifuncs_4_gdebn.c (implementation file for function soGetDirEntryByName)
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
 *  \brief Get an entry by name.
 *
 *  The directory contents, seen as an array of direntries, is parsed to find an entry whose name is <tt>eName</tt>.
 *  Thus, the inode associated to the directory must be in use and belong to the directory type.
 *
 *  The <tt>eName</tt> must also be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'.
 *
 *  The process that calls the operation must have execution (x) permission on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be located
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the directory entry whose
 *                     name is passed, is to be stored
 *                     (nothing is stored if \c NULL)
 *  \param p_idx pointer to the location where the index to the directory entry whose name is passed, or the index of
 *               the first entry that is free and is in the clean state, is to be stored
 *               (nothing is stored if \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range or the pointer to the string is \c NULL or the
 *                      name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type is not a directory
 *  \return -\c ENOENT,  if no entry with <tt>name</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
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


int soGetDirEntryByName (uint32_t nInodeDir, const char *eName, uint32_t *p_nInodeEnt, uint32_t *p_idx)
{
  soProbe (112, "soGetDirEntryByName (%"PRIu32", \"%s\", %p, %"PRIu32")\n", nInodeDir, eName, p_nInodeEnt, p_idx);

  //  return soGetDirEntryByName_bin (nInodeDir, eName, p_nInodeEnt,  p_idx);

  uint32_t status;
  char name[MAX_NAME+1];
  SOInode inode;

  uint32_t total_clts;
  uint32_t frstFCDE;

  uint32_t clt_idx;
  SODirEntry deTable[DPC];
  uint32_t dentry_idx;
  uint32_t abs_dentry_idx;

  /* Parameter validation */
  if (eName == NULL)
    return -EINVAL;

  if (strlen(eName) > MAX_NAME)
    return -ENAMETOOLONG;

  /* Check if eName contains '/' */
  if ( strchr(eName, '/') != NULL )
    return -EINVAL;
  
  strcpy(name, eName);
  if ( strcmp(eName, basename(name)) )
    return -EINVAL;

  /* Checking if user has execute permission */
  if ( (status = soAccessGranted(nInodeDir, X)) != 0)
    return status;

  /* Reading the directory inode */
  if ( (status = soReadInode(&inode, nInodeDir, IUIN)) != 0 )
       return status;

  /* Checking if the inode type is a directory */
  if ((inode.mode & INODE_DIR) != INODE_DIR)
    return -ENOTDIR;

  strcpy(name, eName);

  total_clts = (inode.size / (DPC * sizeof(SODirEntry)));

  frstFCDE = 0;
  clt_idx = 0;
  abs_dentry_idx = 0;
  while (clt_idx < total_clts) {

    if ( (status = soReadFileCluster(nInodeDir, clt_idx, &deTable)) != 0 )
      return status;

    dentry_idx = 0;
    while (dentry_idx < DPC) {

      if (deTable[dentry_idx].nInode == NULL_INODE) {
        if (!frstFCDE)
          frstFCDE = abs_dentry_idx;

      } else {

        if (!strcmp((char*) deTable[dentry_idx].name, name)) {

          if (p_nInodeEnt != NULL)
            *p_nInodeEnt = deTable[dentry_idx].nInode;

          if (p_idx != NULL)
            *p_idx = abs_dentry_idx;

          return 0;
        }
      }

      dentry_idx += 1;
      abs_dentry_idx += 1;
    }
    clt_idx += 1;
  }

  if (p_idx != NULL) {
    if (frstFCDE)
      *p_idx = frstFCDE;
    else
      *p_idx = abs_dentry_idx;
  }
  return -ENOENT;
}
