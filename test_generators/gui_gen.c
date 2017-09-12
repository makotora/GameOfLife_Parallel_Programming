#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
	char filename[50];
	int rows, columns;
	short int** array;

	//get input
	printf("Insert name of file to be created: ");
	scanf("%s", filename);

	printf("Insert number of rows: ");
	scanf("%d", &rows);

	printf("Insert number of columns: ");
	scanf("%d", &columns);


	printf("You can select any cell of the array and make it alive or dead\n");

	//open file
	FILE* file = fopen(filename, "w");
	assert(file != NULL);

	//allocate and initialise array
	int i,j;
	array = malloc(rows*sizeof(short int*));

	for (i=0; i<rows; i++)
	{
		array[i] = malloc(columns*sizeof(short int));

		for (j=0; j<columns; j++)
		{
			array[i][j] = 0;
		}
	}
	

	//ask for alive cells/organisms by showing which are already alive
	int row,column;

	while (1)
	{
		//print the array
		for (i=0; i<rows; i++)
		{
			putchar('|');

			for (j=0; j<columns; j++)
			{
				if (array[i][j] == 1)
					putchar('o');
				else
					putchar(' ');

				putchar('|');
			}

			putchar('\n');
		}

		printf("\nGive a non-positive number to stop the program\n");
		printf("Give row of cell: ");
		scanf("%d", &row);
		printf("Give column of cell: ");
		scanf("%d", &column);

		if (row < 1 || column < 1)
			break;

		if (row > rows || column > columns)
		{
			printf("Given row/column is out of range. Try again\n Array has %d rows and %d columns\n", rows, columns);
		}
		else
		{
			if (array[row-1][column-1] == 1)
			{
				array[row-1][column-1] = 0;	
			}
			else
			{
				array[row-1][column-1] = 1;
			}
		}

	}

	for (i=0; i<rows; i++)
	{
		for (j=0; j<columns; j++)
		{
			if (array[i][j] == 1)
			{
				fprintf(file, "%d %d\n", i+1, j+1);
			}
		}
	}

	fclose(file);

	return 0;
}