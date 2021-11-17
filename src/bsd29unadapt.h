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
 * @file  Undo adaptations defined in bsd29adapt.h, restoring access to 
 *        modern constructs, while still prevering access to select
 *        definitions from 2.9BSD code.
 */

#ifndef __BSD29UNADAPT_H__
#define __BSD29UNADAPT_H__

/* Define constructs that provide access to certain conflicting 2.9BSD macros 
 * even after their modern definitions have been restored.
  */

#ifdef major
inline int16_t bsd29_major(dev_t dev) { return major(dev); }
#endif

#ifdef minor
inline int16_t bsd29_minor(dev_t dev) { return minor(dev); }
#endif

#ifdef makedev
static inline dev_t bsd29_makedev(int16_t x, int16_t y) { return makedev(x, y); }
#endif

#ifdef fsbtodb
static inline bsd29_daddr_t bsd29_fsbtodb(bsd29_daddr_t b) { return fsbtodb(b); }
#endif

#ifdef dbtofsb
static inline bsd29_daddr_t bsd29_dbtofsb(bsd29_daddr_t b) { return dbtofsb(b); }
#endif

#ifdef S_IFMT
enum {
    BSD29_S_IFMT    = S_IFMT,
    BSD29_S_IFDIR   = S_IFDIR,
    BSD29_S_IFCHR   = S_IFCHR,
    BSD29_S_IFBLK   = S_IFBLK,
    BSD29_S_IFREG   = S_IFREG,
    BSD29_S_IFLNK   = S_IFLNK,
    BSD29_S_IFMPC   = S_IFMPC,
    BSD29_S_IFMPB   = S_IFMPB,
    BSD29_S_ISUID   = S_ISUID,
    BSD29_S_ISGID   = S_ISGID,
    BSD29_S_ISVTX   = S_ISVTX,
    BSD29_S_IREAD   = S_IREAD,
    BSD29_S_IWRITE  = S_IWRITE,
    BSD29_S_IEXEC   = S_IEXEC
};
#endif /* S_IFMT */

#ifdef FREAD
enum {
    BSD29_FREAD     = FREAD,
    BSD29_FWRITE    = FWRITE
};
#endif /* FREAD */

#ifdef LOOKUP
enum {
    BSD29_LOOKUP    = LOOKUP,
    BSD29_CREATE    = CREATE,
    BSD29_DELETE    = DELETE
};
#endif /* LOOKUP */

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
#undef bhash
#undef binit
#undef bmap
#undef bread
#undef breada
#undef brelse
#undef bsize
#undef buf
#undef buffers
#undef bunhash
#undef bwrite
#undef caddr_t
#undef callo
#undef callout
#undef cdevsw
#undef chmod
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
#undef fileNFILE
#undef filsys
#undef free
#undef fserr
#undef fubyte
#undef getblk
#undef geteblk
#undef geterror
#undef getf
#undef getfs
#undef ialloc
#undef iexpand
#undef ifind
#undef ifree
#undef iget
#undef ihinit
#undef iinit
#undef incore
#undef ino_t
#undef inode
#undef inodeNINODE
#undef iodone
#undef iomove
#undef iowait
#undef iput
#undef itrunc
#undef iupdat
#undef link
#undef maknode
#undef mapin
#undef mapout
#undef mknod
#undef mount
#undef mountNMOUNT
#undef namei
#undef nblkdev
#undef nbuf
#undef nchrdev
#undef nmount
#undef notavail
#undef off_t
#undef open1
#undef openi
#undef own
#undef owner
#undef panic
#undef plock
#undef prele
#undef printf
#undef rablock
#undef rdwr
#undef readi
#undef readlink
#undef readp
#undef rootdev
#undef rootdir
#undef schar
#undef SEG5
#undef size_t
#undef sleep
#undef spl0
#undef spl6
#undef _spl0
#undef _spl6
#undef stat
#undef stat1
#undef suser
#undef swbuf
#undef symchar
#undef symlink
#undef tablefull
#undef time
#undef time_t
#undef tloop
#undef u
#undef uchar
#undef ufalloc
#undef unlink
#undef update
#undef updlock
#undef uprintf
#undef user
#undef wakeup
#undef wdir
#undef writei
#undef writep
#undef xrele

/* Restore modern definitions of macros that collide with 2.9BSD code.
 */
#pragma pop_macro("NULL")
#pragma pop_macro("EAGAIN")
#pragma pop_macro("ELOOP")
#pragma pop_macro("FREAD")
#pragma pop_macro("FWRITE")
#pragma pop_macro("F_OK")
#pragma pop_macro("X_OK")
#pragma pop_macro("W_OK")
#pragma pop_macro("R_OK")
#pragma pop_macro("O_RDONLY")
#pragma pop_macro("O_WRONLY")
#pragma pop_macro("O_RDWR")
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
#pragma pop_macro("S_IFLNK")
#pragma pop_macro("S_ISUID")
#pragma pop_macro("S_ISGID")
#pragma pop_macro("S_ISVTX")
#pragma pop_macro("S_IREAD")
#pragma pop_macro("S_IWRITE")
#pragma pop_macro("S_IEXEC")
#pragma pop_macro("bcopy")

#endif /* __BSD29UNADAPT_H__ */