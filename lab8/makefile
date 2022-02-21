#CS 360 Lab A

CC = gcc 

INCLUDES = -I/home/jplank/cs360/include

CFLAGS = -g $(INCLUDES)

LIBDIR = /home/jplank/cs360/objs

LIBS = -L -lsocket -lnsl -lpthread $(LIBDIR)/libfdr.a $(LIBDIR)/sockettome.o

EXECUTABLES: chat_server

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

chat_server: chat_server.o
	$(CC) $(CFLAGS) -o chat_server chat_server.o $(LIBS)

#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm -f core $(EXECUTABLES) *.o


