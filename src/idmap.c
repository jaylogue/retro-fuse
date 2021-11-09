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
 * @file  UID/GID mapping table implementation
 */

#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

#include "idmap.h"

enum {
    MAP_SIZE_INC = 128
};

/** Add an entry to a UID/GID mapping table.
 */
int idmap_addidmap(struct idmap * map, uint32_t hostid, uint32_t fsid)
{
    if (fsid > map->maxfsid)
        return -EINVAL;
    if (map->entries == NULL || (map->entrycount % MAP_SIZE_INC) == 0) {
        map->entries = (struct idmap_entry *)realloc(map->entries, 
            sizeof(struct idmap_entry) * (map->entrycount + MAP_SIZE_INC));
        if (map->entries == NULL)
            return -ENOMEM;
    }
    map->entries[map->entrycount].hostid = hostid;
    map->entries[map->entrycount].fsid = fsid;
    map->entrycount++;
    return 0;
}

/** Map a filesystem UID/GID to a correspondign host id.
 */
uint32_t idmap_tohostid(struct idmap * map, uint32_t fsid)
{
    for (size_t i = 0; i < map->entrycount; i++)
        if (map->entries[i].fsid == fsid)
            return map->entries[i].hostid;
    if (fsid == map->defaultfsid)
        return map->defaulthostid;
    if (fsid > map->maxfsid)
        return map->defaulthostid;
    return fsid;
}

/** Map a host UID/GID to a correspondign filesystem id.
 */
uint32_t idmap_tofsid(struct idmap * map, uint32_t hostid)
{
    for (size_t i = 0; i < map->entrycount; i++)
        if (map->entries[i].hostid == hostid)
            return map->entries[i].fsid;
    if (hostid == map->defaulthostid)
        return map->defaultfsid;
    if (hostid > map->maxfsid)
        return map->defaultfsid;
    return hostid;
}
