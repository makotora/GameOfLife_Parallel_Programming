OBJS = gol.o functions.o
SOURCE = gol.c functions.c
HEADER = functions.h
CC = gcc
CFLAGS= -c -Wall
LFLAGS= -Wall

all: gol

gol: gol.o functions.o
	$(CC) $(LFLAGS) gol.o functions.o -o gol

gol.o: gol.c
	$(CC) $(CFLAGS) gol.c

functions.o: functions.c
	$(CC) $(CFLAGS) functions.c

clean:
	rm -f $(OBJS) gol

count:
	wc $(SOURCE) $(HEADER)