#include <stdio.h>
#include <stdlib.h>
#include "functions.h"

void read_alive(short int** array)
{
	int row, col;
	printf("Give row and column of 'alive' cells\n");
	printf("Row [1, %d]\n", N);
	printf("Col [1, %d]\n", M);
	printf("To stop, just give a non-positive row or column\n");

	while (1)
	{
		printf("\nRow: ");
		scanf("%d", &row);
		if (row <= 0)
			break;

		printf("Column: ");
		scanf("%d", &col);
		if (col <= 0)
			break;

		if (row > N || col > M)
		{
			printf("Invalid row or column! Try again\n");
			printf("Row [1, %d]\n", N);
			printf("Col [1, %d]\n", M);
		}
		else
		{
			array[row-1][col-1] = 1;
		}
	}
}


void print_array(short int** array)
{
	int i, j;

	for (i=0; i<N; i++)
	{
		putchar('|');

		for (j=0; j<M; j++)
		{
			if (array[i][j] == 1)
				putchar('o');
			else
				putchar(' ');

			putchar('|');
		}

		putchar('\n');

		// putchar(' ');

		// for (j=0; j<M; j++)
		// {
		// 	putchar('-');
		// 	putchar('-');
		// }

		// putchar(' ');
		// putchar('\n');
	}
}


int num_of_neighbours(short int** array, int row, int col)
{
	//neighbours
	int up_row = (row-1+N) % N;
	int down_row = (row+1) % N;
	int right_col = (col+1) % M;
	int left_col = (col-1+M) % M;

	int n1,n2,n3,n4,n5,n6,n7,n8;

	//up
	n1 = array[up_row][left_col];
	n2 = array[up_row][col];
	n3 = array[up_row][right_col];

	//down
	n4 = array[down_row][left_col];
	n5 = array[down_row][col];
	n6 = array[down_row][right_col];

	//left-right
	n7 = array[row][left_col];
	n8 = array[row][right_col];

	return n1 + n2 + n3 + n4 + n5 + n6 + n7 + n8;
}


void print_neighbour_nums(short int** array)
{
	int i,j;

	for (i=0; i<N; i++)
	{
		for (j=0; j<M; j++)
		{
			printf(" %d", num_of_neighbours(array, i, j));
		}
		putchar('\n');
	}
}