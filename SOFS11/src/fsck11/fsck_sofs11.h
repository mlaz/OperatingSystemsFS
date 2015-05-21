#include "sofs_buffercache.h"
#include "sofs_basicoper.h"
#include "sofs_const.h"
#include "sofs_superblock.h"
#include "sofs_basicconsist.h"

/* #include <errno.h> */

/* Status Constants */

/** \brief Invalid Magic number */
#define FSCKOK 0


/* SuperBlock Checking Related */

/** \brief Invalid Magic number */
#define EMAGIC 530

/** \brief Invalid version number */
#define EVERSION 531

/** \brief Inconsistent name string */
#define EVNAME 532

/** \brief Inconsistent mstat flag */
#define EMSTAT 533

/** \brief Inconsistent inode table start value */
#define ESBISTART 534

/** \brief Inconsistent inode table size value */
#define ESBISIZE 535

/** \brief Inconsistent total inode value */
#define ESBITOTAL 536

/** \brief Inconsistent free inode value */
#define ESBIFREE 537

/** \brief Inconsistent data zone start value */
#define ESBDZSTART 540

/** \brief Inconsistent data zone total value */
#define ESBDZTOTAL 541

/** \brief Inconsistent data zone free value */
#define ESBDZFREE 542

/* InodeTable Consistency Checking Related */

/** \brief Inconsistent inode linked list reference */
#define EIBADINODEREF 543

/** \brief Inconsistent inode linked list head */
#define EIBADHEAD 544

/** \brief Inconsistent inode linked list tail */
#define EIBADTAIL 545

/** \brief Inconsistent ifree value on superblock */
#define EBADFREECOUNT 546

/** \brief Inode not free within the linked list */
#define EILLNOTFREE 547

/** \brief Inode linked list might have a loop */
#define EILLLOOP 548

/** \brief Inode linked list is broken */
#define EILLBROKEN 549

/* DataZone Consistency Checking Related */

/** \brief Retrieval cache index out of bounderies **/
#define ERCACHEIDX 550

/** \brief Retrieval cache cluster is not free **/
#define ERCACHEREF 551

/** \brief Invalid retrieval cache reference (cluster not clean) **/
#define ERCLTREF 552

/** \brief Insertion cache index out of bounderies **/
#define EICACHEIDX 553

/** \brief Retrieval cache cluster is not free **/
#define EICACHEREF 554

/** \brief DZone linked list might have a loop **/
#define EDZLLLOOP 555

/** \brief DZone linked list broken **/
#define EDZLLBROKEN 556

/** \brief Inconsistent DZone linked list tail */
#define EDZBADTAIL 557

/** \brief Inconsistent DZone linked list head */
#define EDZBADHEAD 558

/** \brief Inconsistent DZone linked list reference */
#define EDZLLBADREF 559

/** \brief Inconsistent number of (free clean) data clusters on retrieval cache */
#define ERMISSCLT 560

/** \brief Inconsistent number of free data clusters */
#define EFREECLT 561

/* Methods */

/* Super Block Related Tasks */

int fsckCheckSuperBlockHeader (SOSuperBlock *p_sb);

int fsckCheckSBInodeMetaData (SOSuperBlock *p_sb);

int fsckCheckDZoneMetaData (SOSuperBlock *p_sb, uint32_t ntotal);


/* Inode Related Tasks */

int fsckCheckInodeTable (SOSuperBlock *p_sb);

int fsckCheckInodeList (SOSuperBlock *p_sb);

/* Data Cluster Related Tasks */

int fsckCheckCltCaches (SOSuperBlock *p_sb);

int fsckCheckDataZone (SOSuperBlock *p_sb);

int fsckCheckCltLList (SOSuperBlock *p_sb);
