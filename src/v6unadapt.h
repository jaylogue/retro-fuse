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

#ifndef __V6UNADAPT_H__
#define __V6UNADAPT_H__


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
#undef callo    
#undef callout  
#undef cdevsw   
#undef chmod    
#undef chown    
#undef close    
#undef closef   
#undef closei   
#undef clrbuf   
#undef devtab   
#undef dpadd    
#undef dpcmp    
#undef falloc   
#undef file     
#undef file     
#undef filsys   
#undef free     
#undef getblk   
#undef geterror 
#undef getf     
#undef getfs    
#undef hilo     
#undef httab    
#undef ialloc   
#undef ifree    
#undef iget     
#undef iinit    
#undef incore   
#undef inode    
#undef integ    
#undef iodone   
#undef iomove   
#undef iowait   
#undef iput     
#undef itrunc   
#undef iupdat   
#undef ldiv     
#undef link     
#undef lrem     
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
#undef open1    
#undef openi    
#undef owner    
#undef panic    
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
#undef stat1    
#undef suser    
#undef suword   
#undef swbuf    
#undef time     
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

#ifdef FREAD
enum {
    V6_FREAD    = FREAD,
    V6_FWRITE   = FWRITE
};
#endif /* FREAD */

/* Restore modern definitions of macros that collide with v7 code.
 */
#pragma pop_macro("NULL")
#pragma pop_macro("NSIG")
#pragma pop_macro("SIGIOT")
#pragma pop_macro("SIGABRT")
#pragma pop_macro("EAGAIN")
#pragma pop_macro("FREAD")
#pragma pop_macro("FWRITE")

#endif /* __V6UNADAPT_H__ */