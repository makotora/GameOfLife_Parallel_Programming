#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		printf("Invalid number of arguments!\n");
		printf("Usage: ./fgen lines columns alive_count file_name\n");
		return -1;
	}

	int lines, columns, alive_count;
	char* file_name;
	lines = atoi(argv[1]);
	columns = atoi(argv[2]);
	alive_count = atoi(argv[3]);
	file_name = argv[4];
	printf("Lines: %d\nColumns: %d\n", lines, columns);
	printf("Generating file '%s' for %d alive organisms\n", file_name, alive_count);

	FILE* file = fopen(file_name, "w");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		return -1;
	}

	srand(time(NULL));
	int i;

	for (i=0; i<alive_count; i++)
	{
		int x,y;

		x = (rand() % lines) + 1;
		y = (rand() % columns) + 1;

		fprintf(file, "%d %d\n", x, y);
	}

	fclose(file);
	printf("Done\n");

	return 0;
}