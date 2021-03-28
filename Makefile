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

PROG = v6fs

SRC = \
	src/v6fuse.c \
	src/v6fs.c \
	src/v6adapt.c \
	src/dskio.c

V6SRC = \
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

V6OBJS = $(V6SRC:.c=.o)
OBJS = $(SRC:.c=.o) $(V6OBJS)

INCS = ./src ./ancient-src/v6

LIBS = -lfuse

CC = gcc
CFLAGS = -std=c11 -g -Wall -O0 $(foreach I,$(INCS),-I $(I))

$(V6OBJS) : CFLAGS += -DANCIENT_SRC

all : $(PROG)

$(PROG) : $(OBJS)
	$(CC) -o $@ -Xlinker $^ $(LIBS)

clean :
	rm -f $(PROG) $(PROG).map $(OBJS)
