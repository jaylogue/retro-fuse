CC = gcc

PROG = v6fuse

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

CFLAGS = -std=c11 -ggdb -Wall -O0 $(foreach I,$(INCS),-I $(I))

INCS = ./src ./ancient-src/v6

LIBS = -lfuse

$(V6OBJS) : CFLAGS += -DANCIENT_SRC

all : $(PROG)

$(PROG) : $(OBJS)
	$(CC) -o $@ -Xlinker -Map=$@.map $^ $(LIBS)

clean :
	rm -f $(PROG) $(PROG).map $(OBJS)
