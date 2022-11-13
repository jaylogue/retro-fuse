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
 * @file  Filesystem byte order definitions and utility functions.
 */

#ifndef __FSUTIL_H__
#define __FSUTIL_H__

#include "stdint.h"
#include "endianness.h"

/** Filesystem byte order options
 */
enum fs_byteorder {
    fs_byteorder_unknown = 0,
    fs_byteorder_le,
    fs_byteorder_be,
    fs_byteorder_pdp
};

/* Byte re-ordering functions */

#if defined(__LITTLE_ENDIAN__)

static inline int16_t fs_htole_i16(int16_t v)       { return v; }
static inline int16_t fs_htobe_i16(int16_t v)       { return (int16_t)bswap16((uint16_t)v); }
static inline int16_t fs_htopdp_i16(int16_t v)      { return v; }

static inline uint16_t fs_htole_u16(uint16_t v)     { return v; }
static inline uint16_t fs_htobe_u16(uint16_t v)     { return bswap16(v); }
static inline uint16_t fs_htopdp_u16(uint16_t v)    { return v; }

static inline int32_t fs_htole_i32(int32_t v)       { return v; }
static inline int32_t fs_htobe_i32(int32_t v)       { return (int32_t)bswap32((uint32_t)v); }
static inline int32_t fs_htopdp_i32(int32_t v)      { uint32_t uv = (uint32_t)v; 
                                                      return (int32_t)((uv << 16) | (uv >> 16)); }

#elif defined(__BIG_ENDIAN__)

static inline int16_t fs_htole_i16(int16_t v)       { return (int16_t)bswap16((uint16_t)v); }
static inline int16_t fs_htobe_i16(int16_t v)       { return v; }
static inline int16_t fs_htopdp_i16(int16_t v)      { return (int16_t)bswap16((uint16_t)v); }

static inline uint16_t fs_htole_u16(uint16_t v)     { return bswap16(v); }
static inline uint16_t fs_htobe_u16(uint16_t v)     { return v; }
static inline uint16_t fs_htopdp_u16(uint16_t v)    { return bswap16(v); }

static inline int32_t fs_htole_i32(int32_t v)       { return (int32_t)bswap32((uint32_t)v); }
static inline int32_t fs_htobe_i32(int32_t v)       { return v; }
static inline int32_t fs_htopdp_i32(int32_t v)      { uint32_t uv = bswap32((uint32_t)v); 
                                                      return (int32_t)((uv << 16) | (uv >> 16)); }

#endif /* defined(__BIG_ENDIAN__) */

#endif /* __FSUTIL_H__ */
