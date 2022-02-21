#CS 360 Lab 3 Fakemake

CC = gcc 

INCLUDES = -I/home/jplank/cs360/include

CFLAGS = -g $(INCLUDES)

LIBDIR = /home/jplank/cs360/objs

LIBS = $(LIBDIR)/libfdr.a 

EXECUTABLES: fakemake

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

fakemake: fakemake.o
	$(CC) $(CFLAGS) -o fakemake fakemake.o $(LIBS)

#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm -f core $(EXECUTABLES) *.o


