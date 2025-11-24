#!/usr/bin/env bash

NAME=${1:-test}
echo $NAME

mkdir -p data/${NAME}
TOTAL_PHYSICAL_CORES=4
TOTAL_LOGICAL_CORES=4

# Setup
samples=5000	    # 30 seconds (1 ms + 14 ms )
outer=2			# 30 reps
num_thread=$TOTAL_LOGICAL_CORES
date=`date +"%m%d-%H%M"`

# Alert
echo "This script will take about $(((30)*$outer*3/60+10)) minutes. Reduce 'outer' if you want a shorter run."

# Warmup
#stress-ng -q --cpu $TOTAL_LOGICAL_CORES -t 10m

# Run
sudo rm -rf out
mkdir out
sudo rm -rf input.txt

for selector in `seq 0 255`; do
	echo $selector >> input.txt
done

for BYTEBLOCK in `seq 0 15`; do

    sudo rm -rf out
    mkdir out
    
    # Rebuild only the driver_aes target
    make clean > /dev/null
    make driver_aes AES_DEFINES="-DBYTEBLOCK=${BYTEBLOCK}"

    echo ">>> Running driver_aes BYTEBLOCK=${BYTEBLOCK}"
    sudo ./bin/driver_aes ${num_thread} ${samples} ${outer}

    # Save results
    outdir="data/${NAME}/BYTEBLOCK_${BYTEBLOCK}_${date}"
    mkdir -p "${outdir}"
    cp -r out "${outdir}"

done