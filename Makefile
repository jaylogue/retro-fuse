#
# Copyright 2021 Jay Logue
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


########### GENERAL SETTINGS ###########

.DEFAULT_GOAL := all
CC = cc
CPPFLAGS = -MMD -MP -I./src
CFLAGS = -std=c11 -g -Wall -O0 -fno-common
LIBS = -lfuse
OS := $(shell uname -s)
LD_MAP_FLAG = -Wl,-Map=$@.map 
ifeq ($(OS),Darwin)
	LD_MAP_FLAG = -Wl,-map -Wl,$@.map
endif

vpath %.c $(dir $(MAKEFILE_LIST))


############### UNIX V6 ################

V6FS_PROG = v6fs

V6_SRC = \
	ancient-src/v6/ken/alloc.c \
	ancient-src/v6/dmr/bio.c \
	ancient-src/v6/ken/subr.c \
	ancient-src/v6/ken/iget.c \
	ancient-src/v6/ken/rdwri.c \
	ancient-src/v6/ken/nami.c \
	ancient-src/v6/ken/fio.c \
	ancient-src/v6/ken/pipe.c \
	ancient-src/v6/ken/sys2.c \
	ancient-src/v6/ken/sys3.c \
	ancient-src/v6/ken/sys4.c
V6_OBJS = $(V6_SRC:.c=.o)
V6_DEPS = $(V6_SRC:.c=.d)

V6FS_SRC = \
	src/fusecommon.c \
	src/v6fuse.c \
	src/v6fs.c \
	src/v6adapt.c \
	src/idmap.c \
	src/dskio.c
V6FS_OBJS = $(V6FS_SRC:.c=.o)
V6FS_DEPS = $(V6FS_SRC:.c=.d)

$(V6_OBJS) src/v6fs.o src/v6adapt.o : CFLAGS += -I./ancient-src/v6
$(V6_DEPS) src/v6fs.d src/v6adapt.d : CPPFLAGS += -I./ancient-src/v6

-include $(V6_DEPS) $(V6FS_DEPS)
 
$(V6FS_PROG) : $(V6FS_OBJS) $(V6_OBJS)
	$(CC) -o $@ $(LD_MAP_FLAG) $^ $(LIBS)

ALL_PROGS += $(V6FS_PROG)
ALL_OUTPUTS += $(V6FS_PROG) $(V6FS_PROG).map $(V6FS_OBJS) $(V6_OBJS) $(V6FS_DEPS) $(V6_DEPS)


############### UNIX V7 ################

V7FS_PROG = v7fs

V7_SRC = \
	ancient-src/v7/sys/alloc.c \
	ancient-src/v7/dev/bio.c \
	ancient-src/v7/sys/subr.c \
	ancient-src/v7/sys/iget.c \
	ancient-src/v7/sys/rdwri.c \
	ancient-src/v7/sys/nami.c \
	ancient-src/v7/sys/fio.c \
	ancient-src/v7/sys/pipe.c \
	ancient-src/v7/sys/sys2.c \
	ancient-src/v7/sys/sys3.c \
	ancient-src/v7/sys/sys4.c \
	ancient-src/v7/sys/main.c
V7_OBJS = $(V7_SRC:.c=.o)
V7_DEPS = $(V7_SRC:.c=.d)

V7FS_SRC = \
	src/fusecommon.c \
	src/v7fuse.c \
	src/v7fs.c \
	src/v7adapt.c \
	src/idmap.c \
	src/dskio.c
V7FS_OBJS = $(V7FS_SRC:.c=.o)
V7FS_DEPS = $(V7FS_SRC:.c=.d)

$(V7_OBJS) src/v7fs.o src/v7adapt.o : CFLAGS += -I./ancient-src/v7
$(V7_DEPS) src/v7fs.d src/v7adapt.d : CPPFLAGS += -I./ancient-src/v7

-include $(V7_DEPS) $(V7FS_DEPS)

$(V7FS_PROG) : $(V7FS_OBJS) $(V7_OBJS)
	$(CC) -o $@ $(LD_MAP_FLAG) $^ $(LIBS)

ALL_PROGS += $(V7FS_PROG)
ALL_OUTPUTS += $(V7FS_PROG) $(V7FS_PROG).map $(V7FS_OBJS) $(V7_OBJS) $(V7FS_DEPS) $(V7_DEPS)


############### 2.9 BSD ################

BSD29FS_PROG = bsd29fs

BSD29_SRC = \
	ancient-src/bsd29/sys/alloc.c \
	ancient-src/bsd29/dev/bio.c \
	ancient-src/bsd29/sys/subr.c \
	ancient-src/bsd29/sys/iget.c \
	ancient-src/bsd29/sys/rdwri.c \
	ancient-src/bsd29/sys/nami.c \
	ancient-src/bsd29/sys/fio.c \
	ancient-src/bsd29/sys/pipe.c \
	ancient-src/bsd29/sys/sys2.c \
	ancient-src/bsd29/sys/sys3.c \
	ancient-src/bsd29/sys/sys4.c \
	ancient-src/bsd29/sys/main.c \

BSD29_OBJS = $(BSD29_SRC:.c=.o)
BSD29_DEPS = $(BSD29_SRC:.c=.d)

BSD29FS_SRC = \
	src/fusecommon.c \
	src/bsd29fuse.c \
	src/bsd29fs.c \
	src/bsd29adapt.c \
	src/idmap.c \
	src/dskio.c
BSD29FS_OBJS = $(BSD29FS_SRC:.c=.o)
BSD29FS_DEPS = $(BSD29FS_SRC:.c=.d)

$(BSD29_OBJS) src/bsd29fs.o src/bsd29adapt.o : CFLAGS += -Wno-comment -Wno-endif-labels -I./ancient-src
$(BSD29_DEPS) src/bsd29fs.d src/bsd29adapt.d : CPPFLAGS += -Wno-comment -Wno-endif-labels -I./ancient-src

-include $(BSD29_DEPS) $(BSD29FS_DEPS)

$(BSD29FS_PROG) : $(BSD29FS_OBJS) $(BSD29_OBJS)
	$(CC) -o $@ $(LD_MAP_FLAG) $^ $(LIBS)

ALL_PROGS += $(BSD29FS_PROG)
ALL_OUTPUTS += $(BSD29FS_PROG) $(BSD29FS_PROG).map $(BSD29FS_OBJS) $(BSD29_OBJS) $(BSD29FS_DEPS) $(BSD29_DEPS)


############### 2.11 BSD ################

BSD211FS_PROG = bsd211fs

BSD211_SRC = \
	ancient-src/bsd211/sys/kern_descrip.c \
	ancient-src/bsd211/sys/sys_generic.c \
	ancient-src/bsd211/sys/sys_inode.c \
	ancient-src/bsd211/sys/ufs_alloc.c \
	ancient-src/bsd211/sys/ufs_bio.c \
	ancient-src/bsd211/sys/ufs_bmap.c \
	ancient-src/bsd211/sys/ufs_fio.c \
	ancient-src/bsd211/sys/ufs_inode.c \
	ancient-src/bsd211/sys/ufs_namei.c \
	ancient-src/bsd211/sys/ufs_subr.c \
	ancient-src/bsd211/sys/ufs_syscalls.c \
	ancient-src/bsd211/sys/ufs_syscalls2.c \
	ancient-src/bsd211/sys/vfs_vnops.c

BSD211_OBJS = $(BSD211_SRC:.c=.o)
BSD211_DEPS = $(BSD211_SRC:.c=.d)

BSD211FS_SRC = \
	src/fusecommon.c \
	src/bsd211fuse.c \
	src/bsd211adapt.c \
	src/bsd211fs.c \
	src/idmap.c \
	src/dskio.c

BSD211FS_OBJS = $(BSD211FS_SRC:.c=.o)
BSD211FS_DEPS = $(BSD211FS_SRC:.c=.d)

$(BSD211_OBJS) src/bsd211fs.o src/bsd211adapt.o : CFLAGS += -Wno-comment -Wno-endif-labels -Wno-dangling-else -Wno-parentheses -Wno-char-subscripts -I./ancient-src
$(BSD211_DEPS) src/bsd211fs.d src/bsd211adapt.d : CPPFLAGS += -Wno-comment -Wno-endif-labels -Wno-dangling-else -Wno-parentheses -Wno-char-subscripts -I./ancient-src

-include $(BSD211_DEPS) $(BSD211FS_DEPS)

$(BSD211FS_PROG) : $(BSD211FS_OBJS) $(BSD211_OBJS)
	$(CC) -o $@ $(LD_MAP_FLAG) $^ $(LIBS)

ALL_PROGS += $(BSD211FS_PROG)
ALL_OUTPUTS += $(BSD211FS_PROG) $(BSD211FS_PROG).map $(BSD211FS_OBJS) $(BSD211_OBJS) $(BSD211FS_DEPS) $(BSD211_DEPS)


########### GENERAL TARGETS ############

all : $(ALL_PROGS)

test : $(addprefix test-, $(ALL_PROGS))

test-% : %
	./test/retro-fuse-test.py $*

clean :
	rm -f $(ALL_OUTPUTS)
