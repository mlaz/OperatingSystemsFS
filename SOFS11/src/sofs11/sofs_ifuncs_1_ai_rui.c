/**
 *  \file sofs_ifuncs_1_ai.c (implementation file of function soAllocInode)
 *
 *  \author Artur Carneiro Pereira - September 2008
 *  \author António Rui Borges - September 2010 / September 2011
 *  \author Miguel Oliveira e Silva - September 2011
 *  \author Rui André daFonseca Neto - #mec:42520 - P06
 *
 *  \remarks In case an error occurs, all functions return a negative value which is the symmetric of the system error
 *           or the local error that better represents the error cause. Local errors are out of the range of the
 *           system errors.
 */

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"


/** binary implementation prototype */
int soAllocInode_bin (uint32_t type, uint32_t* p_nInode);

/**
 *  \brief Allocate a free inode.
 *
 *  The inode is retrieved from the list of free inodes, marked in use, associated to the legal file type passed as
 *  a parameter and generally initialized. It must be free and if is free in the dirty state, it has to be cleaned
 *  first.
 *
 *  Upon initialization, the new inode has:
 *     \li the field mode set to the given type, while the free flag and the permissions are reset
 *     \li the owner and group fields set to current userid and groupid
 *     \li the <em>prev</em> and <em>next</em> fields, pointers in the double-linked list of free inodes, change their
 *         meaning: they are replaced by the <em>time of last file modification</em> and <em>time of last file
 *         access</em> which are set to current time
 *     \li the reference fields set to NULL_CLUSTER
 *     \li all other fields reset.

 *  \param type the inode type (it must represent either a file, or a directory, or a symbolic link)
 *  \param p_nInode pointer to the location where the number of the just allocated inode is to be stored
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>type</em> is illegal or the <em>pointer to inode number</em> is \c NULL
 *  \return -\c ENOSPC, if the list of free inodes is empty
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -\c EFININVAL, if the free inode is inconsistent
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soAllocInode (uint32_t type, uint32_t* p_nInode)
{
   soProbe (411, "soAllocInode (%"PRIu32", %p)\n", type, p_nInode);

   uint32_t nBlk, offset;

   int error, i;
   SOSuperBlock *sb; 	

   if(type != INODE_SYMLINK && type != INODE_FILE && type != INODE_DIR) { return -EINVAL; }

   if(p_nInode == NULL) { return -EINVAL; }

   /***** Load do SuperBloco *****/	
   if ( (error = soLoadSuperBlock()) != 0) {  return error; } 	
   sb = soGetSuperBlock();
   
   if((error = soQCheckInT(sb)) != 0) return error;

   /***** verificar se existem Inodes livres *****/
   if (sb->ifree == 0) { return -ENOSPC; }
   
   /* Devolve o número do Inode que vai ser alocado */
   *p_nInode = sb->ihead;

   /***** obter o numero do bloco com o 1º Inode livre e o offset correspondente *****/
   if((error = soConvertRefInT(sb->ihead, &nBlk, &offset)) != 0){ return error; }

   /***** Load do Inode *****/
   error = soLoadBlockInT(nBlk);
   if (error) { return error; }
   SOInode* pFreeInode = soGetBlockInT();
   
    
   if((error = soQCheckFInode(&pFreeInode[offset])) != 0){ return error; }
   
   uint32_t nextInodeN = (pFreeInode[offset].vD1).next;

   /***** Actualizar o Super-Nó *****/

   sb->ihead = nextInodeN; /* À cabeça fica o proximo Inode livre da lista */	

    if(sb->ifree == 1){
        sb->ihead = NULL_INODE;
        sb->itail = NULL_INODE;
    }
    else
        sb->ihead = (pFreeInode[offset].vD1).next;
   
   /***** Inicializar o Inode *****/
   pFreeInode[offset].mode = (uint16_t) type;
   pFreeInode[offset].refcount = 0;
   pFreeInode[offset].owner = getuid();
   pFreeInode[offset].group = getgid();
   pFreeInode[offset].size = 0;
   pFreeInode[offset].clucount = 0;
   (pFreeInode[offset].vD1).atime = time(NULL);
   (pFreeInode[offset].vD2).mtime = time(NULL);
   
   for (i = 0; i < N_DIRECT; i++) { pFreeInode[offset].d[i] = NULL_INODE; }
   pFreeInode[offset].i1 = NULL_INODE;
   pFreeInode[offset].i2 = NULL_INODE;
         
   if ((error = soStoreBlockInT()) != 0) { return error; }
   
   /*Consistency check*/
  if((error = soQCheckInodeIU(sb, &pFreeInode[offset])) != 0)
    return error;
   
    /***** Actualizar a lista de Inodes livres (se houverem inodes livres) *****/
   if (nextInodeN != NULL_INODE)
   {
      if ((error = soConvertRefInT(nextInodeN, &nBlk, &offset)) != 0) { return error; }
      
      if ((error = soLoadBlockInT(nBlk)) != 0) { return error; }
      
      SOInode *nextIn = soGetBlockInT();
      (nextIn[offset].vD2).prev = NULL_INODE; 
      
      if ((error = soStoreBlockInT()) != 0) { return error; }
   }
   sb->ifree--; /* Decrementar o numero do Inodes livres */
   if ((error = soStoreSuperBlock()) != 0) { return error; }
   
   return 0;
}

