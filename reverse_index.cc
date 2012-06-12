
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "timer.h"
#include <iostream>
#include <queue>
#include <ftw.h>
#include <fnmatch.h>
#include <string>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
using namespace std;


// Global parameters
int nthreads;
char* data_dir;
int debug;


// Debug prints to be used inside parallel section
// You may want to redirect stdout when enebled
#define DEBUG(...) do { \
    if (debug) \
        printf(__VA_ARGS__); \
} while (0)


// Global queue that holds paths to data files.
queue<string> data_files;


// Defining mutexes
pthread_mutex_t df_mutex;

// Prints usage information
void
usage (const char* argv0)
{
    const char* help =
        "Usage: %s [switches] -i data_dir\n"
        "       -i data_dir:    data directory that contains files to be parsed\n"
        "       -t nthreads:    number of threads\n"
        "       -d:             enables debug facilities, slow performance\n";
    fprintf(stderr, help, argv0);
    exit(-1);
}


// Populate a queue with the relative file paths to data files
static int
populateQueue (const char *fpath, const struct stat *sb, int typeflag)
{
    // if it's a file
    if (typeflag == FTW_F) {
        DEBUG("found file: %s\n", fpath);
        data_files.push(string(fpath));
    }

    // tell ftw to continue
    return 0;
}


string
getDataFilePath ()
{
    string ret;
    if (data_files.empty()) {
        return "";
    } else {
        ret = data_files.front();
        data_files.pop();
        return ret;
    }
}


// While queue of files not empty: Parse files (map) into a
//      thread local dictionary while doing local reduction.
// If queue becomes empty: Add from thread local dictionary to
//      global dictionary.
void*
mapReduce (void*)
{
    string file_path;

    // map with thread-local reduction
    while (true) {
        // Grab a file from the shared global queue for parsing
        pthread_mutex_lock (&df_mutex);
        file_path = getDataFilePath();
        pthread_mutex_unlock (&df_mutex);

        // Check for end of queue
        if (file_path.empty()) break;

        // Load and parse file
        int fd = open(file_path.c_str(), O_RDONLY);
        struct stat sb;
        fstat (fd, &sb);
        void* data = mmap (NULL, sb.st_size, PROT_READ, 0, fd, 0);


        munmap(data, sb.st_size);
    }

    // reduce


    pthread_exit((void*)0);
}

int
main (int argc, char** argv)
{

    printf("\nRunning Reverse Indexing...\n");

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

    // Populates queue of files to be parsed
    if (ftw(data_dir, populateQueue, 16) != 0) {
        fprintf(stderr, "Error: something went wrong reading the files");
    } else {
        printf("\n\nFound %lu files to parse", data_files.size());
    }

    // Create thread id's pool and initialize mutexes
    pthread_t global_threads[nthreads];
    pthread_mutex_init(&df_mutex, NULL);

    // Timer to count parallel region
    TIMER_T start;
    TIMER_T stop;
    TIMER_READ(start);
    long i;
    for (i = 0; i < nthreads; i++) {
        pthread_create(&global_threads[i], NULL, mapReduce, (void*)NULL);
    }
    for (i = 0; i < nthreads; i++) {
        pthread_join(global_threads[i], (void**)NULL);
    }
    TIMER_READ(stop);

    // Destroy mutexes
    pthread_mutex_destroy(&df_mutex);

    // Calculate the time that took the parallel section
    double parallelTime = TIMER_DIFF_SECONDS(start, stop);
    printf("\n\nTime taken for do reverse indexing is %9.6f sec.\n", parallelTime);

    return 0;
}
