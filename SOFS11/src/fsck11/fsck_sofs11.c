

#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "sofs_const.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_basicoper.h"
#include "fsck_sofs11.h"

static void printUsage ();
static void processError (int error);

int
main (int argc, char **argv)
{
  struct stat st;
  SOSuperBlock *p_sb;
  int status, error = 0;
  uint32_t ntotal;       /* total number of blocks */
  uint8_t *clt_table;
  if (argc != 2)
    {
      printUsage();
      return EXIT_FAILURE;
    }

  /* Getting file size */
  if (stat(argv[1], &st) == -1)
    {
        perror("stat");
        return EXIT_FAILURE;
    }

  /* Check whether the file size is a multiple of BLOCK_SIZE */
  if (st.st_size % BLOCK_SIZE != 0)
    {
      fprintf (stderr, "%s: Bad size of support file.\n", basename(argv[0]) );
      return EXIT_FAILURE;
    }

  /* Opening a buffered communication channel with the storage device */
  if ((status = soOpenBufferCache (argv[optind], BUF)) != 0)
    {
      printf("Failed opening buffered communication channel.\n");
      return EXIT_FAILURE;
    }

  if ( (status = soLoadSuperBlock()) != 0 )
    {
      printf("Failed fetching super block.\n");
      return EXIT_FAILURE;
    }
  p_sb = soGetSuperBlock ();

  /** PASSAGE 1 **/

  /* SB */
  /* Checking super block header integrity */
  printf("Checking super block header integrity...\t\t");
  if ( (error = fsckCheckSuperBlockHeader (p_sb)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* Checking super block inode table metadata integrity */
  printf("Checking super block inode table metadata integrity...\t");
  if ( (error = fsckCheckSBInodeMetaData (p_sb)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* Checking super block data zone metadata integrity */
  printf("Checking super block data zone metadata integrity...\t");

  ntotal = st.st_size / BLOCK_SIZE;

  if ( (error = fsckCheckDZoneMetaData (p_sb, ntotal)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* INODES */
  /* Checking inode table integrity */
  printf("Checking inode table integrity...\t\t\t");
  if ( (error = fsckCheckInodeTable (p_sb)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* Checking inode linked list integrity */
  printf("Checking inode linked list integrity...\t\t\t");
  if ( (error = fsckCheckInodeList (p_sb)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* DATA ZONE */
  /* Allocating cluster metadata table*/
  clt_table = (uint8_t*) calloc(p_sb->dzone_total, sizeof(uint8_t));

  /* Checking cluster caches integrity */
  printf("Checking cluster caches integrity...\t\t\t");
  if ( (error = fsckCheckCltCaches (p_sb, clt_table)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* Checking data zone integrity */
  printf("Checking data zone integrity...\t\t\t\t");
  if ( (error = fsckCheckDataZone (p_sb, clt_table)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

  /* Checking cluster linked list integrity */
  printf("Checking cluster linked list integrity...\t\t");
  if ( (error = fsckCheckCltLList (p_sb)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");

    /* Checking cluster linked list integrity */
  printf("Checking inode to cluster references integrity...\t");
  if ( (error = fsckCheckInodeClusters (p_sb, clt_table)) != FSCKOK )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  int i;
  for ( i = 0; i < p_sb->dzone_total; i++)
    {
      printf("clt[%d]:\n",i);
      if(clt_table[i] == 0)
        printf("\tCLT_UNCHECK\n");
      if (clt_table[i] & CLT_FREE)
        printf("\tCLT_FREE\n");
      if (clt_table[i] & CLT_CLEAN)
        printf ("\tCLT_CLEAN\n");
      if (clt_table[i] & CLT_REF)
        printf("\tCLT_REF\n");
      if (clt_table[i] & CLT_REF_ERR)
        printf("\tCLT_REF_ERR\n");
      if (clt_table[i] & CLT_IND_ERR)
        printf("\tCLT_IND_ERR\n");
      
    }
  printf("Passage 1 Done.\n");

  return EXIT_SUCCESS;
}

static void printUsage ()
{
  printf("usage: fsck11 <volume_path>\n");
}

static void processError (int error)
{
  printf("[ERROR]\n");
  switch (-error)
    {

      /* SuperBlock Checking Related */
    case EMAGIC :
      {
        printf("Invalid Magic number.\n");
        break;
      }

    case EVERSION :
      {
        printf("Invalid version number.\n");
        break;
      }

    case EVNAME :
      {
        printf("Inconsistent name string.\n");
        break;
      }

    case EMSTAT :
      {
        printf("Inconsistent mstat flag.\n");
        break;
      }

    case ESBISTART :
      {
        printf("Inconsistent inode table start value.\n");
        break;
      }

    case ESBISIZE :
      {
        printf("Inconsistent inode table size value.\n");
        break;
      }

    case ESBITOTAL :
      {
        printf("Inconsistent total inode value.\n");
        break;
      }

    case ESBIFREE :
      {
        printf("Inconsistent free inode value.\n");
        break;
      }

    case ESBDZSTART :
      {
        printf("Inconsistent data zone start value.\n");
        break;
      }

    case ESBDZTOTAL :
      {
        printf("Inconsistent data zone total value.\n");
        break;
      }

    case ESBDZFREE :
      {
        printf("Inconsistent data zone free value.\n");
        break;
      }

      /* InodeTable Consistency Checking Related */
    case EIBADINODEREF :
      {
        printf("Inconsistent inode linked list reference.\n");
        break;
      }

    case EIBADHEAD :
      {
        printf("Inconsistent inode linked list head.\n");
        break;
      }

    case EIBADTAIL :
      {
        printf("Inconsistent inode linked list tail.\n");
        break;
      }

    case EBADFREECOUNT :
      {
        printf("Inconsistent ifree value on superblock.\n");
        break;
      }

    case EILLNOTFREE :
      {
        printf("Inode not free within the linked list.\n");
        break;
      }

    case EILLLOOP :
      {
        printf("Inode linked list might have a loop.\n");
        break;
      }

    case EILLBROKEN :
      {
        printf("Inode linked list is broken.\n");
        break;
      }

      /* DataZone Consistency Checking Related */
    case ERCACHEIDX :
      {
        printf("Retrieval cache index out of bounderies.\n");
        break;
      }

    case ERCACHEREF :
      {
        printf("Retrieval cache cluster is not free.\n");
        break;
      }

    case  ERCLTREF :
      {
        printf("Invalid retrieval cache reference (cluster not clean).\n");
        break;
      }

    case  EICACHEIDX :
      {
        printf("Insertion cache index out of bounderies.\n");
        break;
      }

    case  EICACHEREF :
      {
        printf("Retrieval cache cluster is not free.\n");
        break;
      }


    case EDZLLLOOP :
      {
        printf("DZone linked list might have a loop.\n");
        break;
      }

    case EDZLLBROKEN :
      {
        printf("DZone linked list broken.\n");
        break;
      }

    case EDZBADTAIL :
      {
        printf("Inconsistent DZone linked list tail.\n");
        break;
      }

    case EDZBADHEAD :
      {
        printf("Inconsistent DZone linked list head.\n");
        break;
      }

    case EDZLLBADREF :
      {
        printf("Inconsistent DZone linked list reference.\n");
        break;
      }

    case ERMISSCLT :
      {
        printf("Inconsistent number of (free clean) data clusters on retrieval cache.\n");
        break;
      }

    case EFREECLT :
      {
        printf("Inconsistent number of free data clusters.\n");
        break;
      }

    /* case EINVAL : */
    /*   { */
    /*     printf("EINVAL \n"); */
    /*     break; */
    /*   } */

      /* TODO: BasicOper/Buffercache Related */
    default:
      printf("Unknown error: %d \n", error);
    }
}
