#include <stdio.h>
#include <sys/time.h>
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
#define REDUCE_RATE 1
#define PRINT_INITIAL 0
#define PRINT_STEPS 0
#define PRINT_FINAL 0
#define DEFAULT_N 420
#define DEFAULT_M 420
#define MAX_LOOPS 500
#define CUDA_THREADS 4
#define CUDA_BLOCKS 64

void print_1d_array(short int * array, int N, int M);

__global__ void parallel_populate(short int* array1, short int* array2, int N, int M, int *no_change)
{

  uint worldSize = N * M;

  //Κάθε GPU core παίρνει ένα κέλι με την σειρά
  for (uint cellId = __mul24(blockIdx.x, blockDim.x) + threadIdx.x;
      cellId < worldSize;
      cellId += blockDim.x * gridDim.x) {

    uint x = cellId % N;
    uint yAbs = cellId - x;
    uint xLeft = (x + N - 1) % N;
    uint xRight = (x + 1) % N;
    uint yAbsUp = (yAbs + worldSize - N) % worldSize;
    uint yAbsDown = (yAbs + N) % worldSize;
 
    uint aliveCells = array1[xLeft + yAbsUp] + array1[x + yAbsUp]
      + array1[xRight + yAbsUp] + array1[xLeft + yAbs] + array1[xRight + yAbs]
      + array1[xLeft + yAbsDown] + array1[x + yAbsDown] + array1[xRight + yAbsDown];

    array2[x + yAbs] =
      aliveCells == 3 || (aliveCells == 2 && array1[x + yAbs]) ? 1 : 0;	
  	
  	if(array1[x + yAbs] != array2[x + yAbs])
  		*no_change = 0;
  }
}

double timedifference_msec(struct timeval t0, struct timeval t1)
{
    return ((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f)/1000.0f;
}


int main(int argc, char* argv[])
{
	//the following can be also given by the user (to do)
	int N,M;
	int max_loops = -1;

	int cudaBlocks = -1;
	int cudaThreads = -1;

	struct timeval start;
	struct timeval finish;

	gol_array* ga1;
	gol_array* ga2;
	int i;

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
  		else if ( !strcmp(argv[i], "-b") )
  		{
  			cudaBlocks = atoi(argv[i+1]);
  			i++;
  		}
		else if ( !strcmp(argv[i], "-t") )
  		{
  			cudaThreads = atoi(argv[i+1]);
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
	}

	if (cudaThreads == -1)
	{
		cudaThreads = CUDA_THREADS;
		
	}
	
	if (cudaBlocks == -1)
	{
		cudaBlocks = CUDA_BLOCKS;	
	}

	if ( INFO )
	{
		printf("N = %d, M = %d\n", N, M);
		printf("Running with max loops:  %d\n", max_loops);
		printf("Running with blocks:     %d\n", cudaBlocks);
		printf("Running with threads:    %d\n", cudaThreads);
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
	
/*	if (PRINT_INITIAL)
	{
		printf("Printing initial array:\n\n");
		print_array(ga1->array, N, M);
		putchar('\n');	
	}
*/	
	short int* arr1;
	short int* arr2;
	int *cudaNoChange;

	cudaMalloc((void **) &cudaNoChange, sizeof(int *));
	cudaMalloc((void **) &arr1, N*M*sizeof(short int *));
	cudaMalloc((void **) &arr2, N*M*sizeof(short int *));
	
	short int* oneDarray1 = (short int *) malloc(N*M*sizeof(short int));
	short int* oneDarray2 = (short int *) malloc(N*M*sizeof(short int));
		

	cudaMemcpy(arr1, ga1->flat_array, sizeof(short int) * M * N, cudaMemcpyHostToDevice);

	int count;

	int *no_change = (int *) malloc(1*sizeof(int));

	if (STATUS)
		printf("Starting the Game of Life\n");

	gettimeofday(&start, 0);

	cudaMemcpy(oneDarray1, arr1, sizeof(short int) * M * N, cudaMemcpyDeviceToHost);
/*	print_1d_array(oneDarray1, N,M);
	printf("\n\n\n\n");*/


for(count = 0; count < max_loops; count++) 
	{
		*no_change = 1;
		cudaMemcpy(cudaNoChange, no_change, sizeof(int), cudaMemcpyHostToDevice);

		cudaMemcpy(arr1, oneDarray1, sizeof(short int) * M * N, cudaMemcpyHostToDevice);
		cudaMemcpy(arr2, oneDarray2, sizeof(short int) * M * N, cudaMemcpyHostToDevice);

		parallel_populate<<<cudaBlocks,cudaThreads>>>(arr1,  arr2, N,  M, cudaNoChange);

		cudaMemcpy(oneDarray1, arr1, sizeof(short int) * M * N, cudaMemcpyDeviceToHost);
		cudaMemcpy(oneDarray2, arr2, sizeof(short int) * M * N, cudaMemcpyDeviceToHost);
		
		cudaMemcpy(no_change, cudaNoChange, sizeof(int), cudaMemcpyDeviceToHost);

		if (PRINT_STEPS) {
			print_1d_array(oneDarray1	, N, M);
			putchar('\n');
		}

		if(count % REDUCE_RATE == 0)
		{
			if(*no_change == 1)
			{
				printf("Terminating because there was no change at loop number %d\n", count);
				break;
			}
		}

		//swap arrays (array2 becomes array1)
		short int* temp;
		temp = oneDarray1;
		oneDarray1 = oneDarray2;
		oneDarray2 = temp;
	}

	if (*no_change == 0 && STATUS)
	{
		printf("Max loop number (%d) was reached. Terminating Game of Life\n", max_loops);
	}


	gettimeofday(&finish, 0);
	
	double local_elapsed = timedifference_msec(start, finish);
	if (INFO)
		printf("Time elapsed: %.3f seconds\n", local_elapsed);

	if (PRINT_FINAL)
	{
		print_1d_array(oneDarray2, N,M);
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

void print_1d_array(short int * array, int N, int M)
{
	int i, j;

	for (i=0; i<N; i++)
	{
		putchar('|');

		for (j=0; j<M; j++)
		{
			if (array[i*M+j] == 1)
				putchar('o');
			else
				putchar(' ');

			putchar('|');
		}

		putchar('\n');
	}	
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
