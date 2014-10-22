/**
 *  \file sofs_syscalls_others.c (implementation file for a number of syscalls)
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
 *  \brief Mount the SOFS10 file system.
 *
 *  A buffered communication channel is established with the storage device.
 *  The superblock is read and it is checked if the file system was properly unmounted the last time it was mounted. If
 *  not, a consistency check is performed (presently, the check is superficial, a more thorough one is required).
 *
 *  \param devname absolute path to the Linux file that simulates the storage device
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>device path</em> is a \c NULL string or the magic number is not the one
 *                      characteristic of SOFS11
 *  \return -\c ENAMETOOLONG, if the absolute path exceeds the maximum allowed length
 *  \return -\c EBUSY, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EIO, if it fails on writing
 *  \return -\c ELIBBAD, if the supporting file size is invalid or the file system is inconsistent
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soMountSOFS (const char *devname)
{
  soProbe (61, "soMountSOFS (\"%s\")\n", devname);

  int soMountSOFS_bin (const char *devname);
  return soMountSOFS_bin(devname);
}

/**
 *  \brief Unmount the SOFS10 file system.
 *
 *  The buffered communication channel previously established with the storage device is closed. This means, namely,
 *  that the contents of the storage area is flushed into the storage device to keep data update. Before that, however,
 *  the mount flag of the superblock is set to <em>properly unmounted</em>.
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soUnmountSOFS (void)
{
  soProbe (62, "soUnmountSOFS ()\n");

  int soUnmountSOFS_bin (void);
  return soUnmountSOFS_bin();
}

/**
 *  \brief Get file system statistics.
 *
 *  It tries to emulate <em>statvfs</em> system call.
 *
 *  Information about a mounted file system is returned.
 *  It checks whether the calling process can access the file specified by the path.
 *
 *  \param ePath path to any file within the mounted file system
 *  \param st pointer to a statvfs structure
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the pointers are \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soStatFS (const char *ePath, struct statvfs *st)
{
  soProbe (63, "soStatFS (\"%s\", %p)\n", ePath, st);

  int soStatFS_bin (const char *ePath, struct statvfs *st);
  return soStatFS_bin(ePath, st);
}

/**
 *  \brief Get file status.
 *
 *  It tries to emulate <em>stat</em> system call.
 *
 *  Information about a specific file is returned.
 *  It checks whether the calling process can access the file specified by the path.
 *
 *  \param ePath path to the file
 *  \param st pointer to a stat structure
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if any of the pointers are \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soStat (const char *ePath, struct stat *st)
{
  soProbe (64, "soStat (\"%s\", %p)\n", ePath, st);

  int soStat_bin (const char *ePath, struct stat *st);
  return soStat_bin(ePath, st);
}

/**
 *  \brief Check real user's permissions for a file.
 *
 *  It tries to emulate <em>access</em> system call.
 *
 *  It checks whether the calling process can access the file specified by the path.
 *
 *  \param ePath path to the file
 *  \param opRequested operation to be performed:
 *                    F_OK (check if file exists)
 *                    a bitwise combination of R_OK, W_OK, and X_OK
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path or
 *                      no operation of the defined class is described
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one, or the operation is denied
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soAccess (const char *ePath, int opRequested)
{
  soProbe (65, "soAccess (\"%s\", %u)\n", ePath, opRequested);

  int soAccess_bin (const char *ePath, int opRequested);
  return soAccess_bin(ePath, opRequested);
}

/**
 *  \brief Change permissions of a file.
 *
 *  It tries to emulate <em>chmod</em> system call.
 *
 *  It changes the permissions of a file specified by the path.
 *
 *  \remark If the file is a symbolic link, its contents shall always be used to reach the destination file, so the
 *          permissions of a symbolic link can never be changed (they are set to rwx for <em>user</em>, <em>group</em>
 *          and <em>other</em> when the link is created and remain unchanged thereafter).
 *
 *  \param ePath path to the file
 *  \param mode permissions to be set:
 *                    a bitwise combination of S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH,
                      S_IXOTH
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path or
 *                      no mode of the defined class is described
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation is neither the file's owner, nor is <em>root</em>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soChmod (const char *ePath, mode_t mode)
{
  soProbe (66, "soChmod (\"%s\", %u)\n", ePath, mode);

  int soChmod_bin (const char *ePath, mode_t mode);
  return soChmod_bin(ePath, mode);
}

/**
 *  \brief Change the ownership of a file.
 *
 *  It tries to emulate <em>chown</em> system call.
 *
 *  It changes the ownership of a file specified by the path.
 *
 *  \param ePath path to the file
 *  \param owner file user id (-1, if user is not to be changed)
 *  \param group file group id (-1, if group is not to be changed)
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation is neither the file's owner, nor is <em>root</em>, nor
 *                     the specified group is one of the owner's supplementary groups
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soChown (const char *ePath, uid_t owner, gid_t group)
{
  soProbe (67, "soChown (\"%s\", %u, %u)\n", ePath, owner, group);

  int soChown_bin (const char *ePath, uid_t owner, gid_t group);
  return soChown_bin(ePath, owner, group);
}

/**
 *  \brief Change the last access and modification times of a file.
 *
 *  It tries to emulate <em>utime</em> system call.
 *
 *  \param ePath path to the file
 *  \param times pointer to a structure where the last access and modification times are passed, if \c NULL, the last
 *               access and modification times are set to the current time
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT,  if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation is neither the file's owner, nor is <em>root</em>, or
 *                     has not write permission
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soUtime (const char *ePath, const struct utimbuf *times)
{
  soProbe (68, "soUtime (\"%s\", %p)\n", ePath, times);

  int soUtime_bin (const char *ePath, const struct utimbuf *times);
  return soUtime_bin(ePath, times);
}

/**
 *  \brief Open a regular file.
 *
 *  It tries to emulate <em>open</em> system call.
 *
 *  \param ePath path to the file
 *  \param flags access modes to be used:
 *                    O_RDONLY, O_WRONLY, O_RDWR
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path or
 *                      no access mode of the defined class is described
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EISDIR, if <tt>ePath</tt> represents a directory
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not the proper permission (read / write) on the file
 *                     described by <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soOpen (const char *ePath, int flags)
{
  soProbe (69, "soOpen (\"%s\", %x)\n", ePath, flags);

  int soOpen_bin (const char *ePath, int flags);
  return soOpen_bin(ePath, flags);
}

/**
 *  \brief Close a regular file.
 *
 *  It tries to emulate <em>close</em> system call.
 *
 *  \param ePath path to the file
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EISDIR, if <tt>ePath</tt> represents a directory
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soClose (const char *ePath)
{
  soProbe (70, "soClose (\"%s\")\n", ePath);

  int soClose_bin (const char *ePath);
  return soClose_bin(ePath);
}

/**
 *  \brief Synchronize a file's in-core state with storage device.
 *
 *  It tries to emulate <em>fsync</em> system call.
 *
 *  \param ePath path to the file
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt>, but the last one, is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soFsync (const char *ePath)
{
  soProbe (71, "soFsync (\"%s\")\n", ePath);

  int soFsync_bin (const char *ePath);
  return soFsync_bin(ePath);
}

/**
 *  \brief Open a directory for reading.
 *
 *  It tries to emulate <em>opendir</em> system call.
 *
 *  \param ePath path to the file
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt> is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c EPERM, if the process that calls the operation has not read permission on the directory described by
 *                     <tt>ePath</tt>
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soOpendir (const char *ePath)
{
  soProbe (72, "soOpendir (\"%s\")\n", ePath);

  int soOpendir_bin (const char *ePath);
  return soOpendir_bin(ePath);
}

/**
 *  \brief Close a directory.
 *
 *  It tries to emulate <em>closedir</em> system call.
 *
 *  \param ePath path to the file
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the pointer to the string is \c NULL or the path string does not describe an absolute path
 *  \return -\c ENAMETOOLONG, if the path name or any of its components exceed the maximum allowed length
 *  \return -\c ENOTDIR, if any of the components of <tt>ePath</tt> is not a directory
 *  \return -\c ELOOP, if the path resolves to more than one symbolic link
 *  \return -\c ENOENT, if no entry with a name equal to any of the components of <tt>ePath</tt> is found
 *  \return -\c EACCES, if the process that calls the operation has not execution permission on any of the components
 *                      of <tt>ePath</tt>, but the last one
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails on writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */
int soClosedir (const char *ePath)
{
  soProbe (73, "soClosedir (\"%s\")\n", ePath);

  int soClosedir_bin (const char *ePath);
  return soClosedir_bin(ePath);
}
