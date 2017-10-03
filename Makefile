OBJS = gol.o gol_mpi.o
SOURCE = gol.c gol_mpi.c
HEADER =
CC = gcc
MPICC = mpicc
CFLAGS= -c -Wall
LFLAGS= -Wall
OUT = gol gol_mpi

all: gol gol_mpi

gol: gol.o gol_lib_make
	$(CC) $(LFLAGS) gol.o ./gol_lib/gol_array.o ./gol_lib/functions.o -o gol

gol_mpi: gol_lib_make
	$(MPICC) $(LFLAGS) gol_mpi.c ./gol_lib/gol_array.o ./gol_lib/functions.o -o gol_mpi -lm

gol.o: gol.c
	$(CC) $(CFLAGS) gol.c

clean: gol_lib_clean
	rm -f $(OBJS) $(OUT)

gol_lib_make:
	cd gol_lib && make && cd ..

gol_lib_clean:
	cd gol_lib && make clean && cd ..

count:
	wc $(SOURCE) $(HEADER)
