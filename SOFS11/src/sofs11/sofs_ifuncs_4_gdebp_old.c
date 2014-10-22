/**
 *  \file sofs_ifuncs_4_gdebp.c (implementation file for function soGetDirEntryByPath)
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
 *  \author Grupo2 P6 - September 2011
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


/*Allusion to internal functions*/
static int soTraversePath(SOSuperBlock *p_sb, char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt);

/**
 *  \brief Get an entry by path.
 *
 *  The directory hierarchy of the file system is traversed to find an entry whose name is the rightmost component of
 *  <tt>ePath</tt>. The path is supposed to be absolute and each component of <tt>ePath</tt>, with the exception of the
 *  rightmost one, should be a directory name or symbolic link name to a path.
 *
 *  The process that calls the operation must have execution (x) permission on all the components of the path with
 *  exception of the rightmost one.
 *
 *  \param ePath pointer to the string holding the name of the path
 *  \param p_nInodeDir pointer to the location where the number of the inode associated to the directory that holds the
 *                     entry is to be stored
 *                     (nothing is stored if \c NULL)
 *  \param p_nInodeEnt pointer to the location where the number of the inode associated to the entry is to be stored
 *                     (nothing is stored if \c NULL)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path or any of the path components exceed the maximum allowed length
 *  \return -\c ERELPATH, if the path is relative and it is not a symbolic link
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EDIRINVAL, if the directory is inconsistent
 *  \return -\c EDEINVAL, if the directory entry is inconsistent
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soGetDirEntryByPath (const char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt)
{
  soProbe (111, "soGetDirEntryByPath (\"%s\", %p, %p)\n", ePath, p_nInodeDir, p_nInodeEnt);

  printf("\nePath = %s\n", ePath);

  /** Variables **/
  int error;
  char absPath[1];
  char auxPath[MAX_PATH + 1];
  char dirPath[MAX_PATH + 1];
  char basePath[MAX_NAME + 1];
  uint32_t nInodeDir;
  uint32_t nInodeEnt;
  SOSuperBlock *sb;

  /** Parameter check **/
  if(ePath == NULL)
    return -EINVAL;
  /** Conformity check **/
  if(strlen(ePath) > MAX_PATH + 1)
    return -ENAMETOOLONG;

  /** Check if ePath is an absolute path **/
  absPath[0] = '/';
  if(ePath[0] != absPath[0])
    return -ERELPATH;

  /** Get basename and dirname of ePath **/
  strcpy((char *)auxPath, ePath);
  strcpy(dirPath, dirname(auxPath));
  strcpy((char *)auxPath, ePath);
  strcpy(basePath, basename(auxPath));

  /** Loading SuperBlock **/
  if((error = soLoadSuperBlock()) != 0)
    return error;
  if((sb = soGetSuperBlock()) == NULL)
    return -EBADF;

  /** Get parent directory inode number **/
  if((error = soTraversePath(sb, dirPath, NULL, &nInodeDir)) != 0)
    return error;

  /** Get entry inode number **/
  /*if((error = soGetDirEntryByName_bin(nInodeDir, basePath, &nInodeEnt, NULL)) != 0)
    return error;*/

  /*Get entry inode number*/
  if(strcmp(basePath, "/") != 0){
    if((error = soGetDirEntryByName(nInodeDir, basePath, &nInodeEnt, NULL)) != 0)
      return error;
  }
  /*Entry directory of root direcrory is "."*/
  else if((error = soGetDirEntryByName(nInodeDir, ".", &nInodeEnt, NULL)) != 0)
    return error;

  /** Update return values **/
  if(p_nInodeDir != NULL)
    *p_nInodeDir = nInodeDir;
  if(p_nInodeEnt != NULL)
    *p_nInodeEnt = nInodeEnt;

  /** Operation successful **/
  return 0;

}


/*Auxiliary function to traverse the parent directory(s)*/
static int soTraversePath(SOSuperBlock *p_sb, char *ePath, uint32_t *p_nInodeDir, uint32_t *p_nInodeEnt){

  /** Variables **/
  int error;
  int entryFound;
  char auxPath[MAX_PATH + 1];
  char dirPath[MAX_PATH + 1];
  char basePath[MAX_NAME + 1];
  SOInode parentDirInode;
  uint32_t nInodeDir;
  uint32_t nInodeEnt;

  /** Get basename and dirname of ePath **/
  strcpy((char *)auxPath, ePath);
  strcpy(dirPath, dirname(auxPath));
  strcpy((char *)auxPath, ePath);
  strcpy(basePath, basename(auxPath));

  entryFound = 0;
  /** Get current dir inode number **/
  if(strcmp(dirPath, "/") == 0){
    /*Parent directory is the ".." entry*/
    if((error = soGetDirEntryByName(0, "..", &nInodeDir, NULL)) != 0)
      return error;

    /*Get entry inode number*/
    if(strcmp(basePath, "/") != 0){
      if((error = soGetDirEntryByName(0, basePath, &nInodeEnt, NULL)) != 0)
        return error;
    }
    /*Entry directory of root direcrory is "."*/
    else if((error = soGetDirEntryByName(0, ".", &nInodeEnt, NULL)) != 0)
      return error;
    /*Update auxiliary variable*/

    entryFound = 1;
  }
  else if((error = soTraversePath(p_sb, dirPath, NULL, &nInodeDir)) != 0)
    return error;


  /** Read Parent directory inode **/
  if((error = soReadInode(&parentDirInode, nInodeDir, IUIN)) != 0)
    return error;

  /** Check if inode of parent directory indeed refers to a directory **/
  if((parentDirInode.mode & INODE_DIR) != INODE_DIR)
    return -ENOTDIR;

  /** Check parent directory consistency **/
  if((error = soQCheckDirCont(p_sb, &parentDirInode)) != 0)
    return error;

  /** Check execution permission on parent directory **/
  if((error = soAccessGranted(nInodeDir, X)) != 0)
    return error;

  /** Get Entry inode number if needed**/
  if(entryFound == 0){
    if((error = soGetDirEntryByName(nInodeDir, basePath, &nInodeEnt, NULL)) != 0)
      return error;
  }

  /** Update return values **/
  if(p_nInodeDir != NULL)
    *p_nInodeDir = nInodeDir;
  if(p_nInodeEnt != NULL)
    *p_nInodeEnt = nInodeEnt;

  /** Traverse successful **/
  return 0;
}
