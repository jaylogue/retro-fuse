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
 * @file  Definitions for known filesystem types.
 */

#ifndef __FSTYPE_H__
#define __FSTYPE_H__

#include "fsbyteorder.h"

/** Known filesystem formats
 */
enum fs_type {

    fs_type_unknown = 0,

    /** Unix v6 for the PDP-11 
     *
     *  v6 superblock structure, PDP-11 endian, BSIZE = 512
     */
    fs_type_v6,

    /** Unix v7 for the PDP-11 
     *
     *  v7 superblock structure, PDP-11 endian, BSIZE = 512
     */
    fs_type_v7,

    /** Unix System III for the PDP-11 
     *
     *  System III superblock structure, PDP-11 endian, BSIZE = 512
     */
    fs_type_sys3,

    /** 2.9BSD for the PDP-11 
     *
     *  2.9BSD superblock structure, PDP-11 endian, BSIZE = 512
     */
    fs_type_29bsd,

    /** 2.11BSD for the PDP-11 
     *
     *  2.11BSD superblock structure, PDP-11 endian, BSIZE = 512
     */
    fs_type_211bsd,

    /** Microsoft Xenix version 2.x for little-endian machines
     *
     *  Xenix 2.x superblock structure, little-endian, BSIZE = 512
     *
     *  (E.g.: Altos Xenix 2.x for ACS 8600, Altos 586, etc.)
     */
    fs_type_msxenix2_le,

    /** Microsoft Xenix version 2.x for big-endian machines
     *
     *  Xenix 2.x superblock structure, big-endian, BSIZE = 512
     *
     *  (E.g. TRS-XENIX v01.xx for TRS-80 Model 16/Tandy 6000, etc.)
     */
    fs_type_msxenix2_be,

    /** Microsoft Xenix version 3.x for little-endian machines
     *
     *  Xenix 3.x superblock structure, little-endian, BSIZE = 512
     */
    fs_type_msxenix3_le,

    /** Microsoft Xenix version 3.x for big-endian machines
     *
     *  Xenix 3.x superblock structure, big-endian, BSIZE = 512
     * 
     *  (E.g.: Tandy 68000/XENIX v03.xx, Xenix for Apple Lisa, etc.)
     */
    fs_type_msxenix3_be,

    /** IBM PC XENIX 1.x/2.x 
     *
     *  Xenix 3.x superblock structure with NICFREE = 100, little-endian, BSIZE = 1024
     */
    fs_type_ibmpcxenix,
};

/* Utility Functions */

static inline int fs_byteorder(int type)
{
    switch (type) {
    case fs_type_msxenix2_le:
    case fs_type_msxenix3_le:
    case fs_type_ibmpcxenix:
        return fs_byteorder_le;
    case fs_type_msxenix2_be:
    case fs_type_msxenix3_be:
        return fs_byteorder_be;
    case fs_type_v6:
    case fs_type_v7:
    case fs_type_sys3:
    case fs_type_29bsd:
    case fs_type_211bsd:
        return fs_byteorder_pdp;
    default:
        return fs_byteorder_unknown;
    }
}

static inline int fs_blocksize(int type)
{
    switch (type) {
    case fs_type_ibmpcxenix:
        return 1024;
    default:
        return 512;
    }
}

#endif /* __FSTYPE_H__ */
