OBJS = gol_array.o functions.o
SOURCE = gol_array.c functions.c
HEADER = gol_array.h functions.h
CC = gcc
CFLAGS= -c -Wall
LFLAGS= -Wall

all: gol_array.o functions.o

fgen: file_generator.o
	$(CC) $(LFLAGS) file_generator.o -o fgen

ggen: gui_gen.o
	$(CC) -o ggen gui_gen.c

gol_array.o: gol_array.c
	$(CC) $(CFLAGS) gol_array.c

functions.o: functions.c
	$(CC) $(CFLAGS) functions.c

file_generator.o: file_generator.c
	$(CC) $(CFLAGS) file_generator.c

gui_gen.o: gui_gen.c
	$(CC) $(CFLAGS) gui_gen.c

clean:
	rm -f $(OBJS)

count:
	wc $(SOURCE) $(HEADER)