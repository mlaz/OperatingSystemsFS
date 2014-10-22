/**
 *  \file sofs_syscalls_write.c (implementation file for syscall soWrite)
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
 *  \author Rui Neto       42520 T6G2 - November 2011
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
 *  \brief Write data into an open regular file.
 *
 *  It tries to emulate <em>write</em> system call.
 *
 *  \param ePath path to the file
 *  \param buff pointer to the buffer where data to be written is stored
 *  \param count number of bytes to be written
 *  \param pos starting [byte] position in the file data continuum where data is to be written into
 *
 *  \return <em>number of bytes effectively written</em>, on success
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

int soWrite (const char *ePath, void *buff, uint32_t count, int32_t pos)
{
    soProbe (79, "soWrite (\"%s\", %p, %u, %u)\n", ePath, buff, count, pos);

  uint32_t nInodeEnt;
  uint32_t nCluster;	
  uint32_t offset;
  uint32_t nClusterLast;	
  uint32_t offsetLast;
  SOInode inode;
  int stat, i = 0;
  SOSuperBlock * p_sb;
  char c_buff[BSLPC];
  
  /* Verifica o valor do buff e o size */
  if(buff == NULL) return -EINVAL;
  if (pos + count > MAX_FILE_SIZE) return -EFBIG;
  
  /* Leitura do Superbloco */
  if((stat = soLoadSuperBlock()) != 0) return stat;
  p_sb = soGetSuperBlock();
  
  /* Obtem o Inode associado ao ePath e verifica se há erros */
  if((stat = soGetDirEntryByPath(ePath, NULL, &nInodeEnt)) != 0) return stat;
 
  /* Leitura e verificação do inode (tem que ser um ficheiro) */
  if((stat = soReadInode(&inode, nInodeEnt, IUIN)) != 0) return stat;
  if((stat = soQCheckDirCont(p_sb, &inode)) == 0) return -EISDIR;
  if(stat != -ENOTDIR) return stat;
  
  /* Se o count for 0, nada é escrito */
  if(count == 0) return 0;
  
  /* Actualização do size do inode */
  if((pos + count) > inode.size) {
	  inode.size = pos + count;
	  if((stat = soWriteInode(&inode, nInodeEnt, IUIN)) != 0) return stat;
  }
  
  /* Obtenção do clustInd e offset apartir do pos */
  if((stat = soConvertBPIDC(pos, &nCluster, &offset)) != 0) return stat;
  
  /* Obtenção do clustInd e offset do final do ficheiro apartir do pos + count - 1 */
  if((stat = soConvertBPIDC(pos + count - 1, &nClusterLast, &offsetLast)) != 0) return stat;
  
  /* Leitura do 1º cluster a escrever no inode */
  if((stat = soReadFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
  
  /* Se for para escrever apenas num pedaço de um cluster */
  if(nCluster == nClusterLast) {
	  memmove(c_buff + offset, buff, count);
	  
	  /* Escrita do conteúdo no cluster */
	  if((stat = soWriteFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
	  
	  return count; 
  }
  
  /*Caso seja para escrever mais que 1 cluster, escrever o resto do primeiro */
  memmove(c_buff+offset, buff, (BSLPC - offset));

  /* Escrita do cluster no inode */	
  if((stat = soWriteFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
  i = BSLPC - offset;
  
  /* Actualiza o indice do cluster */
  nCluster++;

  /* Lê-se o próximo cluster */
  if((stat = soReadFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
  
  /* Leitura e escrita dos clusters intermédios */ 
  while(nCluster < nClusterLast)
  {			
	memmove(c_buff, buff+i, BSLPC);

	/* Escrita do cluster no inode */	
	if((stat = soWriteFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
	i += BSLPC;

	nCluster++;

    /* Lê-se o próximo cluster */
	if((stat = soReadFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
  }

  /* Caso final em que o cluster do qual vamos escrever é o último */
  memmove(c_buff, buff+i , offsetLast + 1);

  /* Escrita do cluster no inode */	
  if((stat = soWriteFileCluster(nInodeEnt, nCluster, &c_buff))!= 0)return stat;
  i += (offsetLast + 1);
 
  /* Retorno do número de bytes lidos */
  return i;
}
