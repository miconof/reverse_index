
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
/*#include "globals.h"*/
#include "timer.h"
/*#include "thread.h"*/

// Debug prints to be used inside parallel sections
#define DEBUG(...) do { \
    if (debug) \
        printf(__VA_ARGS__); \
} while (0)


// Prints usage information
void
usage (char* argv0)
{
    char* help =
        "Usage: %s [switches] -i data_dir\n"
        "       -i data_dir:    data directory that contains files to be parsed\n"
        "       -t nthreads:    number of threads\n"
        "       -d:             enables debug facilities, slow performance\n";
    fprintf(stderr, help, argv0);
    exit(-1);
}

int
main (int argc, char** argv)
{

    // Total Running time
    double totalTime = 0.0;

    printf("\nRunning Reverse Indexing...\n");

    // Global arguments
    int nthreads;
    char* data_dir;
    int debug;

    if (argc == 1) // at least one argument (data directory)
        usage((char*)argv[0]);

    nthreads = 1;
    debug = 0;
    int opt;
    while ((opt = getopt(argc,(char**)argv,"i:t:d")) != EOF) {
        switch (opt) {
            case 'i': data_dir = optarg;
                      break;
            case 't': nthreads = atoi(optarg);
                      break;
            case 'd': debug = 1;
                      break;
            case '?': usage((char*)argv[0]);
                      break;
            default: usage((char*)argv[0]);
                      break;
        }
    }

    // Sanity checks
    if (data_dir == 0) {
        usage((char*)argv[0]);
    }
    int infile;
    if ((infile = open(data_dir, O_RDONLY, "0600")) == -1) {
        fprintf(stderr, "Error: no such directory (%s)\n", data_dir);
        exit(1);
    }

    printf("\nData directory:    %s", data_dir);
    printf("\nNumber of threads: %d", nthreads);
    printf("\nDebug flag:        %d", debug);


    // Create thread pool
    /*thread_startup(nthreads); // Creates pull of threads*/


    // Timer to count different execution phases
    TIMER_T start;
    TIMER_T stop;
    TIMER_READ(start);

    /*thread_start(parseFiles, data_dir); // Start threads*/

    TIMER_READ(stop);

    // Calculate the time that took the parsing
    double parsingTime = TIMER_DIFF_SECONDS(start, stop);
    totalTime += parsingTime;
    printf("\n\nTime taken for parsing files is %9.6f sec.\n", parsingTime);


    // Reduction
    TIMER_READ(start);

    /*thread_start(reduce, NULL); // will need to pass the data structure from parseFiles*/

    TIMER_READ(stop);

    // Calculate the time that took the parsing
    double reduceTime = TIMER_DIFF_SECONDS(start, stop);
    totalTime += reduceTime;
    printf("\n\nTime taken for the reduction is %9.6f sec.\n", reduceTime);


    printf("\n\nTOTAL Time for both operations is %9.6f sec.\n", totalTime);


    // Clean up
    /*thread_shutdown();*/

    return 0;
}
