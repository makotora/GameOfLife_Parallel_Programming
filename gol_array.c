#include "gol_array.h"

gol_array* gol_array_init(int lines, int columns)
{
	short int** array;
	int i, j;

	//allocate a lines*columns array and initialise with zeros
	array = malloc(lines * sizeof(short int*));
	assert(array != NULL);
	
	for (i=0; i<lines; i++)
	{
		array[i] = malloc(columns * sizeof(short int));

		for (j=0; j<columns; j++)
		{
			array[i][j] = 0;
		}
	}

	//allocate gol_array struct
	gol_array* new_gol_array = malloc(sizeof(gol_array));
	new_gol_array->array = array;
	new_gol_array->lines = lines;
	new_gol_array->columns = columns;

	return new_gol_array;
}



void gol_array_free(gol_array** gol_ar)
{
	gol_array* gol_ar_ptr = *gol_ar;
	int i;

	short int** array = gol_ar_ptr->array;
	int N = gol_ar_ptr->lines;

	for (i=0; i<N; i++)
	{
		free(array[i]);
	}

	free(array);
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



void gol_array_read_file(FILE* file, gol_array* gol_ar)
{
	short int** array = gol_ar->array;
	int N = gol_ar->lines;
	int M = gol_ar->columns;

	char line[100];
	char copy[100];
	int counter = 0;
	int successful = 0;

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

		fprintf(file, "%d %d\n", x, y);
		array[x][y] = 1;
	}
}