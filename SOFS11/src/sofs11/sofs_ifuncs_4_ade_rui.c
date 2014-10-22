/**
 *  \file sofs_ifuncs_4_ade.c (implementation file for function soAddDirEntry)
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

#define MAX_HARDLINKS 0xFFFFFFFF

/**
 *  \brief Add an new entry to a directory.
 *
 *  A new entry whose name is <tt>eName</tt> and whose inode number is <tt>nInodeEnt</tt> is added to the
 *  directory associated with the inode whose number is <tt>nInodeDir</tt>. Thus, both inodes must be in use and belong
 *  to a legal type, the former, and to the directory type, the latter.
 *
 *  The <tt>eName</tt> must be a <em>base name</em> and not a <em>path</em>, that is, it can not contain the
 *  character '/'. Besides there should not already be any entry in the directory whose <em>name</em> field is
 *  <tt>eName</tt>.
 *
 *  Whenever the type of the inode associated to the entry to be added is of directory type, the directory is
 *  initialized by setting its contents to represent an empty directory.
 *
 *  The <em>refcount</em> field of the inode associated to the entry to be added and, when required, of the inode
 *  associated to the directory are updated. This may also happen to the <em>size</em> field of either or both inodes.
 *
 *  The process that calls the operation must have write (w) and execution (x) permissions on the directory.
 *
 *  \param nInodeDir number of the inode associated to the directory
 *  \param eName pointer to the string holding the name of the directory entry to be added
 *  \param nInodeEnt number of the inode associated to the directory entry to be added
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the <em>inode numbers</em> are out of range or the pointer to the string is \c NULL
 *                      or the name string does not describe a file name
 *  \return -\c ENAMETOOLONG, if the name string exceeds the maximum allowed length
 *  \return -\c ENOTDIR, if the inode type whose number is <tt>nInodeDir</tt> is not a directory
 *  \return -\c EEXIST, if an entry with the <tt>eName</tt> already exists
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on the directory
 *  \return -\c EPERM, if the process that calls the operation has not write permission on the directory
 *  \return -\c EMLINK, if the maximum number of hardlinks in either one of inodes has already been attained
 *  \return -\c EFBIG, if the directory has already grown to its maximum size
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

int soAddDirEntry (uint32_t nInodeDir, const char *eName, uint32_t nInodeEnt)
{
  soProbe (113, "soAddDirEntry_bin (%"PRIu32", \"%s\", %"PRIu32")\n", nInodeDir, eName, nInodeEnt);
  
  SOSuperBlock *p_sb;
  SOInode inode, inode2;
  SODirEntry de[DPC];
  uint32_t i;
  int stat, ind;
  
  // Leitura do SuperBloco comleitura de erros
  if ( (stat = soLoadSuperBlock()) != 0) return stat;
  p_sb = soGetSuperBlock();
  
  if ( (strpbrk(eName, "/")) != NULL) return -EINVAL;
  
  printf("\n 0");
  
  // Leitura do inodeDir
  if ( (stat = soReadInode(&inode, nInodeDir, IUIN)) != 0) return stat;
  
  // Verifica se inodeDir é mesmo um directorio
  if ( (inode.mode & INODE_TYPE_MASK) != INODE_DIR) return -ENOTDIR;
  
  printf("\tCheckDir");
  // Verificação de consistencia dos direntries do inodeDir
  /*ERROS: ENOTDIR, EDIRINVAL, EDEINVAL, EIUININVAL, ELDCININVAL, EDCINVAL, ELIBAD, EBADF, EIO*/
  if ( (stat = soQCheckDirCont (p_sb, &inode)) != 0)
  {
    printf("\t Error: %u", stat);
    return stat;
  }
    
  printf("\n 1");
  
  
  stat = soGetDirEntryByName(nInodeDir, eName, NULL, &i);
  /* se encontrou um ficheiro com o mesmo nome
   * aqui ja retorna erros em relação ao eName
   */
  if (stat == 0) return -EEXIST; 
  if (stat != (-ENOENT)) return stat;
 
  printf("\n 22");
   
  // Verifica se tem permissoes de execução
  if ( (stat = soAccessGranted(nInodeDir, X)) != 0) return stat;
  // Verifica se tem permissoes de escrita
  if ( (stat = soAccessGranted(nInodeDir, W)) != 0) return -EPERM;

  
  if (inode.refcount >= MAX_HARDLINKS) return -EMLINK;
  
  printf("\n 333");
  
  // Leitura do inodeEnt
  if ( (stat = soReadInode(&inode2, nInodeEnt, IUIN)) != 0) return stat;
  
  printf("\n 4444");
  
  // se inodeEnt for um directorio actualiza as posicoes
  if ( (inode2.mode & INODE_TYPE_MASK) == INODE_DIR)
  {
    
    // criar as entradas default
    de[0].nInode = nInodeEnt;
    bzero(de[0].name, MAX_NAME+1); // garantir que o corpo do nome fica limpo
    de[0].name[0] = '.';  
    de[1].nInode = nInodeDir;
    bzero(de[1].name, MAX_NAME+1);
    de[1].name[0] = '.';
    de[1].name[1] = '.';

    // preenche as entradas nao usadas do directorio
    for (ind=2; ind<DPC; ind++)
    {
        de[ind].nInode = NULL_INODE;
        bzero(de[ind].name, MAX_NAME+1);
    }
    if ( (stat = soWriteFileCluster(nInodeEnt, 0, de)) != 0) return stat;   
    if ( (stat = soReadInode(&inode2, nInodeEnt, IUIN)) != 0) return stat;
    
    // erro era aqui
    inode2.size = sizeof(SODirEntry) * DPC; // actualiza o size
    inode2.refcount++; // "." link para o proprio
    inode.refcount++; // ".." link para o directorio pai
    
    if ( (stat = soWriteInode(&inode, nInodeDir, IUIN)) != 0) return stat;
    if ( (stat = soWriteInode(&inode2, nInodeEnt, IUIN)) != 0) return stat;
  }
  

  if ( (stat = soReadInode(&inode, nInodeDir, IUIN)) != 0) return stat;
  if ( (stat = soReadInode(&inode2, nInodeEnt, IUIN)) != 0) return stat;
  
  printf("\n 55555");
  if ( (i%DPC) == 0)
  {
    if ( (stat = soReadFileCluster(nInodeDir, i/DPC, de)) != 0) return stat;
    
    // criar as entradas default
    de[0].nInode = nInodeEnt;
    strcpy( (char *) de[0].name, eName);
    
    // fazer clean das dirEntries do cluster alocado
    // tinha erro aqui

    for (ind=1; ind<DPC; ind++)
    {
        de[ind].nInode = NULL_INODE;
        bzero(de[ind].name, MAX_NAME+1);
    }

    if ( (stat = soWriteFileCluster(nInodeDir, (i/DPC), de)) != 0) return stat;

    // Load do inodeDir depois de uma escrita
    if ( (stat = soReadInode(&inode, nInodeDir, IUIN)) != 0) return stat;
    
    //Faltava isto
    inode.size += (sizeof(SODirEntry) * DPC);
    printf("\n 666666");
  } else {
    if ( (stat = soReadFileCluster(nInodeDir, i/DPC, de)) != 0) return stat;
    
    strcpy( (char *)de[i%DPC].name, eName);
    de[i%DPC].nInode = nInodeEnt;
    
    if ( (stat = soWriteFileCluster(nInodeDir, i/DPC, de)) != 0) return stat;
    if ( (stat = soReadInode(&inode, nInodeDir, IUIN)) != 0) return stat;
    printf("\n 7777777");
  }
 
  inode2.refcount++;
  if ( (stat = soWriteInode(&inode, nInodeDir, IUIN)) != 0) return stat;
  if ( (stat = soWriteInode(&inode2, nInodeEnt, IUIN)) != 0) return stat;
 
   printf("\nOperation successful\n");
  /* Substitute by your code
     int soAddDirEntry_bin (uint32_t nInodeDir, const char *eName, uint32_t nInodeEnt);
     return soAddDirEntry_bin(nInodeDir, eName, nInodeEnt);*/
  return 0;
}
