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

#ifndef __BSD211FS_H__
#define __BSD211FS_H__

#include <stdint.h>

struct stat;
struct timespec;
struct statvfs;

enum {

    /** Filesystem block size
     */
    BSD211FS_BLOCK_SIZE         = 1024,

    /** Maximum filesystem size, in blocks.
     *  Limited to 2^31-1 due to int32_t daddr_t.
     */
    BSD211FS_MAX_FS_SIZE        = 2147483647,

    /** Minimum filesystem size, in blocks.
     *  (boot block + superblock + inode block + root directory block + lost+found directory block + 1 block for file data)
     */
    BSD211FS_MIN_FS_SIZE        = 6,

    /** Number of inodes per block.
     */
    BSD211FS_INODES_PER_BLOCK   = 16,

    /** Maximum size of the inode table, in blocks.
     *  Limited to 4093 based on logic in mkfs command.
     */
    BSD211FS_MAX_ITABLE_SIZE    = 65500/BSD211FS_INODES_PER_BLOCK,

    /** Minimum size of the inode table, in blocks.
     */
    BSD211FS_MIN_ITABLE_SIZE    = 1,

    /** Maximum value for uid/gid.
     */
    BSD211FS_MAX_UID_GID        = INT16_MAX,

    /** Default value for freelist interleave parameter n (modulus)
     */
    BSD211FS_DEFAULT_FN         = 100,

    /** Default value for freelist interleave parameter m (gap)
     */
    BSD211FS_DEFAULT_FM         = 2,

    /** Maximum value for freelist interleave parameter n (modulus)
     */
    BSD211FS_MAX_FN             = 750,

    /** Maximum number of supplementary group ids (not counting the effective group id)
     */
    BSD211FS_NGROUPS_MAX        = 15,
};

/** Free list interleave parameters */
struct bsd211fs_flparams
{
    uint16_t m;
    uint16_t n;
};

typedef int (*bsd211fs_enum_dir_funct)(const char *entryname, const struct stat *statbuf, void *context);

extern int bsd211fs_init(int readonly);
extern int bsd211fs_shutdown();
extern int bsd211fs_mkfs(uint32_t fssize, uint32_t iratio, const struct bsd211fs_flparams *flparams);
extern int bsd211fs_open(const char * name, int flags, mode_t mode);
extern int bsd211fs_close(int fd);
extern off_t bsd211fs_seek(int fd, off_t offset, int whence);
extern int bsd211fs_link(const char *oldpath, const char *newpath);
extern int bsd211fs_mknod(const char *pathname, mode_t mode, dev_t dev);
extern ssize_t bsd211fs_read(int fd, void *buf, size_t count);
extern ssize_t bsd211fs_pread(int fd, void *buf, size_t count, off_t offset);
extern ssize_t bsd211fs_write(int fd, const void *buf, size_t count);
extern ssize_t bsd211fs_pwrite(int fd, const void *buf, size_t count, off_t offset);
extern int bsd211fs_truncate(const char *pathname, off_t length);
extern int bsd211fs_ftruncate(int fd, off_t length);
extern int bsd211fs_stat(const char *pathname, struct stat *statbuf);
extern int bsd211fs_unlink(const char *pathname);
extern int bsd211fs_rename(const char *oldpath, const char *newpath);
extern int bsd211fs_chmod(const char *pathname, mode_t mode);
extern int bsd211fs_chown(const char *pathname, uid_t owner, gid_t group);
extern int bsd211fs_utimens(const char *filename, const struct timespec times[2]);
extern int bsd211fs_access(const char *pathname, int mode);
extern ssize_t bsd211fs_readlink(const char *pathname, char *buf, size_t bufsiz);
extern int bsd211fs_symlink(const char *target, const char *linkpath);
extern int bsd211fs_mkdir(const char *pathname, mode_t mode);
extern int bsd211fs_rmdir(const char *pathname);
extern int bsd211fs_enumdir(const char *pathname, bsd211fs_enum_dir_funct enum_funct, void *context);
extern int bsd211fs_sync();
extern int bsd211fs_fsync(int fd);
extern int bsd211fs_statfs(const char *pathname, struct statvfs *buf);
extern int bsd211fs_setreuid(uid_t ruid, uid_t euid);
extern int bsd211fs_setregid(gid_t rgid, gid_t egid);
extern int bsd211fs_setgroups(size_t size, const gid_t *list);
extern void bsd211fs_refreshgroups();
extern int bsd211fs_adduidmap(uid_t hostuid, uint32_t fsuid);
extern int bsd211fs_addgidmap(uid_t hostgid, uint32_t fsgid);

#endif /* __BSD211FS_H__ */