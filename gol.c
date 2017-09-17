#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "./gol_lib/gol_array.h"
#include "./gol_lib/functions.h"

#define WAIT_FOR_ENTER 0
#define PRINT_STEPS 0

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

	char* filename = NULL;

	if (argc > 1)
	{
		filename = argv[1];
		gol_array_read_file(filename, ga1);
	}
	else//no input file given, generate a random game array
	{
		printf("No input file given as argument\n");
		printf("Generating a random game of life array to play\n");
		gol_array_generate(ga1);
	}


	printf("Printing initial array:\n\n");
	print_array(ga1->array, N, M);
	putchar('\n');

	if (WAIT_FOR_ENTER)
		getchar();//get \n chars (I getchar() in the loop to get 'Enter' in orded to continue with the next loop)

	//Game of life LOOP
	int count = 0;
	int no_change;

	printf("Starting the Game of Life\n");
	long int start = time(NULL);

	while (count < max_loops)
	{
		count++;
		no_change = 1;
		short int** array1 = ga1->array;
		short int** array2 = ga2->array;

		for (i=0; i<N; i++)
		{
			for (j=0; j<M; j++)
			{
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

		if (PRINT_STEPS)
		{
			print_array(array2, N, M);
			putchar('\n');
		}

		if (no_change == 1)
		{
			printf("Terminating because there was no change at loop number %d\n", count);
			break;
		}

		//swap arrays (array2 becomes array1)
		temp = ga1;
		ga1 = ga2;
		ga2 = temp;

		//wait for input to continue
		if (WAIT_FOR_ENTER)
			getchar();
	}


	if (no_change == 0)
	{
		printf("Max loop number (%d) was reached. Terminating Game of Life\n", max_loops);
	}

	printf("Time elapsed: %ld seconds\n", time(NULL) - start);

	//free arrays
	gol_array_free(&ga1);
	gol_array_free(&ga2);

	return 0;

}