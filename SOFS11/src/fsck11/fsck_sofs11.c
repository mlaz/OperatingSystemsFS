

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

static void printUsage (char **argv);
static void processError (FILE *logfile, int error);
static void logTables(FILE *logfile,
                      uint8_t *clt_table,
                      uint32_t dzone_total,
                      uint8_t *inode_table,
                      uint32_t itotal);

int
main (int argc, char **argv)
{
  char *logfile_path = NULL;
  char *diskfile_path = NULL;
  FILE *logfile;
  struct stat st;
  SOSuperBlock *p_sb;
  int status, error = 0;
  uint32_t ntotal;       /* total number of blocks */
  uint8_t *clt_table;
  uint8_t *inode_table;

  if (argc < 2)
    {
      printUsage(argv);
      return EXIT_FAILURE;
    }

  while ((status = getopt (argc, argv, "f:l:")) != -1)
    switch (status)
      {
      case 'l':
        logfile_path = optarg;
        break;
      case 'f':
        diskfile_path = optarg;
        break;
      case '?':
        printUsage(argv);
      default:
        abort ();
      }

  if (diskfile_path == NULL)
    {
      printUsage(argv);
      return EXIT_FAILURE;
    }

  /* Getting file size */
  if (stat(diskfile_path, &st) == -1)
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
  if ((status = soOpenBufferCache (diskfile_path, BUF)) != 0)
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


  /** Setting up logfile **/
  if (logfile_path == NULL)
    {
      logfile = fopen("/dev/null", "wa");
      if (logfile == NULL)
        {
          fprintf(stderr, "Failed opening /dev/null.\n");
          return EXIT_FAILURE;
        }
    }
  else
    {
      logfile = fopen(logfile_path, "w");
      if (logfile == NULL)
        {
          fprintf(stderr, "Failed opening %s\n", logfile_path);
          return EXIT_FAILURE;
        }
    }

  /** PASSAGE 1 **/

  /* SB */
  /* Checking super block header integrity */
  printf("Checking super block header integrity...\t\t");
  fprintf(logfile, "Checking super block header integrity...\t\t");
  fflush(stdout);
  if ( (error = fsckCheckSuperBlockHeader (p_sb)) != FSCKOK )
    {
      processError(logfile, error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* Checking super block inode table metadata integrity */
  printf("Checking super block inode table metadata integrity...\t");
  fprintf(logfile, "Checking super block inode table metadata integrity...\t");
  fflush(stdout);

  if ( (error = fsckCheckSBInodeMetaData (p_sb)) != FSCKOK )
    {
      processError(logfile, error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* Checking super block data zone metadata integrity */
  printf("Checking super block data zone metadata integrity...\t");
  fprintf(logfile, "Checking super block data zone metadata integrity...\t");
  fflush(stdout);

  ntotal = st.st_size / BLOCK_SIZE;

  if ( (error = fsckCheckDZoneMetaData (p_sb, ntotal)) != FSCKOK )
    {
      processError(logfile, error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  printf("Passage 1 Done.\n");
  fprintf(logfile, "[OK]\n");
  fprintf(logfile, "Passage 1 Done.\n");

  /** PASSAGE 2 **/

  /* INODES */
  /* Checking inode table integrity */
  inode_table = (uint8_t*) calloc(p_sb->itotal, sizeof(uint8_t));
  printf("Checking inode table integrity...\t\t\t");
  fprintf(logfile, "Checking inode table integrity...\t\t\t");
  fflush(stdout);
  if ( (error = fsckCheckInodeTable (p_sb, inode_table)) != FSCKOK )
    {
      logTables(logfile, NULL, 0, inode_table, p_sb->itotal);
      processError(logfile, error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* Checking inode linked list integrity */
  printf("Checking inode linked list integrity...\t\t\t");
  fprintf(logfile, "Checking inode linked list integrity...\t\t\t");
  fflush(stdout);
  if ( (error = fsckCheckInodeList (p_sb)) != FSCKOK )
    {
      logTables(logfile, NULL, 0, inode_table, p_sb->itotal);
      processError(logfile, error);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* DATA ZONE */
  /* Allocating cluster metadata table*/
  clt_table = (uint8_t*) calloc(p_sb->dzone_total, sizeof(uint8_t));

  /* Checking cluster caches integrity */
  printf("Checking cluster caches integrity...\t\t\t");
  fprintf(logfile, "Checking cluster caches integrity...\t\t\t");
  fflush(stdout);
  if ( (error = fsckCheckCltCaches (p_sb, clt_table)) != FSCKOK )
    {
      processError(logfile, error);
      logTables(logfile, clt_table, p_sb->dzone_total, inode_table, p_sb->itotal);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* Checking data zone integrity */
  printf("Checking data zone integrity...\t\t\t\t");
  fprintf(logfile, "Checking data zone integrity...\t\t\t\t");
  fflush(stdout);
  if ( (error = fsckCheckDataZone (p_sb, clt_table)) != FSCKOK )
    {
      processError(logfile, error);
      logTables(logfile, clt_table, p_sb->dzone_total, inode_table, p_sb->itotal);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* Checking cluster linked list integrity */
  printf("Checking cluster linked list integrity...\t\t");
  fprintf(logfile, "Checking cluster linked list integrity...\t\t");
  fflush(stdout);
  if ( (error = fsckCheckCltLList (p_sb)) != FSCKOK )
    {
      processError(logfile, error);
      logTables(logfile, clt_table, p_sb->dzone_total, inode_table, p_sb->itotal);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  /* Checking cluster linked list integrity */
  printf("Checking inode to cluster references integrity...\t");
  fprintf(logfile, "Checking inode to cluster references integrity...\t");
  fflush(stdout);
  if ( (error = fsckCheckInodeClusters (p_sb, clt_table, inode_table)) != FSCKOK )
    {
      processError(logfile, error);
      logTables(logfile, clt_table, p_sb->dzone_total, inode_table, p_sb->itotal);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");

  printf("Passage 2 Done.\n");
  fprintf(logfile, "Passage 2 Done.\n");
  /** PASSAGE 3 **/

  /* Checking directory tree integrity */
  printf("Checking directory tree integrity...\t\t\t");
  fprintf(logfile, "Checking directory tree integrity...\t\t\t");
  fflush(stdout);
  if ( (error = fsckCheckDirTree (p_sb, inode_table)) != FSCKOK )
    {
      processError(logfile, error);
      logTables(logfile, clt_table, p_sb->dzone_total, inode_table, p_sb->itotal);
      return EXIT_SUCCESS;
    }
  printf("[OK]\n");
  fprintf(logfile, "[OK]\n");
  logTables(logfile, clt_table, p_sb->dzone_total, inode_table, p_sb->itotal);

  return EXIT_SUCCESS;
}

static void logTables(FILE *logfile,
                      uint8_t *clt_table,
                      uint32_t dzone_total,
                      uint8_t *inode_table,
                      uint32_t itotal)
{
  int i;
  if (logfile == NULL)
    return;


  if (clt_table != NULL && (dzone_total > 0))
    {
      fprintf(logfile, "\n**DataCluster table:\n");
      for (i = 0; i < dzone_total; i++)
        {
          fprintf(logfile, "clt[%d]:\n",i);
          if (clt_table[i] == 0)
            fprintf(logfile, "\tCLT_UNCHECK\n");
          if (clt_table[i] & CLT_FREE)
            fprintf(logfile, "\tCLT_FREE\n");
          if (clt_table[i] & CLT_CLEAN)
            fprintf(logfile, "\tCLT_CLEAN\n");
          if (clt_table[i] & CLT_REF)
            fprintf(logfile, "\tCLT_REF\n");
          if (clt_table[i] & CLT_REF_ERR)
            fprintf(logfile, "\tCLT_REF_ERR\n");
          if (clt_table[i] & CLT_IND_ERR)
            fprintf(logfile, "\tCLT_IND_ERR\n");

        }
      fflush(logfile);
    }

  if (inode_table != NULL && (itotal > 0))
    {
      fprintf(logfile, "\n**Inode table:\n");
      for (i = 0; i < itotal; i++)
        {
          fprintf(logfile, "inod[%d]:\n",i);
          if (inode_table[i]  == 0)
            fprintf(logfile, "\tINOD_UNCHECK\n");
          if (inode_table[i] & INOD_CHECK)
            fprintf(logfile, "\tINOD_CHECK\n");
          if (inode_table[i] & INOD_FREE)
            fprintf(logfile, "\tINOD_FREE\n");
          if (inode_table[i] & INOD_CLEAN)
            fprintf(logfile, "\tINOD_CLEAN\n");
          if (inode_table[i] & INOD_VISIT)
            fprintf(logfile, "\tINOD_VISIT\n");
          if (inode_table[i] & INOD_REF_ERR)
            fprintf(logfile, "\tINOD_REF_ERR\n");
          if (inode_table[i] & INOD_PARENT_ERR)
            fprintf(logfile, "\tINOD_PARENT_ERR\n");
          if (inode_table[i] & INOD_DOUB_REF)
            fprintf(logfile, "\tINOD_DOUB_REF\n");
          if (inode_table[i] & INOD_LOOP)
            fprintf(logfile, "\tINOD_LOOP\n");

        }
      fflush(logfile);
    }

}

static void printUsage (char **argv)
{
  printf("usage: %s -f <volume_path> -l <logfile_path> \n", argv[0]);
}

static void processError (FILE* logfile, int error)
{
  fprintf(stderr, "[ERROR]\n");
  fprintf(logfile, "[ERROR]\n");
  switch (-error)
    {

      /* SuperBlock Checking Related */
    case EMAGIC :
      {
        fprintf(stderr, "Invalid Magic number.\n");
        fprintf(logfile, "Invalid Magic number.\n");
        break;
      }

    case EVERSION :
      {
        fprintf(stderr, "Invalid version number.\n");
        fprintf(logfile, "Invalid version number.\n");
        break;
      }

    case EVNAME :
      {
        fprintf(stderr, "Inconsistent name string.\n");
        fprintf(logfile, "Inconsistent name string.\n");
        break;
      }

    case EMSTAT :
      {
        fprintf(stderr, "Inconsistent mstat flag.\n");
        fprintf(logfile, "Inconsistent mstat flag.\n");
        break;
      }

    case ESBISTART :
      {
        fprintf(stderr, "Inconsistent inode table start value.\n");
        fprintf(logfile, "Inconsistent inode table start value.\n");
        break;
      }

    case ESBISIZE :
      {
        fprintf(stderr, "Inconsistent inode table size value.\n");
        fprintf(logfile, "Inconsistent inode table size value.\n");
        break;
      }

    case ESBITOTAL :
      {
        fprintf(stderr, "Inconsistent total inode value.\n");
        fprintf(logfile, "Inconsistent total inode value.\n");
        break;
      }

    case ESBIFREE :
      {
        fprintf(stderr, "Inconsistent free inode value.\n");
        fprintf(logfile, "Inconsistent free inode value.\n");
        break;
      }

    case ESBDZSTART :
      {
        fprintf(stderr, "Inconsistent data zone start value.\n");
        fprintf(logfile, "Inconsistent data zone start value.\n");
        break;
      }

    case ESBDZTOTAL :
      {
        fprintf(stderr, "Inconsistent data zone total value.\n");
        fprintf(logfile, "Inconsistent data zone total value.\n");
        break;
      }

    case ESBDZFREE :
      {
        fprintf(stderr, "Inconsistent data zone free value.\n");
        fprintf(logfile, "Inconsistent data zone free value.\n");
        break;
      }

      /* InodeTable Consistency Checking Related */
    case EIBADINODEREF :
      {
        fprintf(stderr, "Inconsistent inode linked list reference.\n");
        fprintf(logfile, "Inconsistent inode linked list reference.\n");
        break;
      }

    case EIBADHEAD :
      {
        fprintf(stderr, "Inconsistent inode linked list head.\n");
        fprintf(logfile, "Inconsistent inode linked list reference.\n");
        break;
      }

    case EIBADTAIL :
      {
        fprintf(stderr, "Inconsistent inode linked list tail.\n");
        fprintf(logfile, "Inconsistent inode linked list reference.\n");
        break;
      }

    case EBADFREECOUNT :
      {
        fprintf(stderr, "Inconsistent ifree value on superblock.\n");
        fprintf(logfile, "Inconsistent inode linked list reference.\n");
        break;
      }

    case EILLNOTFREE :
      {
        fprintf(stderr, "Inode not free within the linked list.\n");
        fprintf(logfile, "Inode not free within the linked list.\n");
        break;
      }

    case EILLLOOP :
      {
        fprintf(stderr, "Inode linked list might have a loop.\n");
        fprintf(logfile, "Inode linked list might have a loop.\n");
        break;
      }

    case EILLBROKEN :
      {
        fprintf(stderr, "Inode linked list is broken.\n");
        fprintf(logfile, "Inode linked list is broken.\n");
        break;
      }

      /* DataZone Consistency Checking Related */
    case ERCACHEIDX :
      {
        fprintf(stderr, "Retrieval cache index out of bounderies.\n");
        fprintf(logfile, "Retrieval cache index out of bounderies.\n");
        break;
      }

    case ERCACHEREF :
      {
        fprintf(stderr, "Retrieval cache cluster is not free.\n");
        fprintf(logfile, "Retrieval cache cluster is not free.\n");
        break;
      }

    case  ERCLTREF :
      {
        fprintf(stderr, "Invalid retrieval cache reference (cluster not clean).\n");
        fprintf(logfile, "Invalid retrieval cache reference (cluster not clean).\n");
        break;
      }

    case  EICACHEIDX :
      {
        fprintf(stderr, "Insertion cache index out of bounderies.\n");
        fprintf(logfile, "Insertion cache index out of bounderies.\n");
        break;
      }

    case  EICACHEREF :
      {
        fprintf(stderr, "Retrieval cache cluster is not free.\n");
        fprintf(logfile, "Retrieval cache cluster is not free.\n");
        break;
      }


    case EDZLLLOOP :
      {
        fprintf(stderr, "DZone linked list might have a loop.\n");
        fprintf(logfile, "DZone linked list might have a loop.\n");
        break;
      }

    case EDZLLBROKEN :
      {
        fprintf(stderr, "DZone linked list broken.\n");
        fprintf(logfile, "DZone linked list broken.\n");
        break;
      }

    case EDZBADTAIL :
      {
        fprintf(stderr, "Inconsistent DZone linked list tail.\n");
        fprintf(logfile, "Inconsistent DZone linked list tail.\n");
        break;
      }

    case EDZBADHEAD :
      {
        fprintf(stderr, "Inconsistent DZone linked list head.\n");
        fprintf(logfile, "Inconsistent DZone linked list head.\n");
        break;
      }

    case EDZLLBADREF :
      {
        fprintf(stderr, "Inconsistent DZone linked list reference.\n");
        fprintf(logfile, "Inconsistent DZone linked list reference.\n");
        break;
      }

    case ERMISSCLT :
      {
        fprintf(stderr, "Inconsistent number of (free clean) data clusters on retrieval cache.\n");
        fprintf(logfile, "Inconsistent number of (free clean) data clusters on retrieval cache.\n");
        break;
      }

    case EFREECLT :
      {
        fprintf(stderr, "Inconsistent number of free data clusters.\n");
        fprintf(logfile, "Inconsistent number of free data clusters.\n");
        break;
      }

    case EDIRLOOP :
      {
        fprintf(stderr, "There is a loop on the directory tree.\n");
        fprintf(logfile, "There is a loop on the directory tree.\n");
        break;
      }

    /* case EINVAL : */
    /*   { */
    /*     fprintf(stderr, "EINVAL \n"); */
    /*     break; */
    /*   } */

      /* TODO: BasicOper/Buffercache Related */
    default:
      fprintf(stderr, "Unknown error: %d \n", error);
      fprintf(logfile, "Unknown error: %d \n", error);
    }
}
