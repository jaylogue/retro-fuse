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
 * @file  Common FUSE filesystem definitions.
 */

#ifndef __FUSECOMMON_H__
#define __FUSECOMMON_H__

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <inttypes.h>

#define RETROFUSE_VERSION 5

/** retro-fuse configuration parameters.
 */
struct retrofuse_config {
    const char *dskfilename;
    uint32_t fssize;
    uint32_t fsoffset;
    const char *mountpoint;
    int readonly;
    int foreground;
    int debug;
    int showversion;
    int showhelp;
    int initfs;
    /* Note that this is a combination of init parameters used by
     * all filesystem types.  For each individual filesystem, some
     * fields may not be used. */
    struct {
        uint32_t isize;
        uint16_t m;
        uint16_t n;
    } initfsparams;
};

extern const char *retrofuse_cmdname;
extern const struct fuse_operations retrofuse_ops;
extern const uint32_t retrofuse_maxuidgid;

extern int retrofuse_parseinitfsopt(struct retrofuse_config *cfg, const char *arg);
extern int retrofuse_adduidmap(uid_t hostid, uint32_t fsid);
extern int retrofuse_addgidmap(gid_t hostid, uint32_t fsid);
extern int retrofuse_initfs(const struct retrofuse_config * cfg);
extern int retrofuse_mount(const struct retrofuse_config * cfg);
extern void retrofuse_showhelp();

#endif /* __FUSECOMMON_H__ */