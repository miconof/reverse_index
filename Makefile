EXECUTABLES = rindex rindex.prof rindex.debug
CC = g++
LD = g++
CFLAGS = -Wall -pthread -std=c++0x -O3

all: rindex profile debug

rindex: reverse_index.cc Makefile
	$(CC) $(CFLAGS) -o rindex reverse_index.cc

profile: reverse_index.cc Makefile
	$(CC) $(CFLAGS) -pg -o rindex.prof reverse_index.cc

debug: reverse_index.cc Makefile
	$(CC) $(CFLAGS) -g -o rindex.debug reverse_index.cc

.PHONY: clean
clean:
	rm -rf *.o $(EXECUTABLES)
