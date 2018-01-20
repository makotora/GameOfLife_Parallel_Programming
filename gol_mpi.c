#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>

#include <mpi.h>

#include "./gol_lib/gol_array.h"
#include "./gol_lib/functions.h"

#define DEBUG 0
#define INFO 0
#define STATUS 1
#define TIME 1
#define PRINT_INITIAL 0
#define PRINT_STEPS 0
#define PRINT_FINAL 0
#define DEFAULT_N 420
#define DEFAULT_M 420
#define MAX_LOOPS 200
#define REDUCE_RATE 1

void gol_array_read_file_and_scatter(char* filename, gol_array* gol_ar, int processors, int lines, int columns,
										int rows_per_block, int cols_per_block, int blocks_per_row, MPI_Comm virtual_comm);
void gol_array_generate_and_scatter(gol_array* gol_ar, int processors, int lines, int columns,
										int rows_per_block, int cols_per_block, int blocks_per_row, MPI_Comm virtual_comm);
void gol_array_gather(short int** array, short int** myarray, int my_rank, int processes, int row_start, int col_start, 
	int blocks_per_row, int blocks_per_col, int rows_per_block, int cols_per_block, MPI_Datatype block_array, MPI_Comm virtual_comm);

int main(int argc, char* argv[])
{
	//the following can be also given by the user (to do)
	int N,M;
	int max_loops = -1;
	int reduce_rate = -1;

	gol_array* ga1;
	gol_array* ga2;
	int i, j;

	/* SECTION A
		MPI Initialize
		Blocks computation
		Column derived data type
	*/

	//MPI init
	int my_rank, processors;
	int tag = 201049;
	MPI_Status status;
	int coordinates[2];
	char* filename = NULL;

	MPI_Init (&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &processors);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

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
		else if ( !strcmp(argv[i], "-r") )
		{
			reduce_rate = atoi(argv[i+1]);
			i++;
		}
	}

	if (N == -1 || M == -1)
	{
		N = DEFAULT_N;
		M = DEFAULT_M;

		if (INFO && my_rank == 0)
		{
			printf("Running with default matrix size %dx%d\n", N, M);
			printf("Usage : 'mpiexec -n <process_num> ./gol_mpi -f <filename> -l <N> -c <M> -m <max_loops> -r <reduce_rate>\n");
		}
	}
	else
	{
		if (my_rank == 0)
		{
			if (N == 0 || M == 0)
			{
				printf("Invalid arguments given!");	
				printf("Usage : 'mpiexec -n <process_num> ./gol_mpi -f <filename> -l <N> -c <M> -m <max_loops> -r <reduce_rate>\n");
				printf("Aborting...\n");
				MPI_Abort(MPI_COMM_WORLD, -1);
			}
		}
	}

	if (max_loops == -1)
	{
		max_loops = MAX_LOOPS;
		
		if ( INFO && my_rank == 0 )
			printf("Running with default max loops %d\n", max_loops);
	}

	if (reduce_rate == -1)
	{
		reduce_rate = REDUCE_RATE;
		
		if ( INFO && my_rank == 0 )
			printf("Running with default reduce rate %d\n", reduce_rate);
	}


	if (my_rank == 0 && INFO)
		printf("N = %d\nM = %d\n", N, M);

	//Calculate block properties
	int line_div, col_div;
	double processors_sqrt = sqrt(processors);
	double div;
	int last_div_ok;

	if (processors_sqrt == floor(processors_sqrt))
	{
		line_div = processors_sqrt;
		col_div = processors_sqrt;
	}
	else
	{
		last_div_ok = 1;
		i = 2;
		while ( i < (processors / 2) )
		{
			div = (float) processors / (float) i;

			if (floor(div) == div)
			{
				if ( ( abs((int) div - (int) (processors / div)) >= abs((int) last_div_ok - (int) (processors / last_div_ok)) )
					|| div == last_div_ok ) {
					break;
				}

				last_div_ok = (int) div;
			}

			i++;
		}

		if (my_rank == 0 && last_div_ok == 1 && processors != 2 && processors != 1)
		{
			printf("Warning, processors num is a prime number and the matrix can't be devided well\n");
		}

		line_div = last_div_ok;
		col_div = processors / line_div;
	}

	int rows_per_block = N / line_div;
	int cols_per_block = M / col_div;
	int blocks_per_row = N / rows_per_block;
	int blocks_per_col = M / cols_per_block;

	if (INFO && my_rank == 0)
	{
		printf("line div : %d\n", line_div);
		printf("col div : %d\n", col_div);
		printf("rows_per_block: %d\n", rows_per_block);
		printf("cols_per_block: %d\n", cols_per_block);
	}

	//Define our column derived data type
	MPI_Datatype derived_type_block_col;

	MPI_Type_vector(rows_per_block,    
	   1,                  
	   cols_per_block + 2,         
	   MPI_SHORT,       
	   &derived_type_block_col);       

	MPI_Type_commit(&derived_type_block_col);

	//Define block_array data type
	//In case we want to gather all arrays to master
	MPI_Datatype derived_type_block_array;

	MPI_Type_vector(rows_per_block,    
	   cols_per_block,                  
	   cols_per_block + 2,         
	   MPI_SHORT,       
	   &derived_type_block_array);       

	MPI_Type_commit(&derived_type_block_array);

	/* SECTION B
		Create our virtual topology
		For the current proccess in the new topology
			Find the ranks of its 8 neighbours
			Calculate its piece/boundaries of the game matrix 
		Matrix allocation, initialization and 'scatter' to processes
	*/

  	//Create our virtual topology
  	MPI_Comm virtual_comm;
  	int ndimansions, reorder;
  	int dimansion_size[2], periods[2];
  	ndimansions = 2;
  	dimansion_size[0] = blocks_per_row;
  	dimansion_size[1] = blocks_per_col;
  	periods[0] = 1;
  	periods[1] = 1;
  	reorder = 1;

  	int ret = MPI_Cart_create(MPI_COMM_WORLD, ndimansions, dimansion_size, periods, reorder, &virtual_comm);
  	MPI_Comm_rank(virtual_comm, &my_rank); //rank might have changed due to reorder

  	//find neighbour processes ranks
  	int rank_u, rank_d, rank_r, rank_l;
  	int rank_ur,  rank_ul ,rank_dr, rank_dl;
  	int neighbour_coords[2];
  	int my_coords[2];

  	if (ret == MPI_COMM_NULL) 
  	{
  		printf("Process %d wont be included in this game of life.\n", my_rank);
  		printf("The grid is smaller than available processes!\n");
  		printf("Aborting.. Try different numbers for better results!\n");
  		MPI_Abort(MPI_COMM_WORLD, -2);
  	}
  	else //get this process's rank and coords
  	{
  		//get this process's coordinates in the virtual topology
  		ret = MPI_Cart_coords(virtual_comm, my_rank, ndimansions, my_coords);

  		//get the ranks of this process's (8) neighbours
  		neighbour_coords[0] = my_coords[0] - 1;
  		neighbour_coords[1] = my_coords[1];
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_u);

  		neighbour_coords[0] = my_coords[0] + 1;
  		neighbour_coords[1] = my_coords[1];
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_d);

  		neighbour_coords[0] = my_coords[0];
  		neighbour_coords[1] = my_coords[1] + 1;
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_r);

  		neighbour_coords[0] = my_coords[0];
  		neighbour_coords[1] = my_coords[1] - 1;
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_l);

  		neighbour_coords[0] = my_coords[0] - 1;
  		neighbour_coords[1] = my_coords[1] + 1;
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_ur);

  		neighbour_coords[0] = my_coords[0] - 1;
  		neighbour_coords[1] = my_coords[1] - 1;
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_ul);

  		neighbour_coords[0] = my_coords[0] + 1;
  		neighbour_coords[1] = my_coords[1] + 1;
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_dr);

  		neighbour_coords[0] = my_coords[0] + 1;
  		neighbour_coords[1] = my_coords[1] - 1;
  		ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &rank_dl);

  		if (DEBUG)
	  	{
	  		printf("Process %d : My coords are %d - %d\nNeighbours of %d: %d %d %d %d %d %d %d %d", my_rank, my_coords[0], my_coords[1], my_rank, rank_u, rank_d, rank_r, rank_l, rank_ur, rank_ul, rank_dr, rank_dl);
	  	}
	}

  	//Calculate this process's boundaries
	int row_start, row_end, col_start, col_end;
  	
	//we use the process's coordinates instead of its rank
	//because mpi might have reordered the processes for better performance (virtual topology)
	// row_start = my_coords[0] * rows_per_block;
	// row_end = row_start + rows_per_block - 1;
	// col_start = my_coords[1] * cols_per_block;
	// col_end = col_start + cols_per_block - 1;

  row_start = 1;
  row_end = rows_per_block;
  col_start = 1;
  col_end = cols_per_block;

	//DEBUG PRINT FOR BOUNDARIES (only master prints it)
	if (DEBUG)
  	{
		if (my_rank == 0)
		{
			int proccess_rank;
			int row_start, row_end, col_start, col_end;
			int i, j;
	  		
	  		for (i = 0; i < blocks_per_row; i++)
	  		{
	  			for (j = 0; j < blocks_per_col; j++)
	  			{
				  	row_start = i * rows_per_block;
				  	row_end = row_start + rows_per_block - 1;
				  	col_start = j * cols_per_block;
				  	col_end = col_start + cols_per_block - 1;

				  	neighbour_coords[0] = i;
				  	neighbour_coords[1] = j;
				  	ret = MPI_Cart_rank(virtual_comm, neighbour_coords, &proccess_rank);
			
					printf("%d: rows [%d-%d] cols [%d-%d]\n", proccess_rank, row_start, row_end, col_start, col_end);
	  			}
	  		}
		}
	} 	

	//allocate and init two gol_arrays with two more rows and two more cols
	ga1 = gol_array_init(rows_per_block + 2, cols_per_block + 2);
	ga2 = gol_array_init(rows_per_block + 2, cols_per_block + 2);

	//read/generate game of life array
	//'scatter' game matrix (send the coordinates to the correct process)
  	if (my_rank == 0) 
  	{

  		if (filename != NULL) 
  		{
  			gol_array_read_file_and_scatter(filename, ga1, processors, N, M, rows_per_block, cols_per_block, blocks_per_row, virtual_comm);
  		}
  		else 
  		{//no input file given, generate a random game array
  			if (INFO)
  			{
  				printf("No input file given as argument\n");
  				printf("Generating a random game of life array to play\n");
  			}
  			gol_array_generate_and_scatter(ga1, processors, N, M, rows_per_block, cols_per_block, blocks_per_row, virtual_comm);
  		}
  	}
  	else //for other processes besides master
  	{
  		//receive coordinates of alive cells, until we receive (-1,-1)
  		short int** array = ga1->array;
  		int finished = 0;

  		while (!finished)
  		{
  			MPI_Recv(coordinates, 2, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
  			if (DEBUG) {
  				fprintf(stderr, "%d: I received (%d,%d)\n", my_rank, coordinates[0], coordinates[1]);
  			}
  			if (coordinates[0] == -1 && coordinates[1] == -1)
  			{
  				finished = 1;
  			}
  			else
  			{
          int x = coordinates[0] % rows_per_block + 1;
          int y = coordinates[1] % cols_per_block + 1; 
  				array[x][y] = 1;
  			}
  		}
  	}

  	//DEBUG PRINT FOR ARRAY SCATTER
  	if (DEBUG)
  	{
  		char outname[20];
  		mkdir("test_outs", 0777);
  		sprintf(outname, "test_outs/out_%d", my_rank);

  		FILE* out = fopen(outname, "w");

	  	fprintf(out, "PROCESS %d ARRAY\n", my_rank);
	  	int r,c;

		for (r=row_start; r<=row_end; r++)
		{
			fputc('|', out);

			for (c=col_start; c<=col_end; c++)
			{
				if (ga1->array[r][c] == 1)
					fputc('o', out);
				else
					fputc(' ', out);

				fputc('|', out);
			}

			fputc('\n', out);
		}

		fclose(out);
	}

	/* SECTION C
		Define communication (requests) that will be made in EVERY loop with MPI_Init
		Game of life loop (with timing)
			x8 MPI_ISend (MPI_Start)
			x8 MPI_IRecv (MPI_Start)
			Calculate 'inner' cells
			Wait for IRecvs to complete
			Calculate 'outer' cells
			MPI_Allreduce for master to see if there was a change or not
			If (change && repeats < max_repeats)
				repeat
			else
				finish
			Wait for ISends to complete (it will return immediatly if the above is used)
	*/

	//Define communication (requests) that will be made in each loop with MPI_Init
	short int** array1 = ga1->array;
	short int** array2 = ga2->array;
	
	//We need to define two types of requests
	//Because the send/receive buffer is changed due to swapping after each loop/generation
	
	//Define tags to seperate messages
	//Maybe up neighbour and down neighbour is the same process, or even it is ourselves..
	//So we need to know which receive goes to which array position
	int send_tags[8];
	send_tags[0] = 1; //send up
	send_tags[1] = 2; //send down
	send_tags[2] = 3; //send left
	send_tags[3] = 4; //send right
	send_tags[4] = 5; //send up left
	send_tags[5] = 6; //send up right
	send_tags[6] = 7; //send down left
	send_tags[7] = 8; //send down right

	int receive_tags[8];
	receive_tags[0] = send_tags[1];//receive up what neighbour sent down
	receive_tags[1] = send_tags[0];
	receive_tags[2] = send_tags[3];//receive left what neighbour sent right
	receive_tags[3] = send_tags[2];
	receive_tags[4] = send_tags[7];//receive up left what neighbour sent down right
	receive_tags[5] = send_tags[6];
	receive_tags[6] = send_tags[5];
	receive_tags[7] = send_tags[4];

	//first the send requests
	MPI_Request send_request[2][8];
	//rows
	if (line_div != 1) {
		MPI_Send_init( &array1[row_start][col_start], cols_per_block, MPI_SHORT, rank_u, send_tags[0], virtual_comm, &send_request[0][0]);
		MPI_Send_init( &array1[row_end][col_start],   cols_per_block, MPI_SHORT, rank_d, send_tags[1], virtual_comm, &send_request[0][1]);
		MPI_Send_init( &array2[row_start][col_start], cols_per_block, MPI_SHORT, rank_u, send_tags[0], virtual_comm, &send_request[1][0]);
		MPI_Send_init( &array2[row_end][col_start],   cols_per_block, MPI_SHORT, rank_d, send_tags[1], virtual_comm, &send_request[1][1]);
	}
	else { //process has all the rows it needs
		MPI_Send_init( &array1[row_start][col_start], cols_per_block, MPI_SHORT, MPI_PROC_NULL, send_tags[0], virtual_comm, &send_request[0][0]);
		MPI_Send_init( &array1[row_end][col_start],   cols_per_block, MPI_SHORT, MPI_PROC_NULL, send_tags[1], virtual_comm, &send_request[0][1]);
		MPI_Send_init( &array2[row_start][col_start], cols_per_block, MPI_SHORT, MPI_PROC_NULL, send_tags[0], virtual_comm, &send_request[1][0]);
		MPI_Send_init( &array2[row_end][col_start],   cols_per_block, MPI_SHORT, MPI_PROC_NULL, send_tags[1], virtual_comm, &send_request[1][1]);
	}
	//cols
	if (col_div != 1) {
		MPI_Send_init( &array1[row_start][col_start], 1, derived_type_block_col, rank_l, send_tags[2], virtual_comm, &send_request[0][2]);
		MPI_Send_init( &array1[row_start][col_end],   1, derived_type_block_col, rank_r, send_tags[3], virtual_comm, &send_request[0][3]);
		MPI_Send_init( &array2[row_start][col_start], 1, derived_type_block_col, rank_l, send_tags[2], virtual_comm, &send_request[1][2]);
		MPI_Send_init( &array2[row_start][col_end],   1, derived_type_block_col, rank_r, send_tags[3], virtual_comm, &send_request[1][3]);
	}
	else { // process has all the col it needs
		MPI_Send_init( &array1[row_start][col_start], 1, derived_type_block_col, MPI_PROC_NULL, send_tags[2], virtual_comm, &send_request[0][2]);
		MPI_Send_init( &array1[row_start][col_end],   1, derived_type_block_col, MPI_PROC_NULL, send_tags[3], virtual_comm, &send_request[0][3]);
		MPI_Send_init( &array2[row_start][col_start], 1, derived_type_block_col, MPI_PROC_NULL, send_tags[2], virtual_comm, &send_request[1][2]);
		MPI_Send_init( &array2[row_start][col_end],   1, derived_type_block_col, MPI_PROC_NULL, send_tags[3], virtual_comm, &send_request[1][3]);
	}
	//corners
	if (line_div != 1 || col_div != 1) {
		MPI_Send_init( &array1[row_start][col_start], 1, MPI_SHORT, rank_ul, send_tags[4], virtual_comm, &send_request[0][4]);
		MPI_Send_init( &array1[row_start][col_end],   1, MPI_SHORT, rank_ur, send_tags[5], virtual_comm, &send_request[0][5]);
		MPI_Send_init( &array1[row_end][col_start],   1, MPI_SHORT, rank_dl, send_tags[6], virtual_comm, &send_request[0][6]);
		MPI_Send_init( &array1[row_end][col_end],     1, MPI_SHORT, rank_dr, send_tags[7], virtual_comm, &send_request[0][7]);
		MPI_Send_init( &array2[row_start][col_start], 1, MPI_SHORT, rank_ul, send_tags[4], virtual_comm, &send_request[1][4]);
		MPI_Send_init( &array2[row_start][col_end],   1, MPI_SHORT, rank_ur, send_tags[5], virtual_comm, &send_request[1][5]);
		MPI_Send_init( &array2[row_end][col_start],   1, MPI_SHORT, rank_dl, send_tags[6], virtual_comm, &send_request[1][6]);
		MPI_Send_init( &array2[row_end][col_end],     1, MPI_SHORT, rank_dr, send_tags[7], virtual_comm, &send_request[1][7]);
	}
	else {
		MPI_Send_init( &array1[row_start][col_start], 1, MPI_SHORT, MPI_PROC_NULL, send_tags[4], virtual_comm, &send_request[0][4]);
		MPI_Send_init( &array1[row_start][col_end],   1, MPI_SHORT, MPI_PROC_NULL, send_tags[5], virtual_comm, &send_request[0][5]);
		MPI_Send_init( &array1[row_end][col_start],   1, MPI_SHORT, MPI_PROC_NULL, send_tags[6], virtual_comm, &send_request[0][6]);
		MPI_Send_init( &array1[row_end][col_end],     1, MPI_SHORT, MPI_PROC_NULL, send_tags[7], virtual_comm, &send_request[0][7]);
		MPI_Send_init( &array2[row_start][col_start], 1, MPI_SHORT, MPI_PROC_NULL, send_tags[4], virtual_comm, &send_request[1][4]);
		MPI_Send_init( &array2[row_start][col_end],   1, MPI_SHORT, MPI_PROC_NULL, send_tags[5], virtual_comm, &send_request[1][5]);
		MPI_Send_init( &array2[row_end][col_start],   1, MPI_SHORT, MPI_PROC_NULL, send_tags[6], virtual_comm, &send_request[1][6]);
		MPI_Send_init( &array2[row_end][col_end],     1, MPI_SHORT, MPI_PROC_NULL, send_tags[7], virtual_comm, &send_request[1][7]);
	}

	//then the receive requests
	MPI_Request recv_request[2][8];
	int up_row = 0;
	int down_row = row_end + 1;
	int left_col = 0;
	int right_col = col_end + 1;

	//rows
	if (line_div != 1) {
		MPI_Recv_init( &array1[up_row][col_start],   cols_per_block, MPI_SHORT, rank_u, receive_tags[0], virtual_comm, &recv_request[0][0]);
		MPI_Recv_init( &array1[down_row][col_start], cols_per_block, MPI_SHORT, rank_d, receive_tags[1], virtual_comm, &recv_request[0][1]);
		MPI_Recv_init( &array2[up_row][col_start],   cols_per_block, MPI_SHORT, rank_u, receive_tags[0], virtual_comm, &recv_request[1][0]);
		MPI_Recv_init( &array2[down_row][col_start], cols_per_block, MPI_SHORT, rank_d, receive_tags[1], virtual_comm, &recv_request[1][1]);
	}
	else {
		MPI_Recv_init( &array1[up_row][col_start],   cols_per_block, MPI_SHORT, MPI_PROC_NULL, receive_tags[0], virtual_comm, &recv_request[0][0]);
		MPI_Recv_init( &array1[down_row][col_start], cols_per_block, MPI_SHORT, MPI_PROC_NULL, receive_tags[1], virtual_comm, &recv_request[0][1]);
		MPI_Recv_init( &array2[up_row][col_start],   cols_per_block, MPI_SHORT, MPI_PROC_NULL, receive_tags[0], virtual_comm, &recv_request[1][0]);
		MPI_Recv_init( &array2[down_row][col_start], cols_per_block, MPI_SHORT, MPI_PROC_NULL, receive_tags[1], virtual_comm, &recv_request[1][1]);
	}
	//cols
	if (col_div != 1) {
		MPI_Recv_init( &array1[row_start][left_col],  1, derived_type_block_col, rank_l, receive_tags[2], virtual_comm, &recv_request[0][2]);
		MPI_Recv_init( &array1[row_start][right_col], 1, derived_type_block_col, rank_r, receive_tags[3], virtual_comm, &recv_request[0][3]);
		MPI_Recv_init( &array2[row_start][left_col],  1, derived_type_block_col, rank_l, receive_tags[2], virtual_comm, &recv_request[1][2]);
		MPI_Recv_init( &array2[row_start][right_col], 1, derived_type_block_col, rank_r, receive_tags[3], virtual_comm, &recv_request[1][3]);
	}
	else {
		MPI_Recv_init( &array1[row_start][left_col],  1, derived_type_block_col, MPI_PROC_NULL, receive_tags[2], virtual_comm, &recv_request[0][2]);
		MPI_Recv_init( &array1[row_start][right_col], 1, derived_type_block_col, MPI_PROC_NULL, receive_tags[3], virtual_comm, &recv_request[0][3]);
		MPI_Recv_init( &array2[row_start][left_col],  1, derived_type_block_col, MPI_PROC_NULL, receive_tags[2], virtual_comm, &recv_request[1][2]);
		MPI_Recv_init( &array2[row_start][right_col], 1, derived_type_block_col, MPI_PROC_NULL, receive_tags[3], virtual_comm, &recv_request[1][3]);		
	}
	//corners
	if (line_div != 1 || col_div != 1) {
		MPI_Recv_init( &array1[up_row][left_col],  1, MPI_SHORT, rank_ul, receive_tags[4], virtual_comm, &recv_request[0][4]);
		MPI_Recv_init( &array1[up_row][right_col], 1, MPI_SHORT, rank_ur, receive_tags[5], virtual_comm, &recv_request[0][5]);
		MPI_Recv_init( &array1[down_row][left_col],  1, MPI_SHORT, rank_dl, receive_tags[6], virtual_comm, &recv_request[0][6]);
		MPI_Recv_init( &array1[down_row][right_col], 1, MPI_SHORT, rank_dr, receive_tags[7], virtual_comm, &recv_request[0][7]);
		MPI_Recv_init( &array2[up_row][left_col],  1, MPI_SHORT, rank_ul, receive_tags[4], virtual_comm, &recv_request[1][4]);
		MPI_Recv_init( &array2[up_row][right_col], 1, MPI_SHORT, rank_ur, receive_tags[5], virtual_comm, &recv_request[1][5]);
		MPI_Recv_init( &array2[down_row][left_col],  1, MPI_SHORT, rank_dl, receive_tags[6], virtual_comm, &recv_request[1][6]);
		MPI_Recv_init( &array2[down_row][right_col], 1, MPI_SHORT, rank_dr, receive_tags[7], virtual_comm, &recv_request[1][7]);
	}
	else {
		MPI_Recv_init( &array1[up_row][left_col],  1, MPI_SHORT, MPI_PROC_NULL, receive_tags[4], virtual_comm, &recv_request[0][4]);
		MPI_Recv_init( &array1[up_row][right_col], 1, MPI_SHORT, MPI_PROC_NULL, receive_tags[5], virtual_comm, &recv_request[0][5]);
		MPI_Recv_init( &array1[down_row][left_col],  1, MPI_SHORT, MPI_PROC_NULL, receive_tags[6], virtual_comm, &recv_request[0][6]);
		MPI_Recv_init( &array1[down_row][right_col], 1, MPI_SHORT, MPI_PROC_NULL, receive_tags[7], virtual_comm, &recv_request[0][7]);
		MPI_Recv_init( &array2[up_row][left_col],  1, MPI_SHORT, MPI_PROC_NULL, receive_tags[4], virtual_comm, &recv_request[1][4]);
		MPI_Recv_init( &array2[up_row][right_col], 1, MPI_SHORT, MPI_PROC_NULL, receive_tags[5], virtual_comm, &recv_request[1][5]);
		MPI_Recv_init( &array2[down_row][left_col],  1, MPI_SHORT, MPI_PROC_NULL, receive_tags[6], virtual_comm, &recv_request[1][6]);
		MPI_Recv_init( &array2[down_row][right_col], 1, MPI_SHORT, MPI_PROC_NULL, receive_tags[7], virtual_comm, &recv_request[1][7]);
	}

	int count;
	int no_change;
	int no_change_sum;

	int communication_type = 0; //switches between 0 and 1 after each loop
	MPI_Status statuses[8];

	MPI_Barrier(MPI_COMM_WORLD);

	double start, finish;
  gol_array* whole_array;

	if (PRINT_INITIAL)
	{
    whole_array = gol_array_init(N, M);
    short int** array = whole_array->array;
		gol_array_gather(array, array1, my_rank, processors, row_start, col_start,
			blocks_per_row, blocks_per_col, rows_per_block, cols_per_block, derived_type_block_array, virtual_comm);
		if (my_rank == 0) {
			printf("Printing initial array:\n\n");
			fflush(stdout);
			print_array(array, N, M);
			fflush(stdout);
			putchar('\n');
		}
    gol_array_free(&whole_array);	
	}

  if (PRINT_STEPS) {
    whole_array = gol_array_init(N, M);
  }

	if (STATUS && my_rank == 0)
		printf("Starting the Game of Life\n");

	start = MPI_Wtime();

	for(count = 0; count < max_loops; count++) 
	{
		no_change = 1;

		//8 Isend
		MPI_Startall(8, send_request[communication_type]);
		//8 IRecv
		MPI_Startall(8, recv_request[communication_type]);

		//calculate/populate 'inner' cells
		for (i=row_start + 1; i<= row_end - 1; i++) {
			for (j= col_start + 1; j <= col_end - 1; j++) {
				//for each cell/organism

				//for each cell/organism
				//see if there is a change
				//populate functions applies the game's rules
				//and returns 0 if a change occurs
				if (populate(array1, array2, N, M, i, j) == 0)
					no_change = 0;	
			}
		}

		//wait for recvs
		MPI_Waitall(8, recv_request[communication_type], statuses);

		//calculate/populate 'outer' cells
		
		//up/down row
		for (j= col_start; j <= col_end; j++) {
			if (populate(array1, array2, N, M, row_start, j) == 0)
				no_change = 0;
			if (populate(array1, array2, N, M, row_end, j) == 0)
				no_change = 0;
		}

		//left/right col
		for (i=row_start; i<= row_end; i++) {
			if (populate(array1, array2, N, M, i, col_start) == 0)
				no_change = 0;
			if (populate(array1, array2, N, M, i, col_end) == 0)
				no_change = 0;
		}

		//corners
		if (populate(array1, array2, N, M, row_start, col_start) == 0)
				no_change = 0;
		if (populate(array1, array2, N, M, row_start, col_end) == 0)
				no_change = 0;
		if (populate(array1, array2, N, M, row_end, col_start) == 0)
				no_change = 0;
		if (populate(array1, array2, N, M, row_end, col_end) == 0)
				no_change = 0;


		if ( REDUCE_RATE > 0 && (count + 1) % REDUCE_RATE == 0)
		{			
			MPI_Allreduce(&no_change, &no_change_sum, 1, MPI_SHORT, MPI_SUM, virtual_comm);
			if (no_change_sum == processors )
			{
				if (my_rank == 0 && STATUS) 
					printf("Terminating because there was no change at loop number %d\n", count);

				break;
			}

		}

		//wait for sends
		MPI_Waitall(8, send_request[communication_type], statuses);

		//only the master process prints
		if (PRINT_STEPS) {
      short int** array = whole_array->array;
			gol_array_gather(array, array2, my_rank, processors, row_start, col_start,
			blocks_per_row, blocks_per_col, rows_per_block, cols_per_block, derived_type_block_array, virtual_comm);
			if (my_rank == 0) {
				fflush(stdout);
				print_array(array, N, M);
				fflush(stdout);
			}
			putchar('\n');
		}

		//swap arrays (array2 becomes array1)
		short int** temp;
		temp = array1;
		array1 = array2;
		array2 = temp;
		communication_type = (communication_type + 1) % 2;

	}


	if (my_rank == 0) 
	{
		if (my_rank == 0 && no_change == 0 && STATUS)
		{
			printf("Max loop number (%d) was reached. Terminating Game of Life\n", max_loops);
		}
	}

	finish = MPI_Wtime();

	double local_elapsed, elapsed;
	local_elapsed = finish - start;

	if (INFO)
		printf("Process %d > Time elapsed: %f seconds\n", my_rank, local_elapsed);

	//reduce to check if the array has changed every reduce_rate loops
	MPI_Reduce(&local_elapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

	if (TIME && my_rank == 0)
		printf("Time elapsed: %f seconds\n", elapsed);

	if (PRINT_FINAL)
	{
		//Gather the whole (final) gol array into master so he can print it out
    if (!PRINT_STEPS) {
      whole_array = gol_array_init(rows_per_block + 2, cols_per_block + 2);
    }
    short int** array = whole_array->array;
		gol_array_gather(array, array1, my_rank, processors, row_start, col_start,
			blocks_per_row, blocks_per_col, rows_per_block, cols_per_block, derived_type_block_array, virtual_comm);
		if (my_rank == 0) {
			fflush(stdout);
			print_array(array, N, M);
			fflush(stdout);
		}
    if (!PRINT_STEPS) {
      gol_array_free(&whole_array);
    }
	}

	//free arrays
	gol_array_free(&ga1);
	gol_array_free(&ga2);

  if (PRINT_STEPS) {
    gol_array_free(&whole_array);
  }

	MPI_Finalize();

	return 0;

}


void gol_array_read_file_and_scatter(char* filename, gol_array* gol_ar, int processors, int lines, int columns,
										int rows_per_block, int cols_per_block, int blocks_per_row, MPI_Comm virtual_comm)
{
	short int** array = gol_ar->array;
	int N = lines;
	int M = columns;
	int tag = 201049;

	char line[100];
	char copy[100];
	int counter = 0;
	int successful = 0;
	int row_of_block;
	int col_of_block;
	int destination_process;
	int coordinates[2];

	FILE* file = fopen(filename, "r");

	if (file == NULL) 
	{
		printf("Error opening file\n");
		MPI_Abort(MPI_COMM_WORLD, -1);
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

		row_of_block = (row - 1) / rows_per_block;
		col_of_block = (col - 1) / cols_per_block;
		int neighbour_coords[2];
		neighbour_coords[0] = row_of_block;
		neighbour_coords[1] = col_of_block;
		MPI_Cart_rank(virtual_comm, neighbour_coords, &destination_process);
		if (DEBUG) {
			printf("(%d,%d) should go to process %d\n", row-1,col-1,destination_process);
		}

		//if its for the master process
		if (destination_process == 0)
			array[row-1+1][col-1+1] = 1; // +1 due to extra row,col kept
		else //if its for another process
		{
			//send the coordinates
			coordinates[0] = row - 1;
			coordinates[1] = col - 1;
			MPI_Send(coordinates, 2, MPI_INT, destination_process, tag, MPI_COMM_WORLD);
		}

	}

	int i;
	coordinates[0] = -1;
	coordinates[1] = -1;

	//send 'ok' message to all processes
	for (i=1; i<processors; i++)
		MPI_Send(coordinates, 2, MPI_INT, i, tag, MPI_COMM_WORLD);


	if (INFO)
		printf("\nSuccesfully read %d coordinates\n", successful);
	fclose(file);
}


void gol_array_generate_and_scatter(gol_array* gol_ar, int processors, int lines, int columns,
										int rows_per_block, int cols_per_block, int blocks_per_row, MPI_Comm virtual_comm)
{
	char datestr[9];
	char timestr[7];

	get_date_time_str(datestr, timestr);
	char filename[36];
	mkdir("generated_tests", 0777);
	sprintf(filename, "generated_tests/rga_%s_%s", datestr, timestr);
	
	FILE* file = fopen(filename, "w");
	if (file == NULL)
	{
		printf("gol_array_generate error opening file!\n");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	short int** array = gol_ar->array;

	srand(time(NULL));
	int alive_count = rand() % (lines*columns + 1);
	int i;
	int tag = 201049;
	int row_of_block;
	int col_of_block;
	int destination_process;
	int coordinates[2];

	for (i=0; i<alive_count; i++)
	{
		int x,y;

		x = rand() % lines;
		y = rand() % columns;

		fprintf(file, "%d %d\n", x+1, y+1);

		row_of_block = x / rows_per_block;
		col_of_block = y / cols_per_block;
		int neighbour_coords[2];
		neighbour_coords[0] = row_of_block;
		neighbour_coords[1] = col_of_block;
		MPI_Cart_rank(virtual_comm, neighbour_coords, &destination_process);
		if (DEBUG) {
			printf("(%d,%d) should go to process %d\n", x,y,destination_process);
		}

		//if its for the master process
		if (destination_process == 0)
			array[x+1][y+1] = 1; // +1 due to extra col/row kept
		else //if its for another process
		{
			coordinates[0] = x;
			coordinates[1] = y;
			MPI_Send(coordinates, 2, MPI_INT, destination_process, tag, MPI_COMM_WORLD);
		}

		// array[x][y] = 1;
	}

	coordinates[0] = -1;
	coordinates[1] = -1;

	for (i=1; i<processors; i++)
		MPI_Send(coordinates, 2, MPI_INT, i, tag, MPI_COMM_WORLD);

	fclose(file);
}


void gol_array_gather(short int** array, short int** myarray, int my_rank, int processes, int row_start, int col_start,
	int blocks_per_row, int blocks_per_col, int rows_per_block, int cols_per_block, MPI_Datatype block_array, MPI_Comm virtual_comm)
{
	int tag = 201400201;
  gol_array* block_gol_array = gol_array_init(rows_per_block + 2, cols_per_block + 2);
  short int** neighbour_block_array = block_gol_array->array;

	if (my_rank == 0)
	{
    //copy master processes array to whole array

    for (int i = 0; i < rows_per_block; i++)
    {
      memcpy(&(array[i][0]), &(myarray[i+1][1]), cols_per_block*sizeof(short int));
    }

		MPI_Status status;
		int i, j;
		int receive_process;
		int neighbour_coords[2];
		int neigbour_rstart;
		int neigbour_cstart;

		for (i = 0; i < blocks_per_row; i++) {
			for (j = 0; j < blocks_per_col; j++) {
				if (i != 0 || j != 0) {
					neigbour_rstart = i * rows_per_block;
					neigbour_cstart = j * cols_per_block;
					neighbour_coords[0] = i;
					neighbour_coords[1] = j;
					MPI_Cart_rank(virtual_comm, neighbour_coords, &receive_process);
					if (DEBUG) {
						fprintf(stderr, "Receiving from %d starting from row %d and col %d\n", receive_process, neigbour_rstart, neigbour_cstart);
					}
					MPI_Recv(&(neighbour_block_array[1][1]), 1, block_array,
					 receive_process, tag, virtual_comm, &status);
          int neighbour_row = neigbour_rstart;
          for (int i = 1; i <= rows_per_block; i++)
          {
            memcpy(&(array[neighbour_row][neigbour_cstart]),
                    &(neighbour_block_array[i][1]), cols_per_block*sizeof(short int));
            neighbour_row++;
          }
				}
			}
		}
	}
	else
	{
		MPI_Send(&(myarray[row_start][col_start]), 1, block_array, 0, tag, virtual_comm);
	}

  gol_array_free(&block_gol_array);
}
