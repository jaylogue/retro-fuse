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
 * @file  Implements a modern interface to a Unix v6 filesystem.
 */

#ifndef __V6FS_H__
#define __V6FS_H__

#include <stdint.h>

struct stat;
struct timespec;
struct statvfs;

enum {

    /** Filesystem block size
     */
    V6FS_BLOCK_SIZE         = 512,

    /** Maximum filesystem size, in blocks.
     *  Limited to 2^16-1 due to 2-byte block numbers used throughout code.
     * 
     *  Note that limit is 2^16-1 even though much of code uses signed 16-bit ints for 
     *  block numbers.  This is because, in places where inequality comparisions are
     *  performed, the code uses char * as a poor man's unsigned integer (e.g. see the
     *  code in rp.c where it checks the block number against the size of a partition).
     */
    V6FS_MAX_FS_SIZE        = UINT16_MAX,

    /** Minimum filesystem size, in blocks.
     *  (boot block + superblock + inode block + root directory block + 1 block for file data)
     */
    V6FS_MIN_FS_SIZE        = 5,

    /** Number of inodes per block.
     */
    V6FS_INODES_PER_BLOCK   = 16,

    /** Maximum size of the inode table, in blocks.
     *  Limited to 4096 due to 2-byte inode number used in on-disk directory structure.
     */
    V6FS_MAX_ITABLE_SIZE    = 65536/V6FS_INODES_PER_BLOCK,

    /** Minimum size of the inode table, in blocks.
     */
    V6FS_MIN_ITABLE_SIZE    = 1,

    /** Maximum value for uid/gid.
     */
    V6FS_MAX_UID_GID        = UINT8_MAX,

    /** Maximum value for freelist interleave parameter n
     */
    V6FS_MAXFN              = 100,
};

/* free list interleave parameters */
struct v6fs_flparams
{
    uint16_t m;
    uint16_t n;
};

typedef int (*v6fs_enum_dir_funct)(const char *entryname, const struct stat *statbuf, void *context);

extern int v6fs_init(int readonly);
extern int v6fs_shutdown();
extern int v6fs_mkfs(uint16_t fssize, uint16_t isize, const struct v6fs_flparams *flparams);
extern int v6fs_open(const char * name, int flags, mode_t mode);
extern int v6fs_close(int fd);
extern off_t v6fs_seek(int fd, off_t offset, int whence);
extern int v6fs_link(const char *oldpath, const char *newpath);
extern int v6fs_mknod(const char *pathname, mode_t mode, dev_t dev);
extern ssize_t v6fs_read(int fd, void *buf, size_t count);
extern ssize_t v6fs_pread(int fd, void *buf, size_t count, off_t offset);
extern ssize_t v6fs_write(int fd, const void *buf, size_t count);
extern ssize_t v6fs_pwrite(int fd, const void *buf, size_t count, off_t offset);
extern int v6fs_truncate(const char *pathname, off_t length);
extern int v6fs_stat(const char *pathname, struct stat *statbuf);
extern int v6fs_unlink(const char *pathname);
extern int v6fs_rename(const char *oldpath, const char *newpath);
extern int v6fs_chmod(const char *pathname, mode_t mode);
extern int v6fs_chown(const char *pathname, uid_t owner, gid_t group);
extern int v6fs_utimens(const char *filename, const struct timespec times[2]);
extern int v6fs_access(const char *pathname, int mode);
extern int v6fs_mkdir(const char *pathname, mode_t mode);
extern int v6fs_rmdir(const char *pathname);
extern int v6fs_enumdir(const char *pathname, v6fs_enum_dir_funct enum_funct, void *context);
extern int v6fs_sync();
extern int v6fs_statfs(const char *pathname, struct statvfs *buf);
extern int v6fs_setreuid(uid_t ruid, uid_t euid);
extern int v6fs_setregid(gid_t rgid, gid_t egid);
extern int v6fs_adduidmap(uid_t hostuid, uint32_t fsuid);
extern int v6fs_addgidmap(uid_t hostgid, uint32_t fsgid);

#endif /* __V6FS_H__ */
