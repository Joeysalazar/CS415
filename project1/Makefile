# Makefile for Project 1 - pseudo-shell

CC = gcc
CFLAGS = -Wall -Wextra -g
OBJ = main.o command.o string_parser.o

all: pseudo-shell

pseudo-shell: $(OBJ)
	$(CC) $(CFLAGS) -o pseudo-shell $(OBJ)

main.o: main.c command.h string_parser.h
	$(CC) $(CFLAGS) -c main.c

command.o: command.c command.h
	$(CC) $(CFLAGS) -c command.c

string_parser.o: string_parser.c string_parser.h
	$(CC) $(CFLAGS) -c string_parser.c

clean:
	rm -f *.o pseudo-shell output.txt

