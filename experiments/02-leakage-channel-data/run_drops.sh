#!/usr/bin/env bash

name={$1:-test}

TOTAL_PHYSICAL_CORES=4
TOTAL_LOGICAL_CORES=4

# Setup
samples=20000		# 40 seconds
outer=20			# 105 reps
num_thread=$TOTAL_LOGICAL_CORES
date=`date +"%m%d-%H%M"`

# Alert
echo "This script will take about $(((30+40)*$outer*3/60/60)) hours. Reduce 'outer' if you want a shorter run."

# Run
sudo rm -rf out
mkdir out
sudo rm -rf input.txt

for selector in 16 32 48; do
	echo $selector >> input.txt
done

sudo ./bin/driver ${num_thread} ${samples} ${outer}
cp -r out data/${name}/out-drops-${date}
