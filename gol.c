#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "functions.h"


#define MAX_LOOPS 1000

int main()
{
	short int** temp;//for swaps;
	short int** array1;
	short int** array2;
	int i, j;

	//allocate two NxM arrays
	array1 = malloc(N * sizeof(short int*));
	assert(array1 != NULL);
	array2 = malloc(N * sizeof(short int*));
	assert(array2 != NULL);

	for (i=0; i<N; i++)
	{
		array1[i] = malloc(M * sizeof(short int));
		array2[i] = malloc(M * sizeof(short int));
	}


	//Initialise array
	for (i=0; i<N; i++)
	{
		for (j=0; j<M; j++)
		{
			array1[i][j] = 0;
		}
	}

	read_alive(array1);

	printf("Printing initial array:\n\n");
	print_array(array1);
	putchar('\n');

	//Game of life LOOP
	int count = 0;

	while (count < MAX_LOOPS)
	{
		count++;
		int no_change = 1;

		for (i=0; i<N; i++)
		{
			for (j=0; j<M; j++)
			{
				//for each cell/organism

				//get the number of neighbours
				int neighbours_num = num_of_neighbours(array1, i, j);
			
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

		print_array(array2);
		putchar('\n');

		if (no_change == 1)
		{
			printf("Terminating because there was no change at loop number %d\n", count);
			break;
		}

		//swap arrays (array2 becomes array1)
		temp = array1;
		array1 = array2;
		array2 = temp;

		//wait for input to continue
		// scanf("%d", &i);
	}



	//free arrays
	for (i=0; i<N; i++)
	{
		free(array1[i]);
		free(array2[i]);
	}

	free(array1);
	free(array2);

	return 0;

}