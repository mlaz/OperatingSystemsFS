/**
 *  \file mkfs_sofs11.c (implementation file)
 *
 *  \brief The SOFS11 formatting tool.
 *
 *  It stores in predefined blocks of the storage device the file system
 *  metadata. With it, the storage device may be
 *  envisaged operationally as an implementation of SOFS11.
 *
 *  The following data structures are created and initialized:
 *     \li the superblock
 *     \li the table of inodes
 *     \li the data zone
 *     \li the contents of the root directory seen as empty.
 *
 *  SINOPSIS:
 *  <P><PRE>                mkfs_sofs11 [OPTIONS] supp-file
 *
 *                OPTIONS:
 *                 -n name --- set volume name (default: "SOFS10")
 *                 -i num  --- set number of inodes (default: N/8, where N = number of blocks)
 *                 -z      --- set zero mode (default: not zero)
 *                 -q      --- set quiet mode (default: not quiet)
 *                 -h      --- print this help.</PRE>
 *
 *  \author Artur Carneiro Pereira - September 2008
 *  \author Miguel Oliveira e Silva - September 2009
 *  \author António Rui Borges - September 2010 - August 2011
 *  \author Grupo 2 P6 - September 2011
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "sofs_const.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_direntry.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"

/* Allusion to internal functions */

static int fillInSuperBlock (SOSuperBlock *p_sb, uint32_t ntotal, uint32_t itotal, uint32_t nclusttotal,
                             unsigned char *name);
static int fillInINT (SOSuperBlock *p_sb);
static int fillInRootDir (SOSuperBlock *p_sb);
static int fillInGenRep (SOSuperBlock *p_sb, int zero);
static int checkFSConsist (void);
static void printUsage (char *cmd_name);
static void printError (int errcode, char *cmd_name);

/* The main function */

int main (int argc, char *argv[])
{
  char *name = "SOFS11";                         /* volume name */
  uint32_t itotal = 0;                           /* total number of inodes, if kept set value automatically */
  int quiet = 0;                                 /* quiet mode, if kept set not quiet mode */
  int zero = 0;                                  /* zero mode, if kept set not zero mode */

  /* process command line options */

  int opt;                                       /* selected option */

  do
    { switch ((opt = getopt (argc, argv, "n:i:qzh")))
        { case 'n': /* volume name */
            name = optarg;
            break;
        case 'i': /* total number of inodes */
          if (atoi (optarg) < 0)
            { fprintf (stderr, "%s: Negative inodes number.\n", basename (argv[0]));
              printUsage (basename (argv[0]));
              return EXIT_FAILURE;
            }
          itotal = (uint32_t) atoi (optarg);
          break;
        case 'q': /* quiet mode */
          quiet = 1;                       /* set quiet mode for processing: no messages are issued */
          break;
        case 'z': /* zero mode */
          zero = 1;                        /* set zero mode for processing: the information content of all free
                                              data clusters are set to zero */
          break;
        case 'h': /* help mode */
          printUsage (basename (argv[0]));
          return EXIT_SUCCESS;
        case -1:  break;
        default:  fprintf (stderr, "%s: Wrong option.\n", basename (argv[0]));
          printUsage (basename (argv[0]));
          return EXIT_FAILURE;
        }
    } while (opt != -1);
  if ((argc - optind) != 1)                      /* check existence of mandatory argument: storage device name */
    { fprintf (stderr, "%s: Wrong number of mandatory arguments.\n", basename (argv[0]));
      printUsage (basename (argv[0]));
      return EXIT_FAILURE;
    }

  /* check for storage device conformity */

  char *devname;                                 /* path to the storage device in the Linux file system */
  struct stat st;                                /* file attributes */

  devname = argv[optind];
  if (stat (devname, &st) == -1)                 /* get file attributes */
    { printError (-errno, basename (argv[0]));
      return EXIT_FAILURE;
    }

  if (st.st_size % BLOCK_SIZE != 0)              /* check file size: the storage device must have a size in bytes
                                                    multiple of block size */
    { fprintf (stderr, "%s: Bad size of support file.\n", basename (argv[0]));
      return EXIT_FAILURE;
    }

  /* evaluating the file system architecture parameters
   * full occupation of the storage device when seen as an array of blocks supposes the equation bellow
   *
   *    NTBlk = 1 + NBlkTIN + NTClt*BLOCKS_PER_CLUSTER
   *
   *    where NTBlk means total number of blocks
   *          NTClt means total number of clusters of the data zone
   *          NBlkTIN means total number of blocks required to store the inode table
   *          BLOCKS_PER_CLUSTER means number of blocks which fit in a cluster
   *
   * has integer solutions
   * this is not always true, so a final adjustment may be made to the parameter NBlkTIN to warrant this
   */

  uint32_t ntotal;                               /* total number of blocks */
  uint32_t iblktotal;                            /* number of blocks of the inode table */
  uint32_t nclusttotal;                          /* total number of clusters */

  ntotal = st.st_size / BLOCK_SIZE;
  if (itotal == 0) itotal = ntotal >> 3;
  if ((itotal % IPB) == 0)
    iblktotal = itotal / IPB;
  else iblktotal = itotal / IPB + 1;
  nclusttotal = (ntotal - 1 - iblktotal) / BLOCKS_PER_CLUSTER;
  /* final adjustment */
  iblktotal = ntotal - 1 - nclusttotal * BLOCKS_PER_CLUSTER;
  itotal = iblktotal * IPB;

  /* formatting of the storage device is going to start */

  SOSuperBlock *p_sb;                            /* pointer to the superblock */
  int status;                                    /* status of operation */

  if (!quiet)
    printf("\e[34mInstalling a %"PRIu32"-inodes SOFS11 file system in %s.\e[0m\n", itotal, argv[optind]);

  /* open a buffered communication channel with the storage device */

  if ((status = soOpenBufferCache (argv[optind], BUF)) != 0)
    { printError (status, basename (argv[0]));
      return EXIT_FAILURE;
    }

  /* read the contents of the superblock to the internal storage area
   * this operation only serves at present time to get a pointer to the superblock storage area in main memory
   */

  if ((status = soLoadSuperBlock ()) != 0)
    return status;
  p_sb = soGetSuperBlock ();

  /* filling in the superblock fields:
   *   magic number should be set presently to 0xFFFF, this enables that if something goes wrong during formating, the
   *   device can never be mounted later on
   */

  if (!quiet)
    { printf ("Filling in the superblock fields ... ");
      fflush (stdout);                          /* make sure the message is printed now */
    }

  if ((status = fillInSuperBlock (p_sb, ntotal, itotal, nclusttotal, (unsigned char *) name)) != 0)
    { printError (status, basename (argv[0]));
      soCloseBufferCache ();
      return EXIT_FAILURE;
    }

  if (!quiet) printf ("done.\n");

  /* filling in the inode table:
   *   only inode 0 is in use (it describes the root directory)
   */

  if (!quiet)
    { printf ("Filling in the inode table ... ");
      fflush (stdout);                          /* make sure the message is printed now */
    }

  if ((status = fillInINT (p_sb)) != 0)
    { printError (status, basename (argv[0]));
      soCloseBufferCache ();
      return EXIT_FAILURE;
    }

  if (!quiet) printf ("done.\n");

  /* filling in the contents of the root directory:
   *   the first 2 entries are filled in with "." and ".." references
   *   the other entries are kept empty
   */

  if (!quiet)
    { printf ("Filling in the contents of the root directory ... ");
      fflush (stdout);                          /* make sure the message is printed now */
    }

  if ((status = fillInRootDir (p_sb)) != 0)
    { printError (status, basename (argv[0]));
      soCloseBufferCache ();
      return EXIT_FAILURE;
    }

  if (!quiet) printf ("done.\n");
  /*
   * create the general repository of free data clusters as a double-linked list where the data clusters themselves are
   * used as nodes
   * zero fill the remaining data clusters if full formating was required:
   * zero mode was selected
   */

  if (!quiet)
    { printf ("Creating the general repository of free data clusters ... ");
      fflush (stdout); /* make sure the message is printed now */
    }

  if ((status = fillInGenRep (p_sb, zero)) != 0)
    { printError (status, basename (argv[0]));
      soCloseBufferCache ();
      return EXIT_FAILURE;
    }


  /* magic number should now be set to the right value before writing the contents of the superblock to the storage
     device */
  p_sb->magic = MAGIC_NUMBER;

  if ((status = soStoreSuperBlock ()) != 0)
    return status;

  if (!quiet) printf ("done.\n");

  /* check the consistency of the file system metadata */

  if (!quiet)
    { printf ("Checking file system metadata... ");
      fflush (stdout);                          /* make sure the message is printed now */
    }

  if ((status = checkFSConsist ()) != 0)
    { fprintf(stderr, "error # %d - %s\n", -status, soGetErrorMessage (p_sb, -status));
      soCloseBufferCache ();
      return EXIT_FAILURE;
    }

  if (!quiet) printf ("done.\n");

  /* close the unbuffered communication channel with the storage device */

  if ((status = soCloseBufferCache ()) != 0)
    { printError (status, basename (argv[0]));
      return EXIT_FAILURE;
    }

  /* that's all */

  if (!quiet) printf ("Formating concluded.\n");

  return EXIT_SUCCESS;

} /* end of main */

/*
 * print help message
 */

static void printUsage (char *cmd_name)
{
  printf ("Sinopsis: %s [OPTIONS] supp-file\n"
          "  OPTIONS:\n"
          "  -n name --- set volume name (default: \"SOFS11\")\n"
          "  -i num  --- set number of inodes (default: N/8, where N = number of blocks)\n"
          "  -z      --- set zero mode (default: not zero)\n"
          "  -q      --- set quiet mode (default: not quiet)\n"
          "  -h      --- print this help\n", cmd_name);
}

/*
 * print error message
 */

static void printError (int errcode, char *cmd_name)
{
  fprintf(stderr, "%s: error #%d - %s\n", cmd_name, -errcode, strerror (-errcode));
}

/* filling in the superblock fields:
 *   magic number should be set presently to 0xFFFF, this enables that if something goes wrong during formating, the
 *   device can never be mounted later on
 */

static int fillInSuperBlock (SOSuperBlock *p_sb, uint32_t ntotal, uint32_t itotal, uint32_t nclusttotal,
                             unsigned char *name)
{
  int i, status;					/*Increment and status variables*/

  /** Parameter check **/
  if((p_sb == NULL) || (name == NULL))
    return -1;
  if((ntotal < 0) || (itotal < 0) || (nclusttotal < 0))
    return -1;

  /**Filling Header Data **/
  p_sb->magic = 0xFFFF;						/*file system identification number - presently set to 0xFFFF*/
  p_sb->version = VERSION_NUMBER;				/*version number*/
  strncpy((char*)p_sb->name, (char*) name, MAX_NAME + 1);	/*volume name*/
  p_sb->ntotal = ntotal;					/*total number of blocks in the device*/
  p_sb->mstat = PRU;						/*flag signaling if the file system was properly unmounted*/


  /** Filling Inode Table Data **/
  p_sb->itable_start = 1;             /* Inode table starts in block 1 (block
                                         0 is superblock) */
  p_sb->itable_size = itotal / IPB;   /* number of blocks that the table of
                                         inodes comprises */
  p_sb->itotal = itotal;	      /* total number of inodes */
  p_sb->ifree = itotal - 1;           /* number of free inodes */
  p_sb->ihead = 1;	              /* index of the inode array's first
                                         element */
  p_sb->itail = itotal - 1;           /* index of the inode array's last
                                         element */

  /** Filling Data Zone Data **/
  p_sb->dzone_start = p_sb->itable_start + p_sb->itable_size;	/*physical number of the block where the data zone starts*/
  p_sb->dzone_total = nclusttotal;				/*total number of data clusters*/
  p_sb->dzone_free = nclusttotal - 1;				/*number of free data clusters - only 1 cluster in use by root directory*/

  /*Retrieval Cache*/
  p_sb->dzone_retriev.cache_idx = DZONE_CACHE_SIZE;		/*Retrieval cache is empty -> index = DZONE_CACHE_SIZE*/
  for(i = 0; i < DZONE_CACHE_SIZE; i++)
    p_sb->dzone_retriev.cache[i] = NULL_CLUSTER;

  /*Insertion Cache*/
  p_sb->dzone_insert.cache_idx = 0;				/*Insertion cache is empty -> index = 0*/
  for(i = 0; i < DZONE_CACHE_SIZE; i++)
    p_sb->dzone_insert.cache[i] = NULL_CLUSTER;

  p_sb->dhead = 1;
  p_sb->dtail = nclusttotal - 1;

  /** Write SuperBlock information in block 0 **/
  if((status = soWriteCacheBlock(0, p_sb)) < 0)
    return status;

  /** Operation successful **/
  return 0;
}

/*
 * filling in the inode table:
 *   only inode 0 is in use (it describes the root directory)
 */

static int fillInINT (SOSuperBlock *p_sb)
{
  int i, j, k, status;						/*Increment and status variables*/
  uint32_t freeInode;						/*Auxiliary variable to store the number of the free inodes*/
  SOInode inode[IPB];						/*Array that represents de number of inodes in 1 block.*/

  /** Parameter check **/
  if(p_sb == NULL)
    return -1;

  memset(inode, 0, BLOCK_SIZE);					/*Initialize array*/

  /** Filling Inode Table 1st block **/
  /*Inode 0 - root directory*/
  inode[0].mode = INODE_DIR | 					/*mode: it stores the file type and its access permissions;*/
    INODE_RD_USR | INODE_WR_USR | INODE_EX_USR |		/*root directory has all access permissions at this moment*/
    INODE_RD_GRP | INODE_WR_GRP | INODE_EX_GRP |
    INODE_RD_OTH | INODE_WR_OTH | INODE_EX_OTH;
  inode[0].refcount = 2;					/*refcount: number of directory entries associated to the inode ("." and "..")*/
  inode[0].owner = getuid();					/*owner: user ID of the file owner*/
  inode[0].group = getgid();					/*group: group ID of the file owner*/
  inode[0].size = sizeof(SODirEntry) * DPC;			/*size: file size in bytes*/
  inode[0].clucount = 1;					/*clucount: cluster count - total number of data clusters attached to the file*/
  inode[0].vD1.atime = time(NULL);				/*Inode in use - atime: if the inode is in use, time of last file access*/
  inode[0].vD2.mtime = inode[0].vD1.atime;			/*Inode in use - mtime: if the inode is in use, time of last file modification*/

  inode[0].d[0] = 0;						/*d: direct references to the data the clusters that comprise the file information content*/
								/*   inode 0 only uses 1 cluster, only 1st direct reference is used*/

  for(i = 1; i < N_DIRECT; i ++)				/*Direct References*/
    inode[0].d[i] = NULL_CLUSTER;
  inode[0].i1 = NULL_CLUSTER;					/*reference to the data cluster that holds the next group of direct references*/
  inode[0].i2 = NULL_CLUSTER;					/*reference do the data cluster that holds an array of indirect references*/
  /*Free Inodes*/
  freeInode = 1;						/*Inode 1 is the first free I-node*/
  for(j = 1; j < IPB; j++){
    inode[j].mode = INODE_FREE;					/*Free inode*/
    inode[j].vD1.next = freeInode + 1;				/*next inode in the linked list of that reference the free inodes*/

    if(j == 1)
      inode[j].vD2.prev = NULL_INODE;				/*Inode 1 doesn't have a previous Inode since he is the first Inode of the free Inode list*/
    else
      inode[j].vD2.prev = freeInode - 1;

    for(i = 0; i < N_DIRECT; i++)
      inode[j].d[i] = NULL_CLUSTER;				/*Direct references*/
    inode[j].i1 = NULL_CLUSTER;					/*Indirect references*/
    inode[j].i2 = NULL_CLUSTER;					/*Double indirect*/

    freeInode++;						/*Update inode number*/
  }
  /*Check table size*/
  if(p_sb->itable_size == 1)					/*If i-node table only has 1 block (8 i-nodes) the last inode points to NULL*/
    inode[IPB - 1].vD1.next = NULL_INODE;
  /*Write 1st block data*/
  if((status  = soWriteCacheBlock(p_sb->itable_start, inode)) < 0)
    return status;

  /** Filling Inode table remaining blocks - all inodes are free at this point **/
  for(i = p_sb->itable_start + 1; i < p_sb->dzone_start; i++){
    /*clear previous block information*/
    memset(inode, 0, BLOCK_SIZE);

    /*Update the information of each inode in this block*/
    for(j = 0; j < IPB; j++){
      inode[j].mode = INODE_FREE;
      inode[j].vD1.next = freeInode + 1;
      inode[j].vD2.prev = freeInode - 1;

      for(k = 0; k < N_DIRECT; k++)
        inode[j].d[k] = NULL_CLUSTER;

      inode[j].i1 = NULL_CLUSTER;
      inode[j].i2 = NULL_CLUSTER;

      freeInode++;						/*Update inode number*/
    }

    if(i == p_sb->dzone_start - 1)
      inode[IPB - 1].vD1.next = NULL_INODE;			/*if this is the last block, then the last inode.next points to NULL_INODE*/

    /*Write block data*/
    if((status = soWriteCacheBlock(i, inode)) < 0)
      return status;
  }

  /*Operation successful*/
  return 0;
}

/*
 * filling in the contents of the root directory:
 the first 2 entries are filled in with "." and ".." references
 the other entries are empty
*/

static int fillInRootDir (SOSuperBlock *p_sb)
{
  int i, status;							/*Increment and status variables*/
  SODataClust RootCluster;						/*RootCluster - data cluster to be written*/

  /** Parameter check **/
  if(p_sb == NULL)
    return -1;

  memset(&RootCluster, 0, CLUSTER_SIZE);

  /** Filling data cluster Header **/
  RootCluster.prev = NULL_CLUSTER;					/*Data cluster in use*/
  RootCluster.next = NULL_CLUSTER;					/*Data cluster in use*/
  RootCluster.stat = 0;							/*I-node 0 is root directory's inode*/

  /** Filling data cluster Body **/

  /*First directory entry = "."*/
  RootCluster.info.de[0].nInode = 0;					/*Associated inode number*/
  strcpy((char *) RootCluster.info.de[0].name, ".");

  /*Second directory entry = ".."*/
  RootCluster.info.de[1].nInode = 0;
  strcpy((char *) RootCluster.info.de[1].name, "..");

  /*Remaining directory entries*/
  for(i = 2; i < DPC; i++){
    RootCluster.info.de[i].nInode = NULL_INODE;
    strncpy((char *) RootCluster.info.de[i].name, "\0", MAX_NAME + 1);	/*Remaining directory entries are empty*/
  }

  /** Write First Cluster information **/
  if((status = soWriteCacheCluster(p_sb->dzone_start, &RootCluster)) < 0)
    return status;

  /** Operation successful **/
  return 0;
}

/*
 * create the general repository of free data clusters as a double-linked list where the data clusters themselves are
 * used as nodes
 * zero fill the remaining data clusters if full formating was required:
 *   zero mode was selected
 */


/*
  NFisCluster = dzone_start + NLogCluster *BLOCKS_PER_CLUSTER;
*/
static int fillInGenRep (SOSuperBlock *p_sb, int zero)
{
  int status; /* status variable for error checking*/

  SODataClust currCluster; /* current cluster's */

  uint32_t currPhysNum; /* pysical number where the current cluster
                           is going to be written */

  uint32_t currLogNum; /* current cluster's logical number */

  uint32_t lastCluster; /* last cluster's physical number */

  /** Parameter check **/
  if (p_sb == NULL)
    return -1;

  currLogNum = 1;

  lastCluster = p_sb->dzone_start + (p_sb->dzone_total * BLOCKS_PER_CLUSTER) - BLOCKS_PER_CLUSTER;

  if (zero)
    memset(&currCluster, 0, CLUSTER_SIZE);

  /** Filling general repository **/

  currCluster.prev = NULL_CLUSTER;      /*First cluster of the double-linked list doesn't have previous*/
  currCluster.next = currLogNum + 1;	/*Logical number of the next data cluster in the double-linked list*/
  currCluster.stat = NULL_INODE;	/*Clean state*/

  /* Phsysical number of the first free data cluster */

  currPhysNum = p_sb->dzone_start + BLOCKS_PER_CLUSTER;

  while ( currPhysNum < lastCluster) {

    /* Writing current cluster's metadata to device*/
    if ( ( status = soWriteCacheCluster(currPhysNum, &currCluster) ) < 0 )
      return status;

    currLogNum++;
    currCluster.prev = currLogNum - 1;
    currCluster.next = currLogNum + 1;
    currPhysNum += BLOCKS_PER_CLUSTER;	/*Physical number of the next free data cluster in the double-linked list*/
  }

  /*Write Last Data Cluster*/
  currCluster.next = NULL_CLUSTER;

  if ( (status = soWriteCacheCluster(lastCluster, &currCluster)) < 0 )
    return status;

  /** Operation successful **/
  return 0;
}

/*
  check the consistency of the file system metadata
*/

static int checkFSConsist (void)
{
  SOSuperBlock *p_sb;                            /* pointer to the superblock */
  SOInode *inode;                                /* pointer to the contents of a block of the inode table */
  int stat;                                      /* status of operation */

  /* read the contents of the superblock to the internal storage area and get a pointer to it */

  if ((stat = soLoadSuperBlock ()) != 0) return stat;
  p_sb = soGetSuperBlock ();

  /* check superblock and related structures */

  if ((stat = soQCheckSuperBlock (p_sb)) != 0) return stat;

  /* read the contents of the first block of the inode table to the internal storage area and get a pointer to it */

  if ((stat = soLoadBlockInT (0)) != 0) return stat;
  inode = soGetBlockInT ();

  /* check inode associated with root directory (inode 0) and the contents of the root directory */

  if ((stat = soQCheckInodeIU (p_sb, &inode[0])) != 0) return stat;
  if ((stat = soQCheckDirCont (p_sb, &inode[0])) != 0) return stat;

  /* everything is consistent */

  return 0;
}
