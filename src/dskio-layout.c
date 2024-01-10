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
 * @file  Disk I/O layer -- Layout table.
 */

#include "dskio-internal.h"

const struct dsk_layoutinfo dsk_layouts[] = {
    {
        .layout = dsk_layout_rawdisk,
        .partitioned = false,
        .interleave = -1,
        .geometry = NULL,
        .parttab = NULL,
    },
    {
        .layout = dsk_layout_trsxenix,
        .partitioned = true,
        .interleave = 2,
        .geometry = NULL,
        .parttab = NULL,
    },
    {
        .layout = dsk_layout_rk05,
        .partitioned = false,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 4872,
            .cylinders = 203,
            .heads = 2,
            .sectors = 12,
        },
        .parttab = NULL,
    },
    {
        .layout = dsk_layout_rl01,
        .partitioned = false,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 10240,
            .cylinders = 256,
            .heads = 2,
            .sectors = 20,
        },
        .parttab = NULL
    },
    {
        .layout = dsk_layout_rl02,
        .partitioned = false,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 20480,
            .cylinders = 512,
            .heads = 2,
            .sectors = 20,
        },
        .parttab = NULL
    },
    {
        .layout = dsk_layout_rp03_v6,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 81200,
            .cylinders = 406,
            .heads = 20,
            .sectors = 10,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,     .size = 40600 },
            { .num = 1, .offset = 40600, .size = 40600 },
            { .num = 2, .offset = 0,     .size = 9200  },
            { .num = 3, .offset = 72000, .size = 9200  },
            { .num = 4, .offset = 0,     .size = 65535 },
            { .num = 5, .offset = 15600, .size = 65535 },
            { .num = 6, .offset = 0,     .size = 15600 },
            { .num = 7, .offset = 65600, .size = 15600 },
            { .num = -1 }
        }
    },
    {
        .layout = dsk_layout_rp03_v7,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 81200,
            .cylinders = 406,
            .heads = 20,
            .sectors = 10,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,     .size = 81000 },
            { .num = 1, .offset = 0,     .size = 5000  },
            { .num = 2, .offset = 5000,  .size = 2000  },
            { .num = 3, .offset = 7000,  .size = 74000 },
            { .num = -1 }
        }
    },
    {
        .layout = dsk_layout_rp06_v6,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 340670,
            .cylinders = 815,
            .heads = 19,
            .sectors = 22,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,      .size = 9614  },
            { .num = 1, .offset = 18392,  .size = 65535 },
            { .num = 2, .offset = 48018,  .size = 65535 },
            { .num = 3, .offset = 149644, .size = 20900 },
            { .num = 4, .offset = 0,      .size = 40600 },
            { .num = 5, .offset = 41800,  .size = 40600 },
            { .num = 6, .offset = 83600,  .size = 40600 },
            { .num = 7, .offset = 125400, .size = 40600 },
            { .num = -1 }
        }
    },
    {
        .layout = dsk_layout_rp06_v7,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 340670,
            .cylinders = 815,
            .heads = 19,
            .sectors = 22,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,      .size = 9614   },
            { .num = 1, .offset = 9614,   .size = 8778   },
            { .num = 4, .offset = 18392,  .size = 161348 },
            { .num = 5, .offset = 179740, .size = 160930 },
            { .num = 6, .offset = 18392,  .size = 153406 },
            { .num = 7, .offset = 18392,  .size = 322278 },
            { .num = -1 }
        }
    },
    { .layout = dsk_layout_notspecified }
};
