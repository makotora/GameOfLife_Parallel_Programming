#ifndef GOL_ARRAY_H
#define GOL_ARRAY_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "functions.h"

struct gol_array
{
	short int** array;
	int lines;
	int columns;
};

typedef struct gol_array gol_array;

gol_array* gol_array_init(int lines, int columns);
void gol_array_free(gol_array** gol_ar);
void gol_array_read_input(gol_array* gol_ar);
void gol_array_read_file(FILE* file, gol_array* gol_ar);
void gol_array_generate(gol_array* gol_ar);

#endif