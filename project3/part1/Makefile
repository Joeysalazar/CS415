CC = gcc
CFLAGS = -g -Wall -pthread
LDFLAGS = -lm

all: park

park: park.c
	$(CC) $(CFLAGS) park.c -o park $(LDFLAGS)

clean:
	rm -f park
