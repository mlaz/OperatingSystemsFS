/**
 *  \file sofs_syscalls_read.c (implementation file for syscall soRead)
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
 *  \brief Read data from an open regular file.
 *
 *  It tries to emulate <em>read</em> system call.
 *
 *  \param ePath path to the file
 *  \param buff pointer to the buffer where data to be read is to be stored
 *  \param count number of bytes to be read
 *  \param pos starting [byte] position in the file data continuum where data is to be read from
 *
 *  \return <em>number of bytes effectively read</em>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c EISDIR, if <tt>ePath</tt> describes a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EFBIG, if the starting [byte] position in the file data continuum assumes a value passing its maximum
 *                     size
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not read permission on the file described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soRead (const char *ePath, void *buff, uint32_t count, int32_t pos)
{
    soProbe (78, "soRead (\"%s\", %p, %u, %u)\n", ePath, buff, count, pos);
  
    int stat, readBytes = 0;
    uint32_t nInodeEnt, firstClst, firstByte, lastClst, lastByte;
    uint32_t size = pos + count;
    char cluster[BSLPC];
    SOInode inode;
    SOSuperBlock * sb;
    
    // Validar parametros
    if (buff == NULL) return -EINVAL;
    
      /* Leitura do Superbloco */
    if((stat = soLoadSuperBlock()) != 0) return stat;
    sb = soGetSuperBlock();
  
    // obter o inode do ficheiro indicado por ePath
    if ( (stat = soGetDirEntryByPath(ePath, NULL, &nInodeEnt)) != 0) return stat;
  
    // leitura do inode
    if ( (stat = soReadInode(&inode, nInodeEnt, IUIN)) != 0) return stat;
    
    // verificação do inode
    if((stat = soQCheckDirCont(sb, &inode)) == 0) return -EISDIR;
    if(stat != -ENOTDIR) return stat;
    
    // verifica se o pos esta fora do ficheiro
    if (pos > inode.size) return -EFBIG;
    
    // Corrige o count se pos+count ultrapassar o limite do ficheiro
    if((pos + count) > inode.size) count = inode.size - pos;
    
    // oter o nCluster e o offset apartir do pos
    if ( (stat = soConvertBPIDC(pos, &firstClst, &firstByte)) != 0) return stat;
  
    // obter o nCluster e offset do final do ficheiro apartir de pos+count
    if ( (stat = soConvertBPIDC(size, &lastClst, &lastByte)) != 0) return stat;
    
    // ler primeiro cluster
    if ( (stat = soReadFileCluster(nInodeEnt, firstClst, &cluster)) != 0) return stat;
    
    // ler para o buffer os dados do primeiro cluster caso seja so p primeiro cluster
    if (firstClst == lastClst)
    {
        memcpy(buff, cluster+ firstByte, lastByte - firstByte);
        return lastByte - firstByte;
    }
    
    // caso seja para ler mais que 1 cluster, ler o resto d primeiro
    memcpy(buff, (cluster+firstByte), (BSLPC - firstByte));
    readBytes = BSLPC - firstByte;
    
    // actualiza o indice do cluster
    firstClst++;
    
    // Ler proximo cluster
    if ( (stat = soReadFileCluster(nInodeEnt, firstClst, cluster)) != 0) return stat;
       
    // ler para o buffer os clusters intermedios
    while (firstClst < lastClst)
    {
        memcpy(buff+readBytes, cluster, BSLPC);
        readBytes += BSLPC;
        
        // proximo cluster
        firstClst++;
        
        // ler o proximo cluster 
        if ( (stat = soReadFileCluster(nInodeEnt, firstClst, cluster)) != 0) return stat;
    }
          
    // ler para o buffer o ultimo cluster
    memcpy(buff+readBytes, cluster, lastByte);
    readBytes += lastByte;
     
    // retorno do numero de bytes lidos
    return readBytes; 
}
