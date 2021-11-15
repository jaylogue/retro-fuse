/*
 * Copyright 2021 Jay Logue
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file  Implements a modern interface to a 2.9BSD filesystem.
 */

#ifndef __BSD29FS_H__
#define __BSD29FS_H__

#include <stdint.h>

struct stat;
struct timespec;
struct statvfs;

enum {

    /** Maximum filesystem size, in blocks.
     *  Limited to 2^24-1 due to 3-byte block numbers used in on-disk inode structure.
     */
    // TODO: check this
    BSD29FS_MAX_FS_SIZE        = 16777215,

    /** Minimum filesystem size, in blocks.
     *  (boot block + superblock + inode block + root directory block + 1 block for file data)
     */
    BSD29FS_MIN_FS_SIZE        = 5,

    /** Number of inodes per block.
     */
    BSD29FS_INODES_PER_BLOCK   = 16,

    /** Maximum size of the inode table, in blocks.
     *  Limited to 4096 due to 2-byte inode number used in on-disk directory structure.
     */
    BSD29FS_MAX_ITABLE_SIZE    = 65536/BSD29FS_INODES_PER_BLOCK,

    /** Minimum size of the inode table, in blocks.
     */
    BSD29FS_MIN_ITABLE_SIZE    = 1,

    /** Maximum value for uid/gid.
     */
    BSD29FS_MAX_UID_GID        = INT16_MAX,

    /** Maximum value for freelist interleave parameter n
     */
    // TODO: check this
    BSD29FS_MAXFN = 500,
};

/** Free list interleave parameters */
struct bsd29fs_flparams
{
    uint16_t m;
    uint16_t n;
};

typedef int (*bsd29fs_enum_dir_funct)(const char *entryname, const struct stat *statbuf, void *context);

extern int bsd29fs_init(int readonly);
extern int bsd29fs_shutdown();
extern int bsd29fs_mkfs(uint32_t fssize, uint32_t isize, const struct bsd29fs_flparams *flparams);
extern int bsd29fs_open(const char * name, int flags, mode_t mode);
extern int bsd29fs_close(int fd);
extern off_t bsd29fs_seek(int fd, off_t offset, int whence);
extern int bsd29fs_link(const char *oldpath, const char *newpath);
extern int bsd29fs_mknod(const char *pathname, mode_t mode, dev_t dev);
extern ssize_t bsd29fs_read(int fd, void *buf, size_t count);
extern ssize_t bsd29fs_pread(int fd, void *buf, size_t count, off_t offset);
extern ssize_t bsd29fs_write(int fd, const void *buf, size_t count);
extern ssize_t bsd29fs_pwrite(int fd, const void *buf, size_t count, off_t offset);
extern int bsd29fs_truncate(const char *pathname, off_t length);
extern int bsd29fs_stat(const char *pathname, struct stat *statbuf);
extern int bsd29fs_unlink(const char *pathname);
extern int bsd29fs_rename(const char *oldpath, const char *newpath);
extern int bsd29fs_chmod(const char *pathname, mode_t mode);
extern int bsd29fs_chown(const char *pathname, uid_t owner, gid_t group);
extern int bsd29fs_utimens(const char *filename, const struct timespec times[2]);
extern int bsd29fs_access(const char *pathname, int mode);
extern int bsd29fs_mkdir(const char *pathname, mode_t mode);
extern int bsd29fs_rmdir(const char *pathname);
extern int bsd29fs_enumdir(const char *pathname, bsd29fs_enum_dir_funct enum_funct, void *context);
extern int bsd29fs_sync();
extern int bsd29fs_statfs(const char *pathname, struct statvfs *buf);
extern int bsd29fs_setreuid(uid_t ruid, uid_t euid);
extern int bsd29fs_setregid(gid_t rgid, gid_t egid);
extern int bsd29fs_adduidmap(uid_t hostuid, uint32_t fsuid);
extern int bsd29fs_addgidmap(uid_t hostgid, uint32_t fsgid);

#endif /* __BSD29FS_H__ */
