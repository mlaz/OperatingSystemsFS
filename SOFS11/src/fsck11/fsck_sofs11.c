#include "fsck_sofs11.h"

#include "sofs_buffercache.h"
#include "sofs_const.h"
#include "sofs_superblock.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

static void printUsage ();
static void processError (uint32_t error);

int
main (int argc, char **argv)
{
  uint32_t nclusttotal;
  struct stat st;
  SOSuperBlock *p_sb;

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
      fprintf (stderr, "%s: Bad size of support file.\n", basename (argv[0]));
      return EXIT_FAILURE;
    }

  /*
   * Computing the number of dataclusters for the storage device
   *
   * nblocktotal = (dev_size / BLOCK_SIZE)
   * nclusttotal = nblocktotal / BPC
   **/
  nclustotal = (uint32_t) ( (st.st_size / BLOCK_SIZE) / BLOCKS_PER_CLUSTER );

  prinf("Device size (in clusters): %d.\n", nclustotal);

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
  printf("Checking super block header integrity...\n");
  if ( (fsckCheckSuperBlockHeader (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  /* Checking super block inode table metadata integrity */
  printf("Checking super block inode table metadata integrity...\n");
  if ( (fsckCheckSBInodeMetaData (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  /* Checking super block data zone metadata integrity */
  printf("Checking super block data zone metadata integrity...\n");
  if ( (fsckCheckDZoneMetaData (p_sb, nclusttotal)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  /* INODES */
  /* Checking inode table integrity */
  printf("Checking inode table integrity...\n");
  if ( (fsckCheckInodeTable (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  /* Checking inode linked list integrity */
  printf("Checking inode linked list integrity...\n");
  if ( (fsckCheckInodeList (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  /* DATA ZONE */
  /* Checking cluster caches integrity */
  printf("Checking cluster caches integrity...\n");
  if ( (fsckCheckCltCaches (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }
  /* Checking data zone integrity */
  printf("Checking data zone integrity...\n");
  if ( (fsckCheckDataZone (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  /* Checking cluster linked list integrity */
  printf("Checking cluster linked list integrity...\n");
  if ( (fsckCheckCltLList (p_sb)) != 0 )
    {
      processError(error);
      return EXIT_SUCCESS;
    }

  printf("Passage 1 Done!\n");
  return EXIT_SUCCESS;
}

static void printUsage ()
{
  printf("usage: fsck11 <volume_path>\n");
}

static void processError (uint32_t error)
{
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

    case EEDZLLBROKEN :
      {
        printf("DZone linked list broken.\n");
        break;
      }

    case EINVAL :
      {
        printf("EINVAL \n");
        break;
      }

      /* TODO: BasicOper/Buffercache Related */
    default:
      printf("Unknown error: %d \n", -error);
    }
}
