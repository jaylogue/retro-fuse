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

#include <fuse.h>
#include <inttypes.h>
#include <dskio.h>

#define RETROFUSE_VERSION 8

/** Configuration parameters for the UFS mkfs operation
 */
struct retrofuse_mkfsconfig {
    uint32_t isize;
    uint16_t m;
    uint16_t n;
};

/** Filesystem handler configuration parameters
 * 
 * This structure contains a union of all possible configuration 
 * parameters for all filesystem handlers.  Individual handlers
 * may only use a subset of these values.
 */
struct retrofuse_config {
    const char *dskfilename;
    const char *mountpoint;
    struct dsk_config dskcfg;
    int fstype;
    struct retrofuse_mkfsconfig mkfscfg;
    int overwrite;
    int readonly;
    int foreground;
    int debug;
    int showversion;
    int showhelp;
    int mkfs;
};

#define RETROFUSE_OPT(TEMPLATE, FIELD, VALUE) \
    { TEMPLATE, offsetof(struct retrofuse_config, FIELD), VALUE }


extern const char *retrofuse_cmdname;
extern uint32_t retrofuse_fsblocksize;

extern const struct fuse_opt retrofuse_fuseopts[];
extern const struct fuse_operations retrofuse_fuseops;

extern void retrofuse_initconfig(struct retrofuse_config *cfg);
extern int retrofuse_checkconfig(struct retrofuse_config *cfg);
extern int retrofuse_checkconfig_v7(struct retrofuse_config *cfg);
extern int retrofuse_mkfs(struct retrofuse_config *cfg);
extern int retrofuse_mountfs(struct retrofuse_config *cfg);
extern int retrofuse_parseopt(void *data, const char *arg, int key, struct fuse_args *args);
extern int retrofuse_parseopt_dskcontainer(const char *arg, int *outval);
extern int retrofuse_parseopt_mkfs(const char *arg, struct retrofuse_mkfsconfig *mkfscfg);
extern int retrofuse_parseopt_idmap(const char *arg, uint32_t maxid, int (*addidmap)(uid_t hostid, uint32_t fsid));
extern int retrofuse_parseopt_size(const char *arg, const char *optname, uint32_t unitSize, off_t *outval);
extern void retrofuse_showhelp();

#endif /* __FUSECOMMON_H__ */