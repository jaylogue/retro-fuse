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
CC = gcc
CPPFLAGS = -MMD -MP -I./src
CFLAGS = -std=c11 -g -Wall -O0
LIBS = -lfuse

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
	$(CC) -o $@ -Xlinker $^ $(LIBS)

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
	$(CC) -o $@ -Xlinker $^ $(LIBS)

########### GENERAL TARGETS ############

all : $(V6FS_PROG) $(V7FS_PROG)

clean :
	rm -f $(V6FS_PROG) $(V6FS_PROG).map $(V6FS_OBJS) $(V6_OBJS) $(V6FS_DEPS) $(V6_DEPS)
	rm -f $(V7FS_PROG) $(V7FS_PROG).map $(V7FS_OBJS) $(V7_OBJS) $(V7FS_DEPS) $(V7_DEPS)
