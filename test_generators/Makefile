OBJS = file_generator.o gui_gen.o
SOURCE = file_generator.c gui_gen.c
HEADER =
CC = gcc
CFLAGS= -c -Wall
LFLAGS= -Wall

all: fgen ggen

fgen: file_generator.o
	$(CC) $(LFLAGS) file_generator.o -o fgen

ggen: gui_gen.o
	$(CC) $(LFLAGS) gui_gen.o -o ggen

file_generator.o: file_generator.c
	$(CC) $(CFLAGS) file_generator.c

gui_gen.o: gui_gen.c
	$(CC) $(CFLAGS) gui_gen.c

clean:
	rm -f $(OBJS) fgen ggen

count:
	wc $(SOURCE) $(HEADER)