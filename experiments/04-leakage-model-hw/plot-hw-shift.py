import os
import glob
import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import copy

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
        
    data_ones = {}
    data_zeros = {}

    for (A, B, C, D), freq_list in freqs.items():

        mean_f = float(np.mean(np.concatenate(freq_list)))
        std_f  = float(np.std(np.concatenate(freq_list)))

        # ones: shift_first = C (already positive)
        if C >= 0:
            data_ones.setdefault(A, {})
            data_ones[A][C] = (mean_f, std_f)

        # zeros: shift_second = D (negative shift), convert: -D
        if D < 0:
            shift_zero = -D
            data_zeros.setdefault(A, {})
            data_zeros[A][shift_zero] = (mean_f, std_f)


    # ---------------------------------------------------------
    # Plot ones
    # ---------------------------------------------------------
    plt.figure(figsize=(3, 2))

    for hamming in sorted(data_ones.keys()):
        x, y = [], []
        for shift in sorted(data_ones[hamming].keys()):
            x.append(shift)
            y.append(data_ones[hamming][shift][0] / 1000.0)   # MHz → GHz
        plt.scatter(x, y, s=3, label=f"{hamming} ones")

    plt.xlabel("Shift offset")
    plt.ylabel("Frequency (GHz)")
    plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig(f"./plot/shift_ones.pdf", dpi=300)
    plt.show()
    plt.clf()


    # ---------------------------------------------------------
    # Plot zeros
    # ---------------------------------------------------------
    plt.figure(figsize=(3, 2))

    for hamming in sorted(data_zeros.keys()):
        x, y = [], []
        for shift in sorted(data_zeros[hamming].keys()):
            x.append(shift)
            y.append(data_zeros[hamming][shift][0] / 1000.0)   # MHz → GHz
        plt.scatter(x, y, s=3, label=f"{hamming} zeros")

    plt.xlabel("Shift offset")
    plt.ylabel("Frequency (GHz)")
    plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig(f"./plot/shift_zeros.pdf", dpi=300)
    plt.show()
    plt.clf()
    
    # ---------------------------------------------------------
    # Build data_ones and data_zeros from raw energy samples
    # ---------------------------------------------------------
    data_ones = {}
    data_zeros = {}

    for (A, B, C, D), energy_list in energys.items():

        mean_e = float(np.mean(np.concatenate(energy_list)))
        std_e  = float(np.std(np.concatenate(energy_list)))

        # ones: shift_first = C
        if C >= 0:
            data_ones.setdefault(A, {})
            data_ones[A][C] = (mean_e, std_e)

        # zeros: shift_second = D
        if D < 0:
            shift_zero = -D
            data_zeros.setdefault(A, {})
            data_zeros[A][shift_zero] = (mean_e, std_e)


        # ---------------------------------------------------------
        # Plot ones
        # ---------------------------------------------------------
        plt.figure(figsize=(3, 2))

        for hamming in sorted(data_ones.keys()):
            x, y = [], []
            for shift in sorted(data_ones[hamming].keys()):
                x.append(shift)
                y.append(data_ones[hamming][shift][0] / 0.001)   # J per 1ms → W
            plt.scatter(x, y, s=3, label=f"{hamming} ones")

        plt.xlabel("Shift offset")
        plt.ylabel("Power (W)")
        plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.savefig(f"./plot/shiftpow_ones.pdf", dpi=300)
        plt.show()
        plt.clf()


        # ---------------------------------------------------------
        # Plot zeros
        # ---------------------------------------------------------
        plt.figure(figsize=(3, 2))

        for hamming in sorted(data_zeros.keys()):
            x, y = [], []
            for shift in sorted(data_zeros[hamming].keys()):
                x.append(shift)
                y.append(data_zeros[hamming][shift][0] / 0.001)
            plt.scatter(x, y, s=3, label=f"{hamming} zeros")

        plt.xlabel("Shift offset")
        plt.ylabel("Power (W)")
        plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.savefig(f"./plot/shiftpow_zeros.pdf", dpi=300)
        plt.show()
        plt.clf()

if __name__ == "__main__":
    main()
