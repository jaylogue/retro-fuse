#ifndef __V6FS_H__
#define __V6FS_H__

struct stat;
struct timespec;
struct statvfs;

typedef int (*v6fs_enum_dir_funct)(const char *entryname, const struct stat *statbuf, void *context);

extern void v6fs_init(int readonly);
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
extern int v6fs_chmod(const char *pathname, mode_t mode);
extern int v6fs_chown(const char *pathname, uid_t owner, gid_t group);
extern int v6fs_utimens(const char *filename, const struct timespec times[2]);
extern int v6fs_access(const char *pathname, int mode);
extern int v6fs_mkdir(const char *pathname, mode_t mode);
extern int v6fs_rmdir(const char *pathname);
extern int v6fs_enumdir(const char *pathname, v6fs_enum_dir_funct enum_funct, void *context);
extern void v6fs_sync();
extern int v6fs_statfs(const char *pathname, struct statvfs *buf);
extern int v6fs_setreuid(uid_t ruid, uid_t euid);
extern int v6fs_setregid(gid_t rgid, gid_t egid);
extern int v6fs_adduidmap(uid_t hostuid, char fsuid);
extern int v6fs_addgidmap(uid_t hostgid, char fsgid);

#endif /* __V6FS_H__ */
