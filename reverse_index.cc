
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "timer.h"
#include <iostream>
#include <fstream>
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
#include <sstream>
using namespace std;

// Global parameters
int nthreads;
char* data_dir;
int debug;
int silent;

// Debug prints to be used inside parallel section
// You may want to redirect stdout when enebled
#define DEBUG(...) do { \
    if (debug) \
        printf(__VA_ARGS__); \
} while (0)
#define PRINTF(...) do { \
    if (!silent) \
        printf(__VA_ARGS__); \
} while (0)

struct synch_set {
    pthread_mutex_t set_mutex;
    set<string> files;

    synch_set (set<string> A) : files(A) {
        set_mutex = PTHREAD_MUTEX_INITIALIZER;
    }
};

// Global queue that holds paths to data files.
queue<string> data_files;
// Global map where the final reduction is done.
unordered_map<string, synch_set> global_map;

// Defining mutexes
pthread_mutex_t df_mutex;
pthread_mutex_t ht_mutex;

// Prints usage information
void
usage (const char* argv0)
{
    const char* help =
        "Usage: %s [switches] -i data_dir\n"
        "       -i data_dir:    data directory that contains files to be parsed\n"
        "       -t nthreads:    number of threads\n"
        "       -d:             enables debug facilities, slow performance\n"
        "       -s:             silent output, for measuring time\n";
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

        const char* start = matches[0].rm_so + data;
        size_t len = matches[0].rm_eo - matches[0].rm_so;

        string s = string(start, len);
        links.push_back(s);
        DEBUG("\nAdding to string vector: %s, of length %lu", s.c_str(), len);

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
    string file_path, data;
    int i, num_files;
    vector<string> links;
    unordered_map<string, set<string> > local_map;
    unordered_map<string, set<string> >::iterator it_local;
    stringstream buffer;
    unordered_map<string, synch_set>::iterator it_global;

    num_files = 0;

    // map phase with thread-local reduction
    while (true) {
        // Grab a file path from the shared queue for parsing
        pthread_mutex_lock (&df_mutex);
        file_path = getDataFilePath();
        pthread_mutex_unlock (&df_mutex);

        // Check for end of queue
        if (file_path.empty()) break;
        else num_files++;
        DEBUG("\nT%d reads file: %s", (int)pthread_self(), file_path.c_str());

        // Load file into a string with ifstream 
        // As fast as mmap for small files
        ifstream ifs(file_path.c_str());
        buffer << ifs.rdbuf();
        data = buffer.str();
        buffer.str(string());
        buffer.clear();

        // Create vector of links in the file
        links = getLinks(data.c_str());

        // Add links to the thread local map (local reduction)
        for (i=0; i < (int)links.size(); i++) {
            it_local = local_map.find(links[i]);
            if (it_local != local_map.end()) {
                it_local->second.insert(file_path);
            } else{
                set<string> file_set;
                file_set.insert(file_path);
                local_map.insert(pair<string, set<string> >(links[i],file_set));
            }
        }
    }

    PRINTF("\nThread%d parsed %d files and found %d distinct links", (int)pthread_self(), num_files, (int)local_map.size());

    // reduce: from local map to global map
    for (it_local = local_map.begin(); it_local != local_map.end(); it_local++) {

        // For each entry in local_map, agregate it to global_map
        it_global = global_map.find(it_local->first);
        if (it_global != global_map.end()){

            // If entry already in global_map, grab bucket lock and update it
            pthread_mutex_lock (&(it_global->second.set_mutex));
            it_global->second.files.insert(it_local->second.begin(), it_local->second.end());
            pthread_mutex_unlock (&(it_global->second.set_mutex));

        } else {

            // If not in global_map, insert new entry.
            // Grab global lock and check again if present to avoid race.
            pthread_mutex_lock (&ht_mutex);
            it_global = global_map.find(it_local->first);
            if (it_global != global_map.end()){

                // Its present now, someone was faster...
                pthread_mutex_lock (&(it_global->second.set_mutex));
                it_global->second.files.insert(it_local->second.begin(), it_local->second.end());
                pthread_mutex_unlock (&(it_global->second.set_mutex));

            } else {

                // Still not in global_map, insert.
                synch_set ss(it_local->second);
                global_map.insert(pair<string, synch_set>(it_local->first, ss));

            }
            pthread_mutex_unlock (&ht_mutex);
        }
    }

    pthread_exit((void*)0);
}

int
main (int argc, char** argv)
{

    // At least one argument (data directory)
    if (argc == 1) usage((char*)argv[0]);

    // Default parameters
    nthreads = 1;
    debug = 0;
    silent = 0;

    int opt;
    while ((opt = getopt(argc,(char**)argv,"i:t:sd")) != EOF) {
        switch (opt) {
            case 'i': data_dir = optarg;
                      break;
            case 't': nthreads = atoi(optarg);
                      break;
            case 's': silent = 1;
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
    PRINTF("\nRunning Reverse Indexing...\n");
    PRINTF("\nData directory:    %s", data_dir);
    PRINTF("\nNumber of threads: %d", nthreads);
    PRINTF("\nDebug flag:        %d", debug);

    // Populates queue of files to be parsed
    if (ftw(data_dir, populateQueue, 16) != 0) {
        fprintf(stderr, "Error: something went wrong reading the files\n");
    } else {
        PRINTF("\n\nFound %lu files to parse\n", data_files.size());
    }

    // Create thread id's pool and initialize mutexes
    pthread_t global_threads[nthreads];
    pthread_mutex_init(&df_mutex, NULL);
    pthread_mutex_init(&ht_mutex, NULL);

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
    pthread_mutex_destroy(&ht_mutex);

    // Global map size
    PRINTF("\n\nTotal number of distinct links found after reverse indexing: %lu", global_map.size());

    // Calculate the time that took the parallel section
    double parallelTime = TIMER_DIFF_SECONDS(start, stop);
    PRINTF("\n\nTime taken for do reverse indexing is %9.6f sec.\n", parallelTime);

    return 0;
}
