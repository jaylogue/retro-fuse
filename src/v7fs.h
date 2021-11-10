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
 * @file  Implements a modern interface to a Unix v7 filesystem.
 */

#ifndef __V7FS_H__
#define __V7FS_H__

#include <stdint.h>

struct stat;
struct timespec;
struct statvfs;

enum {

    /** Maximum filesystem size, in blocks.
     *  Limited to 2^24-1 due to 3-byte block numbers used in on-disk inode structure.
     */
    V7FS_MAX_FS_SIZE        = 16777215,

    /** Minimum filesystem size, in blocks.
     *  (boot block + superblock + inode block + root directory block + 1 block for file data)
     */
    V7FS_MIN_FS_SIZE        = 5,

    /** Number of inodes per block.
     */
    V7FS_INODES_PER_BLOCK   = 8,

    /** Maximum size of the inode table, in blocks.
     *  Limited to 8192 due to 2-byte inode number used in on-disk directory structure.
     */
    V7FS_MAX_ITABLE_SIZE    = 65536/V7FS_INODES_PER_BLOCK,

    /** Minimum size of the inode table, in blocks.
     */
    V7FS_MIN_ITABLE_SIZE    = 1,

    /** Maximum value for uid/gid.
     */
    V7FS_MAX_UID_GID        = INT16_MAX,

    /** Maximum value for freelist interleave parameter n
     */
    V7FS_MAXFN = 500,
};

/** Free list interleave parameters */
struct v7fs_flparams
{
    uint16_t m;
    uint16_t n;
};

typedef int (*v7fs_enum_dir_funct)(const char *entryname, const struct stat *statbuf, void *context);

extern int v7fs_init(int readonly);
extern int v7fs_shutdown();
extern int v7fs_mkfs(uint32_t fssize, uint32_t isize, const struct v7fs_flparams *flparams);
extern int v7fs_open(const char * name, int flags, mode_t mode);
extern int v7fs_close(int fd);
extern off_t v7fs_seek(int fd, off_t offset, int whence);
extern int v7fs_link(const char *oldpath, const char *newpath);
extern int v7fs_mknod(const char *pathname, mode_t mode, dev_t dev);
extern ssize_t v7fs_read(int fd, void *buf, size_t count);
extern ssize_t v7fs_pread(int fd, void *buf, size_t count, off_t offset);
extern ssize_t v7fs_write(int fd, const void *buf, size_t count);
extern ssize_t v7fs_pwrite(int fd, const void *buf, size_t count, off_t offset);
extern int v7fs_truncate(const char *pathname, off_t length);
extern int v7fs_stat(const char *pathname, struct stat *statbuf);
extern int v7fs_unlink(const char *pathname);
extern int v7fs_rename(const char *oldpath, const char *newpath);
extern int v7fs_chmod(const char *pathname, mode_t mode);
extern int v7fs_chown(const char *pathname, uid_t owner, gid_t group);
extern int v7fs_utimens(const char *filename, const struct timespec times[2]);
extern int v7fs_access(const char *pathname, int mode);
extern int v7fs_mkdir(const char *pathname, mode_t mode);
extern int v7fs_rmdir(const char *pathname);
extern int v7fs_enumdir(const char *pathname, v7fs_enum_dir_funct enum_funct, void *context);
extern int v7fs_sync();
extern int v7fs_statfs(const char *pathname, struct statvfs *buf);
extern int v7fs_setreuid(uid_t ruid, uid_t euid);
extern int v7fs_setregid(gid_t rgid, gid_t egid);
extern int v7fs_adduidmap(uid_t hostuid, uint32_t fsuid);
extern int v7fs_addgidmap(uid_t hostgid, uint32_t fsgid);

#endif /* __V7FS_H__ */
