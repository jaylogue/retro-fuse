#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fcntl.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <fuse.h>

#include <v6fs.h>
#include <dskio.h>

static struct v6fuse_config {
    const char *filename;
} v6fuse_config;

static const struct fuse_opt v6fuse_options[] = {
    { "--file=%s", offsetof(struct v6fuse_config, filename), 0 },
    FUSE_OPT_END
};

static void * v6fuse_init(struct fuse_conn_info * conn)
{
    /* TODO: fail if cfg->direct_io requested */

    /* initialize disk i/o subssytem */
    dsk_open(v6fuse_config.filename);

    /* initialize the unix kernel data structures */
    v6fs_init();

    return NULL;
}

static void v6fuse_destroy(void * userdata)
{
    v6fs_sync();
}

static int v6fuse_getattr(const char *pathname, struct stat *statbuf)
{
    return v6fs_stat(pathname, statbuf);
}

static int v6fuse_access(const char *pathname, int mode)
{
    return v6fs_access(pathname, mode);
}

struct v6fuse_adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

static int v6fuse_filldir(const char *entryname, const struct stat *statbuf, void *_context)
{
    struct v6fuse_adddir_context * context = (struct v6fuse_adddir_context *)_context;
    context->filler(context->buf, entryname, statbuf, 0);
    return 0;
}

static int v6fuse_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct v6fuse_adddir_context context = {
        .buf = buf,
        .filler = filler
    };
    return v6fs_enumdir(pathname, v6fuse_filldir, &context);
}

static int v6fuse_open(const char *pathname, struct fuse_file_info *fi)
{
    int res = v6fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int v6fuse_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    return v6fs_pread((int)fi->fh, buf, size, offset);
}

static int v6fuse_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    return v6fs_pwrite((int)fi->fh, buf, size, offset);
}

static int v6fuse_truncate(const char *pathname, off_t length)
{
    return v6fs_truncate(pathname, length);
}

static int v6fuse_flush(const char *pathname, struct fuse_file_info *fi)
{
    v6fs_sync();
    return 0;
}

static int v6fuse_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    v6fs_sync();
    return 0;
}

static int v6fuse_release(const char *pathname, struct fuse_file_info *fi)
{
    return v6fs_close((int)fi->fh);
}

static int v6fuse_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    return v6fs_mknod(pathname, mode, dev);
}

static int v6fuse_mkdir(const char *pathname, mode_t mode)
{
    return v6fs_mkdir(pathname, mode);
}

static int v6fuse_rmdir(const char *pathname)
{
    return v6fs_rmdir(pathname);
}

static int v6fuse_link(const char *oldpath, const char *newpath)
{
    return v6fs_link(oldpath, newpath);
}

static int v6fuse_unlink(const char *pathname)
{
    return v6fs_unlink(pathname);
}

static int v6fuse_chmod(const char *pathname, mode_t mode)
{
    return v6fs_chmod(pathname, mode);
}

static int v6fuse_chown(const char *pathname, uid_t uid, gid_t gid)
{
    return v6fs_chown(pathname, uid, gid);
}

static int v6fuse_utimens(const char *pathname, const struct timespec tv[2])
{
    return v6fs_utimens(pathname, tv);
}

static const struct fuse_operations v6fuse_ops = 
{
    .init       = v6fuse_init,
    .destroy    = v6fuse_destroy,
    .getattr    = v6fuse_getattr,
    .access     = v6fuse_access,
    .readdir    = v6fuse_readdir,
    .open       = v6fuse_open,
    .read       = v6fuse_read,
    .write      = v6fuse_write,
    .truncate   = v6fuse_truncate,
    .flush      = v6fuse_flush,
    .fsync      = v6fuse_fsync,
    .release    = v6fuse_release,
    .mknod      = v6fuse_mknod,
    .mkdir      = v6fuse_mkdir,
    .rmdir      = v6fuse_rmdir,
    .link       = v6fuse_link,
    .unlink     = v6fuse_unlink,
    .chmod      = v6fuse_chmod,
    .chown      = v6fuse_chown,
    .utimens    = v6fuse_utimens,
};

#if 0
int print_dir(const char * entryname, const struct stat * statbuf, void * context)
{
    printf("%s\n", entryname);
    return 0;
}
#endif

int main(int argc, char *argv[])
{
#if 1
	int res;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    memset(&v6fuse_config, 0, sizeof(v6fuse_config));

	/* Parse options */
	if (fuse_opt_parse(&args, &v6fuse_config, v6fuse_options, NULL) == -1)
		return 1;

	res = fuse_main(args.argc, args.argv, &v6fuse_ops, NULL);
	fuse_opt_free_args(&args);
	return res;
#endif

#if 0
    dsk_open("test.dsk");

    /* initialize the unix kernel data structures */
    v6fs_init();

    // v6fs_enumdir("/", print_dir, NULL);

#if 0
    {
        char buf[8192];

        int fd = v6fs_open("/s1/ls.c", O_RDONLY, 0);
        if (fd < 0) {
            printf("v6fs_open failed: %d\n", fd);
            return -1;
        }
        
        ssize_t res = v6fs_read(fd, buf, sizeof(buf)-1);
        if (res < 0) {
            printf("v6fs_read failed: %ld\n", res);
            return -1;
        }

        v6fs_close(fd);

        buf[res] = 0;

        printf("%s", buf);

    }
#endif

#if 1

    {
        int res = v6fs_mkdir("/test", 0777);
        if (res < 0) {
            printf("v6fs_mkdir failed: %ld\n", res);
            return -1;
        }

        res = v6fs_rmdir("/test");
        if (res < 0) {
            printf("v6fs_rmdir failed: %ld\n", res);
            return -1;
        }
    }

#endif

#endif
}