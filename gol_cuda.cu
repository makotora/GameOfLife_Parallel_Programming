#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "./gol_lib/gol_array.h"
#include "./gol_lib/functions.h"

#define DEBUG 1
#define INFO 1
#define STATUS 1
#define TIME 1
#define PRINT_INITIAL 1
#define PRINT_STEPS 1
#define PRINT_FINAL 1
#define DEFAULT_N 420
#define DEFAULT_M 420
#define MAX_LOOPS 200
#define THREADS 1024

__global__ void parallel_populate(short int** array1, short int** array2, int N, int M, int rpb, int cpb, int bpr, int bpc, int *no_change)
{
	int i,j;
	int row_start, row_end, col_start, col_end;
  	
  	int index = threadIdx.x + blockIdx.x* blockDim.x;
  	//we use the process's coordinates instead of its rank
  	//because mpi might have reordered the processes for better performance (virtual topology)
  	row_start =  (index/bpr) * rpb ;
  	row_end = row_start + rpb - 1;
  	col_start = (index%bpc)* cpb;
  	col_end = col_start + cpb - 1;


  	for (i=row_start + 1; i<= row_end - 1; i++) {
		for (j= col_start + 1; j <= col_end - 1; j++) {
				//for each cell/organism

				//for each cell/organism
				//see if there is a change
				//populate functions applies the game's rules
				//and returns 0 if a change occurs
				//get the number of neighbours
			int neighbours_num;

			int up_row = (i-1+N) % N;
			int down_row = (i+1) % N;
			int right_col = (j+1) % M;
			int left_col = (j-1+M) % M;

			//neighbours
			int n1,n2,n3,n4,n5,n6,n7,n8;

			//up
			n1 = array1[up_row][left_col];
			n2 = array1[up_row][j];
			n3 = array1[up_row][right_col];

			//down
			n4 = array1[down_row][left_col];
			n5 = array1[down_row][j];
			n6 = array1[down_row][right_col];

			//left-right
			n7 = array1[i][left_col];
			n8 = array1[i][right_col];

			neighbours_num = n1 + n2 + n3 + n4 + n5 + n6 + n7 + n8;


			if (array1[i][j] == 1)//if its alive
			{
				if (neighbours_num < 2 || neighbours_num > 3)//0,1 or 4 to 8 neighbours
				{//the organism dies
					array2[i][j] = 0;
					*no_change = 0;
				}
				else//2 or 3 neigbours. So the organism survives (no change)
				{
					array2[i][j] = 1;
					*no_change = 1;
				}
			}
			else//if its dead
			{
				if (neighbours_num == 3)//3 neighbours
				{//a new organism is born
					array2[i][j] = 1;
					*no_change = 0;
				}
				else
				{//still no organism (no change)
					array2[i][j] = 0;
					*no_change = 1;
				}
			}	
		}
	}
}


int main(int argc, char* argv[])
{
	//the following can be also given by the user (to do)
	int N,M;
	int max_loops = -1;
	int threads = -1;

	gol_array* ga1;
	gol_array* ga2;
	int i, j;

	/* SECTION A
		Blocks computation
		Matrix allocation, initialization
	*/

	char* filename = NULL;

	N = -1;
	M = -1;

  	//Read matrix size and game of life grid
  	//Or use default values and randomly generate a game if no arguments are given
	i = 0;
  	while (++i < argc)
  	{
  		if ( !strcmp(argv[i], "-f") )
  		{
  			filename = argv[i+1];
  			i++;
  		}
  		else if ( !strcmp(argv[i], "-l") )
  		{
  			N = atoi(argv[i+1]);
  			i++;
  		}
  		else if ( !strcmp(argv[i], "-c") )
  		{
  			M = atoi(argv[i+1]);
  			i++;
  		}
  		else if ( !strcmp(argv[i], "-m") )
  		{
  			max_loops = atoi(argv[i+1]);
  			i++;
  		}
		else if ( !strcmp(argv[i], "-t") )
  		{
  			threads = atoi(argv[i+1]);
  			i++;
  		}
  	}

	if (N == -1 || M == -1)
	{
		N = DEFAULT_N;
		M = DEFAULT_M;

		if (INFO)
		{
			printf("Running with default matrix size %dx%d\n", N, M);
			printf("Usage : './gol_cuda -f <filename> -l <N> -c <M> -n <max_loops>\n");
		}
	}
	else
	{
		if (N == 0 || M == 0)
		{
			printf("Invalid arguments given!");	
			printf("Usage : './gol_cuda -f <filename> -l <N> -c <M> -n <max_loops>\n");
			printf("Aborting...\n");
		}
	}

	if (max_loops == -1)
	{
		max_loops = MAX_LOOPS;
		
		if ( INFO )
			printf("Running with default max loops %d\n", max_loops);
	}

	if (threads == -1)
	{
		threads = THREADS;
		
		if ( INFO )
			printf("Running with default threads num %d\n", threads);
	}



	if (INFO)
		printf("N = %d\nM = %d\n", N, M);

	//Calculate block properties
	int line_div, col_div;
	double threads_sqrt = sqrt(threads);
	double div;
	int last_div_ok;

	if (threads_sqrt == floor(threads))
	{
		line_div = threads_sqrt;
		col_div = threads_sqrt;
	}
	else
	{
		last_div_ok = 1;
		i = 2;
		while ( i < (threads / 2) )
		{
			div = (float) threads / (float) i;

			if (floor(div) == div)
			{
				if ( ( abs((int) div - (int) (threads / div)) >= abs((int) last_div_ok - (int) (threads / last_div_ok)) )
					|| div == last_div_ok ) {
					break;
				}

				last_div_ok = (int) div;
			}

			i++;
		}

		if (last_div_ok == 1 && threads != 2 && threads != 1)
		{
			printf("Warning, threads num is a prime number and can't be devided well\n");
		}

		line_div = last_div_ok;
		col_div = threads / line_div;
	}

	int rows_per_block = N / line_div;
	int cols_per_block = M / col_div;
	int blocks_per_row = N / rows_per_block;
	int blocks_per_col = M / cols_per_block;

	if (INFO)
	{
		printf("line div : %d\n", line_div);
		printf("col div : %d\n", col_div);
		printf("rows_per_block: %d\n", rows_per_block);
		printf("cols_per_block: %d\n", cols_per_block);
	}

	//allocate and init two NxM gol_arrays
	ga1 = gol_array_init(N, M);
	ga2 = gol_array_init(N, M);


	if (filename != NULL) 
	{
		gol_array_read_file(filename, ga1);
	}
	else 
	{//no input file given, generate a random game array
		if (INFO)
		{
			printf("No input file given as argument\n");
			printf("Generating a random game of life array to play\n");
		}
		gol_array_generate(ga1);
	}
	
	if (PRINT_INITIAL)
	{
		printf("Printing initial array:\n\n");
		print_array(ga1->array, N, M);
		putchar('\n');	
	}




  	//Calculate this threads's boundaries
	int row_start, row_end, col_start, col_end;
  	
  	
/*
  	row_start = 
  	row_end = 
  	col_start = 
  	col_end = 
*/


	short int** array1 = ga1->array;
	short int** array2 = ga2->array;
	
	short int* arr1;
	short int* arr2;

	cudaMalloc((void **) &arr1, N*M*sizeof(short int *));
	cudaMalloc((void **) &arr2, N*M*sizeof(short int *));
	short int** arr33 = (short int **) malloc(N*sizeof(short int*));
	short int* arrAll = (short int *) malloc(N*M*sizeof(short int));

	for (int i = 0; i < N; ++i)
	{
		memcpy( arrAll + i*M*sizeof(short int) , array1[i], M*sizeof(short int));
	}

	cudaMemcpy(arr1, arrAll, sizeof(short int) * M * N, cudaMemcpyHostToDevice);

	for (int i = 0; i < N; ++i)
	{
		printf("i=%d\n", i);
		arr33[i] = (short int *) malloc(M*sizeof(short int));
		cudaMemcpy(arr33 + i*M*sizeof(short int) , arr1 + i*M*sizeof(short int), sizeof(short int) * M, cudaMemcpyDeviceToHost);
	}
	printf("hahahaha\n");
	print_array(arr33, N, M);
	return;

	int count;
	int no_change;
	int no_change_sum;

	double start, finish;

	if (STATUS)
		printf("Starting the Game of Life\n");

	start = time(NULL);

	for(count = 0; count < max_loops; count++) 
	{
		no_change = 1;


		if (PRINT_STEPS) {
			print_array(array2, N, M);
			putchar('\n');
		}

		//swap arrays (array2 becomes array1)
		short int** temp;
		temp = array1;
		array1 = array2;
		array2 = temp;

	}

	if (no_change == 0 && STATUS)
	{
		printf("Max loop number (%d) was reached. Terminating Game of Life\n", max_loops);
	}


	finish = time(NULL);

	double local_elapsed = finish - start;

	if (INFO)
		printf("Time elapsed: %f seconds\n", local_elapsed);

	if (PRINT_FINAL)
	{
		print_array(array1, N, M);
	}

	//free arrays
	gol_array_free(&ga1);
	gol_array_free(&ga2);

	return 0;

}

gol_array* gol_array_init(int lines, int columns)
{
	//allocate one big flat array, so as to make sure that the memory is continuous
	//in our 2 dimensional array
	short int* flat_array = (short int*) calloc(lines*columns, sizeof(short int));
	assert(flat_array != NULL);

	short int** array = (short int**) malloc(lines*sizeof(short int*));
	assert(array != NULL);
	int i;

	//make a 2 dimension array by pointing to our flat 1 dimensional array
	
	for (i=0; i<lines; i++)
	{
		array[i] = &(flat_array[columns*i]);
	}

	//allocate gol_array struct
	gol_array* new_gol_array = (gol_array*) malloc(sizeof(gol_array));
	new_gol_array->flat_array = flat_array;
	new_gol_array->array = array;
	new_gol_array->lines = lines;
	new_gol_array->columns = columns;

	return new_gol_array;
}



void gol_array_free(gol_array** gol_ar)
{
	gol_array* gol_ar_ptr = *gol_ar;

	free(gol_ar_ptr->flat_array);
	free(gol_ar_ptr->array);
	free(*gol_ar);
	*gol_ar = NULL;
}



void gol_array_read_input(gol_array* gol_ar)
{
	short int** array = gol_ar->array;
	int N = gol_ar->lines;
	int M = gol_ar->columns;

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



void gol_array_read_file(char* filename, gol_array* gol_ar)
{
	short int** array = gol_ar->array;
	int N = gol_ar->lines;
	int M = gol_ar->columns;

	char line[100];
	char copy[100];
	int counter = 0;
	int successful = 0;

	FILE* file = fopen(filename, "r");

	if (file == NULL) 
	{
		printf("Error opening file\n");
		return;
	}

	while (1)
	{
		counter++;

		if (fgets(line, 100, file) == NULL)
			break;

		/*ignore blank lines or lines that start with '#'*/
		if (strlen(line) == 0 || line[0] == '#')
			continue;

		//the line should contain just 2 numbers
		//which are the coordinates (row column) of an alive organism/cell
		char* token;
		int row,col;

		strcpy(copy, line);//keep a copy of the actual line
		//strtok messes the string up.. 
		
		//get row (first token of line)
		token = strtok(line, " ");

		if (token == NULL)
		{
			printf("Skipping invalid line (%d): '%s'\n",counter, copy);
			continue;
		}

		row = atoi(token);

		//get column
		token = strtok(NULL, " ");

		if (token == NULL)
		{
			printf("Skipping invalid line (%d): '%s'\n",counter, copy);
			continue;
		}

		col = atoi(token);

		//ignore invalid lines (row or column out of bounds)
		if (row < 1 || row > N || col < 1 || col > M)
		{
			printf("Invalid row or column\n");
			printf("Skipping invalid line (%d): '%s'\n",counter, copy);
			continue;
		}

		successful++;
		array[row-1][col-1] = 1;
	}

	printf("\nSuccesfully read %d coordinates\n", successful);
	fclose(file);
}


void gol_array_generate(gol_array* gol_ar)
{
	char datestr[9];
	char timestr[7];

	get_date_time_str(datestr, timestr);
	char filename[36];
	sprintf(filename, "generated_tests/rga_%s_%s", datestr, timestr);
	
	FILE* file = fopen(filename, "w");
	if (file == NULL)
	{
		printf("gol_array_generate error opening file!\n");
		exit(-1);
	}

	short int** array = gol_ar->array;
	int lines = gol_ar->lines;
	int columns = gol_ar->columns;

	srand(time(NULL));
	int alive_count = rand() % (lines*columns + 1);
	int i;

	for (i=0; i<alive_count; i++)
	{
		int x,y;

		x = rand() % lines;
		y = rand() % columns;

		fprintf(file, "%d %d\n", x+1, y+1);
		array[x][y] = 1;
	}

	fclose(file);
}

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