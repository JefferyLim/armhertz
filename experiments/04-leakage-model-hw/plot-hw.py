import os
import glob
import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import copy

def bitfield(n):
    bit_list = [int(digit) for digit in bin(n)[2:]] # [2:] to chop off the "0b" part
    if len(bit_list) < 8:
        for _ in range(8 - len(bit_list)):
            bit_list.insert(0, 0)
    return bit_list

def parse_file(fn):

    energy = []
    freq = []
    time = []
    with open(fn) as f:
        for line in f:
            c = line.strip().split()
            if len(c) >= 3:
                energy.append(float(c[0]))
                # the freq in your parse_file was converted from Hz -> MHz already
                freq.append(float(c[1]) / 1e6)   # Hz → MHz
                time.append(float(c[2]))         # seconds

    return np.array(energy), np.array(freq), np.array(time)

def main():

    # Prepare output directory
    try:
        os.makedirs('plot')
    except:
        pass
        
    parser = argparse.ArgumentParser()
    parser.add_argument('folder')
    args = parser.parse_args()

    in_dir = args.folder

    # Find files
    all_files = sorted(glob.glob(os.path.join(in_dir, "all_*")), reverse=True)
   


    freqs = {}
    energys = {}
    times = {}
    meta = {}       # optional: stores parsed (A,B,C,D)
    
    for f in all_files:
        base = os.path.splitext(os.path.basename(f))[0]
        _, A, B, C, D, rept = base.split("_")
        
        # parse file data
        energy, freq, time = parse_file(f)

        A = int(A)
        B = int(B)
        C = int(C)
        D = int(D)
        rept = int(rept)

        label = (A, B, C, D)     # unique 4-field key
        
        
        # Allocate lists if needed
        if label not in energys:
            energys[label] = []
            freqs[label] = []
            times[label] = []
            meta[label] = (A, B, C, D)

        # Append raw unprocessed arrays
        energys[label].append(energy)
        freqs[label].append(freq)
        times[label].append(time)
        
        
        
if __name__ == "__main__":
    main()