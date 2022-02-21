#CS 360 Lab 1 Makefile
#James Calloway 8/1/99

CC = gcc 

INCLUDES = -I/home/jplank/cs360/include

CFLAGS = -g $(INCLUDES)

LIBDIR = /home/jplank/cs360/objs

LIBS = $(LIBDIR)/libfdr.a 

EXECUTABLES: l2p1 l2p2 l2p3

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

l2p1: l2p1.o
	$(CC) $(CFLAGS) -o l2p1 l2p1.o $(LIBS)
l2p2: l2p2.o
	$(CC) $(CFLAGS) -o l2p2 l2p2.o $(LIBS)
l2p3: l2p3.o
	$(CC) $(CFLAGS) -o l2p3 l2p3.o $(LIBS)

#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm -f core $(EXECUTABLES) *.o


