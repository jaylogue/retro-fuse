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
 * @file  Undo adaptations defined in v7adapt.h, restoring access to 
 *        modern constructs, while still prevering access to select
 *        definitions from v7 code.
 */

#ifndef __V7UNADAPT_H__
#define __V7UNADAPT_H__


/* Define constructs that provide access to certain conflicting v7 macros 
 * even after their modern definitions have been restored.
  */

#ifdef major
inline int16_t v7_major(dev_t dev) { return major(dev); }
#endif

#ifdef minor
inline int16_t v7_minor(dev_t dev) { return minor(dev); }
#endif

#ifdef makedev
static inline dev_t v7_makedev(int16_t x, int16_t y) { return makedev(x, y); }
#endif

#ifdef S_IFMT
enum {
    V7_S_IFMT   = S_IFMT,
    V7_S_IFDIR  = S_IFDIR,
    V7_S_IFCHR  = S_IFCHR,
    V7_S_IFBLK  = S_IFBLK,
    V7_S_IFREG  = S_IFREG,
    V7_S_IFMPC  = S_IFMPC,
    V7_S_IFMPB  = S_IFMPB,
    V7_S_ISUID  = S_ISUID,
    V7_S_ISGID  = S_ISGID,
    V7_S_ISVTX  = S_ISVTX,
    V7_S_IREAD  = S_IREAD,
    V7_S_IWRITE = S_IWRITE,
    V7_S_IEXEC  = S_IEXEC
};
#endif /* S_IFMT */

#ifdef FREAD
enum {
    V7_FREAD    = FREAD,
    V7_FWRITE   = FWRITE
};
#endif /* FREAD */

/* Undefine name mapping macros
 */
#undef access
#undef alloc
#undef badblock
#undef bawrite
#undef bcopy
#undef bdevsw
#undef bdwrite
#undef bflush
#undef bfreelist
#undef binit
#undef bmap
#undef bread
#undef breada
#undef brelse
#undef buf
#undef buffers
#undef bwrite
#undef caddr_t
#undef callo
#undef callout
#undef cdevsw
#undef chmod
#undef chown
#undef close
#undef closef
#undef clrbuf
#undef copyout
#undef daddr_t
#undef dev_t
#undef devtab
#undef dinode
#undef direct
#undef falloc
#undef fblk
#undef file
#undef filsys
#undef free
#undef getblk
#undef geteblk
#undef geterror
#undef getf
#undef getfs
#undef hilo
#undef httab
#undef ialloc
#undef iexpand
#undef ifree
#undef iget
#undef iinit
#undef incore
#undef ino_t
#undef inode
#undef integ
#undef iodone
#undef iomove
#undef iowait
#undef iput
#undef itrunc
#undef iupdat
#undef link
#undef lshift
#undef maknode
#undef mapfree
#undef max
#undef min
#undef mknod
#undef mount
#undef namei
#undef nblkdev
#undef nchrdev
#undef notavail
#undef off_t
#undef open1
#undef openi
#undef owner
#undef panic
#undef plock
#undef prdev
#undef prele
#undef printf
#undef rablock
#undef rdwr
#undef readi
#undef readp
#undef rootdev
#undef rootdir
#undef schar
#undef sleep
#undef spl0
#undef spl6
#undef splx
#undef stat
#undef stat1
#undef suser
#undef suword
#undef swbuf
#undef time
#undef time_t
#undef tloop
#undef tmtab
#undef u
#undef uchar
#undef ufalloc
#undef unlink
#undef update
#undef updlock
#undef user
#undef wakeup
#undef wdir
#undef writei
#undef writep
#undef xrele

/* Restore modern definitions of macros that collide with v7 code.
 */
#pragma pop_macro("NULL")
#pragma pop_macro("NSIG")
#pragma pop_macro("SIGIOT")
#pragma pop_macro("SIGABRT")
#pragma pop_macro("EAGAIN")
#pragma pop_macro("FREAD")
#pragma pop_macro("FWRITE")
#pragma pop_macro("major")
#pragma pop_macro("minor")
#pragma pop_macro("makedev")
#pragma pop_macro("st_atime")
#pragma pop_macro("st_mtime")
#pragma pop_macro("st_ctime")
#pragma pop_macro("S_IFMT")
#pragma pop_macro("S_IFDIR")
#pragma pop_macro("S_IFCHR")
#pragma pop_macro("S_IFBLK")
#pragma pop_macro("S_IFREG")
#pragma pop_macro("S_ISUID")
#pragma pop_macro("S_ISGID")
#pragma pop_macro("S_ISVTX")
#pragma pop_macro("S_IREAD")
#pragma pop_macro("S_IWRITE")
#pragma pop_macro("S_IEXEC")
#pragma pop_macro("bcopy")

#endif /* __V7UNADAPT_H__ */