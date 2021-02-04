CC = gcc
CFLAGS = -c -fno-builtin -I ./src
VPATH = ancient-src/v6/dmr ancient-src/v6/ken

all : alloc.o bio.o subr.o iget.o
