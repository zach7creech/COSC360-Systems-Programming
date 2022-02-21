#CS 360 Lab 4ab: tarc and tarx

CC = gcc 

INCLUDES = -I/home/jplank/cs360/include

CFLAGS = -g $(INCLUDES)

LIBDIR = /home/jplank/cs360/objs

LIBS = $(LIBDIR)/libfdr.a 

EXECUTABLES: tarc tarx

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

tarc: tarc.o
	$(CC) $(CFLAGS) -o tarc tarc.o $(LIBS)
tarx: tarx.o
	$(CC) $(CFLAGS) -o tarx tarx.o $(LIBS)
#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm -f core $(EXECUTABLES) *.o


