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
 * @file  UID/GID mapping table
 */

#ifndef __IDMAP_H__
#define __IDMAP_H__

#include <stdint.h>
#include <unistd.h>

struct idmap_entry {
    uint32_t hostid;
    uint32_t fsid;
};

/** Map relating host UID/GID values to corresponding filesystem ids
 */
struct idmap {
    struct idmap_entry * entries;
    size_t entrycount;
    uint32_t maxfsid;
    uint32_t defaulthostid;
    uint32_t defaultfsid;
};

extern int idmap_addidmap(struct idmap * map, uint32_t hostid, uint32_t fsid);
extern uint32_t idmap_tohostid(struct idmap * map, uint32_t fsid);
extern uint32_t idmap_tofsid(struct idmap * map, uint32_t hostid);

#endif /* __IDMAP_H__ */