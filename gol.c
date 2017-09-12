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

	char* filename;
	FILE* file;

	if (argc > 1)
	{
		filename = argv[1];
		file = fopen(filename, "r");

		if (file == NULL)
		{
			printf("Error opening file\n");
			return -1;
		}

		gol_array_read_file(file, ga1);
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

				//get the number of neighbours
				int neighbours_num = num_of_neighbours(array1, N, M, i, j);
			
				if (array1[i][j] == 1)//if its alive
				{
					if (neighbours_num < 2 ||  neighbours_num > 3)//0,1 or 4 to 8 neighbours
					{//the organism dies
						array2[i][j] = 0;
						no_change = 0;
					}
					else//2 or 3 neigbours. So the organism survives (no change)
					{
						array2[i][j] = 1;
					}
				}
				else//if its dead
				{
					if (neighbours_num == 3)//3 neighbours
					{//a new organism is born
						array2[i][j] = 1;
						no_change = 0;
					}
					else
					{//still no organism (no change)
						array2[i][j] = 0;
					}
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

	fclose(file);
	//free arrays
	gol_array_free(&ga1);
	gol_array_free(&ga2);

	return 0;

}