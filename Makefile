OBJS = gol.o gol_array.o functions.o file_generator.o
SOURCE = gol.c gol_array.c functions.c file_generator.c
HEADER = gol_array.h functions.h
CC = gcc
CFLAGS= -c -Wall
LFLAGS= -Wall

all: gol fgen

gol: gol.o gol_array.o functions.o
	$(CC) $(LFLAGS) gol.o gol_array.o functions.o -o gol

fgen: file_generator.o
	$(CC) $(LFLAGS) file_generator.o -o fgen	

gol.o: gol.c
	$(CC) $(CFLAGS) gol.c

gol_array.o: gol_array.c
	$(CC) $(CFLAGS) gol_array.c

functions.o: functions.c
	$(CC) $(CFLAGS) functions.c

file_generator.o: file_generator.c
	$(CC) $(CFLAGS) file_generator.c

clean:
	rm -f $(OBJS) gol fgen

count:
	wc $(SOURCE) $(HEADER)