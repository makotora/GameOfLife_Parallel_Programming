OBJS = gol.o
SOURCE = gol.c
HEADER =
CC = gcc
CFLAGS= -c -Wall
LFLAGS= -Wall

all: gol

gol: gol.o gol_lib_make
	$(CC) $(LFLAGS) gol.o ./gol_lib/gol_array.o ./gol_lib/functions.o -o gol

gol.o: gol.c
	$(CC) $(CFLAGS) gol.c

clean: gol_lib_clean
	rm -f $(OBJS) gol fgen

gol_lib_make:
	cd gol_lib && make && cd ..

gol_lib_clean:
	cd gol_lib && make clean && cd ..

count:
	wc $(SOURCE) $(HEADER)