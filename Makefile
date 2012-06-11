EXECUTABLES = reverse_index
CC = gcc
LD = gcc
CFLAGS = -Wall -pthread

all: reverse_index

reverse_index: reverse_index.c Makefile
	$(CC) $(CFLAGS) -o rindex reverse_index.c

.PHONY: clean
clean:
	rm -rf *.o $(EXECUTABLES)
