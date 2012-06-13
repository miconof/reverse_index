
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
#include <set>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
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

vector<string>
getLinks(const char* data)
{
    regex_t regex;
    regmatch_t matches[1];
    int ret, match;
    vector<string> links;

    // Compile regular expresion, this is very simple and does
    // not consider non-absolute links usually used in html files.
    ret = regcomp(&regex, "href=[\"\'].[^\'\"]*[\"\']", 0);
    if (ret) {fprintf(stderr, "Could not compile regex\n"); exit(1);}

    // While matches found...insert
    while (1) {
        match = regexec (&regex, data, 1, matches, 0);
        if (match == REG_NOMATCH) {
            break;
        }

        assert(matches[0].rm_so != -1);

        const char * start = matches[0].rm_so + data;
        size_t len = matches[0].rm_eo - matches[0].rm_so;

        string s = string(start, len);
        links.push_back(s);
        //DEBUG("\nAdding to string vector: %s, of length %lu\n", s.c_str(), len);

        data += matches[0].rm_eo; // move to the end of the last match
    }
regfree(&regex);
    return links;
}

// While queue of files not empty: Parse files (map) into a
//      thread local dictionary while doing local reduction.
// If queue becomes empty: Goto reduction phase. Add from 
//      thread local dictionary to global dictionary.
void*
mapReduce (void*)
{
    string file_path;
    int i, fd;
    vector<string> links;
    unordered_map<string, set<string> > local_map;
    unordered_map<string, set<string> >::iterator it;

    // map phase with thread-local reduction
    while (true) {
        // Grab a file path from the shared global queue for parsing
        pthread_mutex_lock (&df_mutex);
        file_path = getDataFilePath();
        pthread_mutex_unlock (&df_mutex);

        // Check for end of queue
        if (file_path.empty()) break;
        DEBUG("\nT%d grabs file: %s\n", (int)pthread_self(), file_path.c_str());

        // Load file into memory -- faster
        fd = open(file_path.c_str(), O_RDONLY);
        struct stat sb;
        fstat (fd, &sb);
        if (sb.st_size == 0) continue; // There are files with size 0, mmap crashes
        void* data = mmap (NULL, sb.st_size+4096, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) printf("\nMMAP ERROR: %s\n", sys_errlist[errno]);
        close(fd);

        // Create vector of links in the file
        links = getLinks((const char*)data);

        // Add the links to the local map (local reduction)
        //printf("\nT%d before crashing loop, links size: %d\n", pthread_self(), (int)links.size());
        for (i=0; i < (int)links.size(); i++) {
            //printf("\nT%d inside crashing loop\n", pthread_self());
            string tmp = links[i];
            //printf("\nT%d after links of i in loop\n", pthread_self());

            it = local_map.find(links[i]);
            //if (it != local_map.end()) {
                //it->second.insert(file_path);
            //} else{
                //set<string> file_set;
                //file_set.insert(file_path);
                //local_map.insert(pair<string, set<string> >(links[i],file_set));
            //}
        }
        munmap(data, sb.st_size+1);
    }

    DEBUG("\nNumer of links: %d\n", (int)local_map.size());

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
        fprintf(stderr, "Error: something went wrong reading the files\n");
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
