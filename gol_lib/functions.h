#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gol_array.h"

void print_array(short int** array, int N, int M);
int num_of_neighbours(short int** array, int N, int M, int row, int col);
void print_neighbour_nums(short int** array, int N, int M);
void get_date_time_str(char* datestr, char* timestr);

#endif