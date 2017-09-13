#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <mpi.h>

#include "gol_array.h"
#include "functions.h"

#define PRINT_STEPS 1

int main(int argc, char* argv[])
{
	//the following can be also given by the user (to do)
	int N = 19;
	int M = 70;
	int max_loops = 200;

	gol_array* temp;//for swaps;
	gol_array* ga1;
	gol_array* ga2;
	int i, j;

	//allocate and init two NxM gol_arrays
	ga1 = gol_array_init(N, M);
	ga2 = gol_array_init(N, M);

	//MPI init
	char* filename;
	FILE* file;
	int rank, size;
	int tag = 1400201;
	MPI_Status status;

	MPI_Init (&argc, &argv);
  	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  	MPI_Comm_size (MPI_COMM_WORLD, &size);


	//for master process only
  	if (rank == 0) 
  	{
		if (argc > 1) {
			filename = argv[1];
			file = fopen(filename, "r");

			if (file == NULL) {
					printf("Error opening file\n");
				return -1;
			}

			gol_array_read_file(file, ga1);
		}
		else {//no input file given, generate a random game array 
			printf("No input file given as argument\n");
			printf("Generating a random game of life array to play\n");
			gol_array_generate(ga1);
		}


		printf("Printing initial array:\n\n");
		print_array(ga1->array, N, M);
		putchar('\n');	
		printf("Starting the Game of Life\n");

  	}

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

		for (i=0; i<N; i++) {
			for (j=0; j<M; j++) {
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

	fclose(file);
	//free arrays
	gol_array_free(&ga1);
	gol_array_free(&ga2);

	MPI_Finalize();

	return 0;

}