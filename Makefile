EXECUTABLES = reverse_index
CC = g++
LD = g++
CFLAGS = -Wall -pthread -std=c++0x -O3

all: reverse_index

reverse_index: reverse_index.cc Makefile
	$(CC) $(CFLAGS) -o rindex reverse_index.cc

.PHONY: clean
clean:
	rm -rf *.o $(EXECUTABLES)
