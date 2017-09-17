#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <mpi.h>

#include "./gol_lib/gol_array.h"
#include "./gol_lib/functions.h"

#define PRINT_INITIAL 0
#define PRINT_STEPS 0

int main(int argc, char* argv[])
{
	//the following can be also given by the user (to do)
	int N = 420;
	int M = 420;
	int max_loops = 200;

	gol_array* temp;//for swaps;
	gol_array* ga1;
	gol_array* ga2;
	int i, j;

	//MPI init
	char* filename;
	FILE* file;
	int rank, size;
	int tag = 1400201;
	MPI_Status status;

	MPI_Init (&argc, &argv);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  	MPI_Comm_size (MPI_COMM_WORLD, &size);

  	//allocate and init two NxM gol_arrays
	ga1 = gol_array_init(N, M);
	ga2 = gol_array_init(N, M);


	if (argc != 3 && argc != 4)
	{
		if (rank == 0)
		{
			printf("Running with default matrix size\n");
			printf("Usage 1: 'mpiexec -n <process_num> ./gol_mpi <filename> <N> <M>'");
			printf("Usage 2: 'mpiexec -n <process_num> ./gol_mpi <N> <M>'");
		}
	}
	else
	{
		N = atoi(argv[argc-2]);
		M = atoi(argv[argc-1]);
	}

	if (rank == 0)
		printf("N = %d\nM = %d\n", N, M);

	//for master process only
  	if (rank == 0) 
  	{
		if (argc == 2 || argc == 4) 
		{
			filename = argv[1];
			file = fopen(filename, "r");

			if (file == NULL) {
					printf("Error opening file\n");
				return -1;
			}

			gol_array_read_file(file, ga1);
		}
		else 
		{//no input file given, generate a random game array 
			printf("No input file given as argument\n");
			printf("Generating a random game of life array to play\n");
			gol_array_generate(ga1);
		}
		
		if (PRINT_INITIAL)
		{
			printf("Printing initial array:\n\n");
			print_array(ga1->array, N, M);
		}
		
		putchar('\n');	
		printf("Starting the Game of Life\n");

  	}

  	//Find this process's boundaries
	int size_sqrt = sqrt(size);
	int rows_per_block = N / size_sqrt;
	int cols_per_block = M / size_sqrt;
	int blocks_per_row = N / rows_per_block;
	int blocks_per_col = M / cols_per_block;

	//DEBUG PRINT FOR BOUNDARIES (only master prints it)
	if (rank == 0)
	{
		for (i=0; i<size; i++)
		{
			int row_start, row_end, col_start, col_end;
	  		
	  		row_start = (i / blocks_per_row) * rows_per_block;
	  		row_end = row_start + rows_per_block;
	  		col_start = (i % blocks_per_col) * cols_per_block;
	  		col_end = col_start + cols_per_block;
			
			printf("%d: rows [%d-%d) cols [%d-%d)\n", i, row_start, row_end, col_start, col_end);
		}
	}


	int row_start, row_end, col_start, col_end;
  	row_start = (rank / blocks_per_row) * rows_per_block;
  	row_end = row_start + rows_per_block;

  	col_start = (rank % blocks_per_col) * cols_per_block;
  	col_end = col_start + cols_per_block;

  	//Commented below prints because they are shown in random order
  	//And that is not very helpfull

  	// printf("--------------\n");
  	// printf("Process %d\n", rank);
  	// printf("ROWS: %d to %d\n", row_start, row_end);
  	// printf("COLS: %d to %d\n", col_start, col_end);
  	// printf("--------------\n\n");


  	int p_ul,p_u,p_ur,p_l,p_r,p_dl,p_d,p_dr;

	//Game of life LOOP
	int count = 0;
	int no_change;

	long int start = time(NULL);

	while (count < max_loops) 
	{
		count++;
		no_change = 1;
		short int** array1 = ga1->array;
		short int** array2 = ga2->array;

		//8 Isend
		//8 IRecv


		for (i=row_start + 1; i<row_end - 1; i++) {
			for (j= col_start + 1; j< col_end; j++) {
				//for each cell/organism

				//for each cell/organism
				//see if there is a change
				//populate functions applies the game's rules
				//and returns 0 if a change occurs
				if (populate(array1, array2, N, M, i, j) == 0)
				{
					no_change = 0;
				}	
			}
		}

		//only the master process prints
		if (PRINT_STEPS && rank == 0) {
			print_array(array2, N, M);
			putchar('\n');
		}

		if (no_change == 1) {
			printf("Terminating because there was no change at loop number %d\n", count);
			break;
		}

		//swap arrays (array2 becomes array1)
		temp = ga1;
		ga1 = ga2;
		ga2 = temp;

	}


	if (rank == 0) {
		if (no_change == 0)
		{
			printf("Max loop number (%d) was reached. Terminating Game of Life\n", max_loops);
		}

		printf("Time elapsed: %ld seconds\n", time(NULL) - start);
	}

	if (rank == 0 && file != NULL)
		fclose(file);

	//free arrays
	gol_array_free(&ga1);
	gol_array_free(&ga2);

	MPI_Finalize();

	return 0;

}