OBJS = gol.o gol_mpi.o gol_mpi_openmp.o
SOURCE = gol.c gol_mpi.c gol_mpi_openmp.c
HEADER =
CC = cc
MPICC = mpicc
OPENMP_MPICC = mpicc -fopenmp
CFLAGS= -c -Wall
LFLAGS= -Wall
OUT = gol gol_mpi gol_mpi_openmp gol_cuda

all: gol gol_mpi gol_mpi_openmp gol_cuda

gol: gol.o gol_lib_make
	$(CC) $(LFLAGS) gol.o ./gol_lib/gol_array.o ./gol_lib/functions.o -o gol

gol_mpi: gol_lib_make
	$(MPICC) $(LFLAGS) gol_mpi.c ./gol_lib/gol_array.o ./gol_lib/functions.o -o gol_mpi -lm

gol_mpi_openmp: gol_lib_make gol_mpi_openmp.c
	${OPENMP_MPICC} gol_mpi_openmp.c ./gol_lib/gol_array.o ./gol_lib/functions.o -o gol_mpi_openmp -lm

gol.o: gol.c
	$(CC) $(CFLAGS) gol.c

clean: gol_lib_clean
	rm -f $(OBJS) $(OUT)

gol_lib_make:
	cd gol_lib && make && cd ..

gol_lib_clean:
	cd gol_lib && make clean && cd ..

gol_cuda: gol_cuda.cu gol_lib_make
	nvcc gol_cuda.cu -o gol_cuda
count:
	wc $(SOURCE) $(HEADER)
