#include "functions.h"


void print_array(short int** array, int N, int M)
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
	}
}


int populate(short int** array1, short int** array2, int N, int M, int i, int j)
{
	int no_change;
	//get the number of neighbours
	int neighbours_num = num_of_neighbours(array1, N, M, i, j);

	if (array1[i][j] == 1)//if its alive
	{
		if (neighbours_num < 2 || neighbours_num > 3)//0,1 or 4 to 8 neighbours
		{//the organism dies
			array2[i][j] = 0;
			no_change = 0;
		}
		else//2 or 3 neigbours. So the organism survives (no change)
		{
			array2[i][j] = 1;
			no_change = 1;
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
			no_change = 1;
		}
	}	

	return no_change;
}


int num_of_neighbours(short int** array, int N, int M, int row, int col)
{
	int up_row = (row-1+N) % N;
	int down_row = (row+1) % N;
	int right_col = (col+1) % M;
	int left_col = (col-1+M) % M;

	//neighbours
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


void print_neighbour_nums(short int** array, int N, int M)
{
	int i,j;

	for (i=0; i<N; i++)
	{
		for (j=0; j<M; j++)
		{
			printf(" %d", num_of_neighbours(array, N, M, i, j));
		}
		putchar('\n');
	}
}


void get_date_time_str(char* datestr, char* timestr)
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char day[3];
	char month[3];
	char hour[3];
	char minute[3];
	char second[3];

	if (tm.tm_mon + 1 > 9)
		sprintf(month, "%d",tm.tm_mon + 1);
	else
		sprintf(month, "0%d",tm.tm_mon + 1);

	if (tm.tm_mday > 9)
		sprintf(day, "%d",tm.tm_mday);
	else
		sprintf(day, "0%d",tm.tm_mday);

	if (tm.tm_hour > 9)
		sprintf(hour, "%d",tm.tm_hour);
	else
		sprintf(hour, "0%d",tm.tm_hour);

	if (tm.tm_min > 9)
		sprintf(minute, "%d",tm.tm_min);
	else
		sprintf(minute, "0%d",tm.tm_min);

	if (tm.tm_sec > 9)
		sprintf(second, "%d",tm.tm_sec);
	else
		sprintf(second, "0%d",tm.tm_sec);

	sprintf(datestr, "%d%s%s", tm.tm_year + 1900, month, day);
	sprintf(timestr, "%s%s%s", hour, minute, second);
}