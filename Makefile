CC = gcc

PROG = aufuse

SRC = \
	src/aufuse.c \
	src/dskio.o

V6SRC = \
	src/v6adapt.c \
	ancient-src/v6/ken/alloc.c \
	ancient-src/v6/dmr/bio.c \
	ancient-src/v6/ken/subr.c \
	ancient-src/v6/ken/iget.c \
	ancient-src/v6/ken/rdwri.c \
	ancient-src/v6/ken/nami.c \
	ancient-src/v6/ken/fio.c

INCS = ./src 

CFLAGS = -std=c11 -ggdb -O2 -D_XOPEN_SOURCE=500 $(foreach I,$(INCS),-I $(I))

V6OBJS = $(V6SRC:.c=.o)
OBJS = $(SRC:.c=.o) $(V6OBJS)

$(V6OBJS) : CFLAGS += -DANCIENT_SRC
$(V6OBJS) : INCS += ./ancient-src/v6

all : $(PROG)

$(PROG) : $(OBJS)
	$(CC) -o $@ -Xlinker -Map=$@.map  $^

clean :
	rm -f $(PROG) $(OBJS)
