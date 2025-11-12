#!/usr/bin/env bash

TOTAL_PHYSICAL_CORES=4
TOTAL_LOGICAL_CORES=4

# Setup
samples=10000		# 10 seconds
outer=5			# 30 reps
num_thread=$TOTAL_LOGICAL_CORES
date=`date +"%m%d-%H%M"`

# Alert
echo "This script will take about $(((10)*$outer*3/60+10)) minutes. Reduce 'outer' if you want a shorter run."

# Warmup
stress-ng -q --cpu $TOTAL_LOGICAL_CORES -t 10m

# Run
sudo rm -rf out
mkdir out
sudo rm -rf input.txt

for selector in 16 32 48; do
	echo $selector >> input.txt
done

sudo ./bin/driver-steady ${num_thread} ${samples} ${outer}
cp -r out data/out-steady-${date}
