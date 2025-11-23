#!/usr/bin/env bash

NAME=${1:-test}
echo $NAME

mkdir -p data/${NAME}
TOTAL_PHYSICAL_CORES=4
TOTAL_LOGICAL_CORES=4

# Setup
samples=5000	    # 30 seconds (1 ms + 14 ms )
outer=10			# 30 reps
num_thread=$TOTAL_LOGICAL_CORES
date=`date +"%m%d-%H%M"`

# Alert
echo "This script will take about $(((30)*$outer*3/60+10)) minutes. Reduce 'outer' if you want a shorter run."

# Run
sudo rm -rf out
mkdir out
sudo rm -rf input.txt

NUM_PLAINTEXTS=64 
for ((i=0; i<NUM_PLAINTEXTS; i++)); do
    # Generate 16 random bytes → print as 32 hex chars
    head -c 16 /dev/urandom | xxd -p | tr -d '\n' >> input.txt
    echo >> input.txt
done

sudo ./bin/driver_aes_leak ${num_thread} ${samples} ${outer}
cp -r out data/${NAME}/leak-${date}
