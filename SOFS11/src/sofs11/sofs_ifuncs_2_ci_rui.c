/**
 *  \file sofs_ifuncs_2_ci.c (implementation file for function soCleanInode)
 *
 *  \brief Set of operations to manage inodes: level 2 of the internal file system organization.
 *
 *         The aim is to provide an unique description of the functions that operate at this level.
 *
 *  The operations are:
 *      \li read specific inode data from the table of inodes
 *      \li write specific inode data to the table of inodes
 *      \li clean an inode
 *      \li check the inode access permissions against a given operation.
 *
 *  \author Artur Carneiro Pereira September 2008
 *  \author Miguel Oliveira e Silva September 2009
 *  \author António Rui Borges - September 2010 / September 2011
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
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"


/**
 *  \brief Clean an inode.
 *
 *  The inode must be free in the dirty state.
 *  The inode is supposed to be associated to a file, a directory, or a symbolic link which was previously deleted.
 *
 *  This function cleans the list of data cluster references.
 *
 *  Notice that the inode 0, supposed to belong to the file system root directory, can not be cleaned.
 *
 *  \param nInode number of the inode
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> is out of range
 *  \return -\c EFDININVAL, if the free inode in the dirty state is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c EWGINODENB, if the <em>inode number</em> in the data cluster <tt>status</tt> field is different from the
 *                          provided <em>inode number</em> (FREE AND CLEAN / CLEAN)
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soCleanInode (uint32_t nInode)
{
    soProbe (313, "soCleanInode (%"PRIu32")\n", nInode);
    
    int error;
    uint32_t offset, nBlk, i, j;
    SOSuperBlock* sb;
    SOInode* inode;
    SODataClust dataClt, refDataClt;

    // Load of SuperBlock
    if((error = soLoadSuperBlock()) != 0) { return error; }
    if((sb = soGetSuperBlock()) == NULL) { return -EBADF; }

    // quick check of the inode number is out of range
    if((nInode == 0) || (nInode >= sb->itotal)) { return -EINVAL; }

    // Load of inode
    if((error = soConvertRefInT (nInode, &nBlk, &offset)) != 0) { return error; }
    if((error = soLoadBlockInT(nBlk)) != 0) { return error; }
    inode = soGetBlockInT();

    // Check of the free inode inthe dirty state
    if((error = soQCheckFDInode(sb, &inode[offset])) != 0) { return error; }
    
    
    /** LIMPEZA **/
      
    // referencias directas
    for (i = 0; i < N_DIRECT; i++)
    {
        inode[offset].d[i] = NULL_CLUSTER;
    }


    //se o inode tiver referencias simplesmente indirectas
    if((inode[offset].i1) != NULL_CLUSTER) 
    {     
        //faz load do cluster que possui a tabela de referencias directas
        if((error = soLoadDirRefClust(inode[offset].i1)) != 0) { return error; }

        dataClt = *soGetDirRefClust(); //armazena o conteudo do cluster numa variavel temporaria

        for(i = 0; i < RPC; i++)
        {
            if (dataClt.info.ref[i] != NULL_CLUSTER) //para cada entrada da continuaçao da tabela de referencias directas, se a entrada n for nula
            {
                dataClt.info.ref[i] = NULL_CLUSTER;
            }
        }
        if((error = soStoreDirRefClust()) != 0) { return error; }     
    }


    if((inode[offset].i2) != NULL_CLUSTER) //se o inode tiver referencias duplamente indirectas
    {    
        if(soLoadSngIndRefClust(inode[offset].i2) != 0) //faz load do cluster que possui a tabela de referencias simplesmente indirectas
            return -EIO;

        dataClt = *soGetSngIndRefClust(); //armazena o conteudo do cluster numa variavel temporaria

        for(j = 0; j < RPC; j++)
        {
            if (dataClt.info.ref[j] != NULL_CLUSTER) //para cada entrada da tabela de referencias simplesmente indirectas, se a entrada n for nula
            {
                if(soLoadDirRefClust(dataClt.info.ref[j]) != 0) //faz o load do cluster que contem a continuaçao da tabela de referencias directas
                    return -EIO;

                refDataClt = *soGetDirRefClust(); //armazena o conteudo do cluster numa variavel temporaria

                for(i = 0; i < RPC; i++)
                {
                    if (refDataClt.info.ref[i] != NULL_CLUSTER) //para cada entrada da continuaçao da tabela de referencias directas, se a entrada n for nula
                    {
                        refDataClt.info.ref[i] = NULL_CLUSTER;
                    } 
                }
                if((error = soStoreDirRefClust()) != 0) { return error; } 
            }
        }
        if((error = soStoreSngIndRefClust()) != 0) { return error; } 
    }
    
    inode[offset].i1 = NULL_CLUSTER;
    inode[offset].i2 = NULL_CLUSTER;

    inode[offset].mode = INODE_FREE; //poe todos os bits do mode do inode a "0" excepto o bit 12 (que indica que o inode esta livre e limpo) 

    //volta a guardar o bloco no sistema
    if ((error = soStoreBlockInT()) != 0) { return error; }
    if ((error = soStoreSuperBlock()) != 0) { return error; }

    return 0;
}
