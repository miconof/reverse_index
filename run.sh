#!/bin/bash

USAGE="
Usage:\n
$0 data_dir\n
\n
data_dir: directory with data files\n
\n
Output in results.csv\n
"
if [ $# -ne 1 ]; then
    echo -e $USAGE
    exit 1
fi

DATA=$1
rm results.csv

for nthreads in {1..96}
do
    for ii in {1..5}
    do
        echo -n "$nthreads " >> results.csv
        echo "/usr/bin/time -a -o results.csv -f"%e" ./rindex -i $DATA -s -t $nthreads"
        /usr/bin/time -a -o results.csv -f"%e" ./rindex -i $DATA -s -t $nthreads
    done
    echo "" >> results.csv
done
