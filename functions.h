#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define N 5
#define M 10

void read_alive(short int** array);
void print_array(short int** array);
int num_of_neighbours(short int** array, int row, int col);
void print_neighbour_nums(short int** array);

#endif