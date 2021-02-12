#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#define V6FS_VERSION 1

#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <fuse.h>

#include <v6fs.h>
#include <dskio.h>

struct v6fuse_config {
    const char *dskfilename;
    uint32_t dsksize;
    const char *mountpoint;
    int foreground;
    int debug;
    int showversion;
    int showhelp;
};

#define V6FUSE_OPT(TEMPLATE, FIELD, VALUE) \
    { TEMPLATE, offsetof(struct v6fuse_config, FIELD), VALUE }

static const struct fuse_opt v6fuse_options[] = {
    V6FUSE_OPT("--size=%u", dsksize, 0),
    V6FUSE_OPT("-f", foreground, 1),
    V6FUSE_OPT("--foreground", foreground, 1),
    V6FUSE_OPT("-d", debug, 1),
    V6FUSE_OPT("--debug", debug, 1),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("--debug", FUSE_OPT_KEY_KEEP),
    V6FUSE_OPT("-h", showhelp, 1),
    V6FUSE_OPT("--help", showhelp, 1),
    V6FUSE_OPT("-V", showversion, 1),
    V6FUSE_OPT("--version", showversion, 1),
    FUSE_OPT_END
};

static const char *v6fuse_cmdname;

static int v6fuse_parseopt(void* data, const char* arg, int key, struct fuse_args* args)
{
    struct v6fuse_config *cfg = (struct v6fuse_config *)data;

    switch (key)
    {
    case FUSE_OPT_KEY_OPT:
        return 1;
    case FUSE_OPT_KEY_NONOPT:
        if (cfg->dskfilename == NULL) {
            cfg->dskfilename = arg;
            return 0;
        }
        if (cfg->mountpoint == NULL) {
            cfg->mountpoint = arg;
            return 0;
        }
        fprintf(stderr, "%s: Unexpected argument: %s\n", v6fuse_cmdname, arg);
        return -1;
    default:
        fprintf(stderr, "%s: Invalid option key: %d\n", v6fuse_cmdname, key);
        return -1;
    }
}

static void v6fuse_showhelp()
{
    printf(
        "usage: %s [options] <device-or-image-name> <mount-point>\n"
        "\n"
        "options:\n"
        "    --size                     size of device in blocks\n"
        "    -o [mount-options]         comma-separated list of mount options\n"
        "                                 (see man 8 mount for details)\n"
        "    -o [fuse-options]          comma-separated list of FUSE options\n"
        "                                 (see man 8 mount.fuse for details)\n"
        "    -f  --foreground           run in foreground\n"
        "    -d  --debug                enable debug output (implies -f)\n"
        "    -V  --version              print version information\n"
        "    -h  --help                 print help\n",
        v6fuse_cmdname);
}

static void v6fuse_showversion()
{
    printf("v6fs version: %d\n", V6FS_VERSION);
    printf("FUSE library version: %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
}

static void v6fuse_setfscontext()
{
    struct fuse_context *context = fuse_get_context();
    v6fs_seteuid(context->uid);
    v6fs_setegid(context->gid);
}

static void * v6fuse_init(struct fuse_conn_info * conn)
{
    struct fuse_context *context = fuse_get_context();
    struct v6fuse_config *cfg = (struct v6fuse_config *)context->private_data;

    /* Update FUSE configuration based on capabilities of filesystem. */
    conn->want = (conn->capable & (FUSE_CAP_ATOMIC_O_TRUNC|FUSE_CAP_EXPORT_SUPPORT|FUSE_CAP_BIG_WRITES));
    conn->async_read = 0;

    /* initialize disk i/o subssytem */
    dsk_open(cfg->dskfilename);

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
    v6fuse_setfscontext();
    return v6fs_stat(pathname, statbuf);
}

static int v6fuse_access(const char *pathname, int mode)
{
    v6fuse_setfscontext();
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
    v6fuse_setfscontext();
    return v6fs_enumdir(pathname, v6fuse_filldir, &context);
}

static int v6fuse_open(const char *pathname, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    int res = v6fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int v6fuse_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    return v6fs_pread((int)fi->fh, buf, size, offset);
}

static int v6fuse_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    return v6fs_pwrite((int)fi->fh, buf, size, offset);
}

static int v6fuse_truncate(const char *pathname, off_t length)
{
    v6fuse_setfscontext();
    return v6fs_truncate(pathname, length);
}

static int v6fuse_flush(const char *pathname, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    v6fs_sync();
    return 0;
}

static int v6fuse_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    v6fs_sync();
    return 0;
}

static int v6fuse_release(const char *pathname, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    return v6fs_close((int)fi->fh);
}

static int v6fuse_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    v6fuse_setfscontext();
    return v6fs_mknod(pathname, mode, dev);
}

static int v6fuse_mkdir(const char *pathname, mode_t mode)
{
    v6fuse_setfscontext();
    return v6fs_mkdir(pathname, mode);
}

static int v6fuse_rmdir(const char *pathname)
{
    v6fuse_setfscontext();
    return v6fs_rmdir(pathname);
}

static int v6fuse_link(const char *oldpath, const char *newpath)
{
    v6fuse_setfscontext();
    return v6fs_link(oldpath, newpath);
}

static int v6fuse_unlink(const char *pathname)
{
    v6fuse_setfscontext();
    return v6fs_unlink(pathname);
}

static int v6fuse_chmod(const char *pathname, mode_t mode)
{
    v6fuse_setfscontext();
    return v6fs_chmod(pathname, mode);
}

static int v6fuse_chown(const char *pathname, uid_t uid, gid_t gid)
{
    v6fuse_setfscontext();
    return v6fs_chown(pathname, uid, gid);
}

static int v6fuse_utimens(const char *pathname, const struct timespec tv[2])
{
    v6fuse_setfscontext();
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

    .flag_utime_omit_ok = 1
};

int main(int argc, char *argv[])
{
	int res = 0;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct v6fuse_config cfg;
    struct fuse_chan *chan = NULL;
    struct fuse *fuse = NULL;
    struct fuse_session *session = NULL;
    int sighandlersset = 0; 

    memset(&cfg, 0, sizeof(cfg));

    v6fuse_cmdname = rindex(argv[0], '/');
    if (v6fuse_cmdname == NULL)
        v6fuse_cmdname = argv[0];

    if (fuse_opt_parse(&args, &cfg, v6fuse_options, v6fuse_parseopt) == -1)
        goto exit;

    if (cfg.showhelp) {
        v6fuse_showhelp();
        goto exit;
    }

    if (cfg.showversion) {
        v6fuse_showversion();
        goto exit;
    }

    if (cfg.dskfilename == NULL) {
        fprintf(stderr, "%s: Please supply the name of the device or disk image to be mounted\n", v6fuse_cmdname);
        goto exit;
    }

    if (cfg.mountpoint == NULL) {
        fprintf(stderr, "%s: Please supply the mount point\n", v6fuse_cmdname);
        goto exit;
    }

    chan = fuse_mount(cfg.mountpoint, &args);
    if (chan == NULL) {
        fprintf(stderr, "%s: Failed to mount FUSE filesystem: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    fuse = fuse_new(chan, &args, &v6fuse_ops, sizeof(v6fuse_ops), &cfg);
    if (fuse == NULL) {
        fprintf(stderr, "%s: Failed to initialize FUSE connection: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    if (fuse_daemonize(cfg.foreground) != 0) {
        fprintf(stderr, "%s: Failed to daemonize FUSE process: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    session = fuse_get_session(fuse);
    if (fuse_set_signal_handlers(session) != 0) {
        fprintf(stderr, "%s: Failed to set FUSE signal handlers: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }
    sighandlersset = 1;

    if (fuse_loop(fuse) != 0) {
        fprintf(stderr, "%s: fuse_loop() failed: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    res = 0;

exit:
    if (sighandlersset)
        fuse_remove_signal_handlers(session);
    if (chan != NULL)
        fuse_unmount(cfg.mountpoint, chan);
    if (fuse != NULL)
        fuse_destroy(fuse);
    fuse_opt_free_args(&args);
	return res;
}