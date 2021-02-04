CC = gcc
VPATH = ./src ./ancient-src/v6/dmr ./ancient-src/v6/ken

SRC = v6-adapt.c
V6SRC = alloc.c bio.c subr.c iget.c rdwri.c nami.c fio.c

INCS = ./src ./ancient-src/v6

CFLAGS = -fno-builtin $(foreach I,$(INCS),-I $(I))

V6OBJS = $(V6SRC:.c=.o)
OBJS = $(SRC:.c=.o) $(V6OBJS)

all : $(OBJS)

clean :
	rm -f $(OBJS)
