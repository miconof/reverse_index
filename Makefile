EXECUTABLES = reverse_index
CC = g++
LD = g++
CFLAGS = -Wall -pthread -std=c++0x -O3

all: rindex profile

rindex: reverse_index.cc Makefile
	$(CC) $(CFLAGS) -o rindex reverse_index.cc

profile: reverse_index.cc Makefile
	$(CC) $(CFLAGS) -pg -o rindex.prof reverse_index.cc

.PHONY: clean
clean:
	rm -rf *.o $(EXECUTABLES)
