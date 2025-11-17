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
        
    data = {
        "A": {},
        "B": {},
        "C": {},
        "D": {}
    }

    # Convert your dictionary of raw arrays into mean/std format
    for (A_, B_, C_, D_), freq_list in freqs.items():

        # Identify categories based on (hw_first=A_, shift_first=C_)
        if A_ == 16 and C_ == 0:
            key = "A"
        elif A_ == 16 and C_ == 48:
            key = "B"
        elif A_ == 32 and C_ == 0:
            key = "C"
        elif A_ == 32 and C_ == 32:
            key = "D"
        else:
            continue    # ignore non-matching cases

        # Combine repetitions → one long sample vector
        combined = np.concatenate(freq_list)

        mean = np.mean(combined)
        std = np.std(combined)

        # Store under: data[key][hw_second = B_] = (mean, std)
        data[key][B_] = (mean, std)


    #############################
    # FREQUENCY PLOT (GHz)
    #############################

    plt.figure(figsize=(3, 2))

    for letter in data:
        x, y = [], []
        for hw_second in sorted(data[letter].keys()):
            mean_val = data[letter][hw_second][0] / 1000.0  # MHz → GHz
            x.append(hw_second)
            y.append(mean_val)
        plt.scatter(x, y, label=letter, s=3)

    plt.xlabel('HW of SECOND')
    plt.ylabel('Frequency (GHz)')
    plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig("./plot/freq_ABCD.pdf", dpi=300)
    plt.show()
    plt.clf()



    ###############################################
    # ENERGY-TO-POWER VERSION (raw → W)
    ###############################################

    data = {
        "A": {},
        "B": {},
        "C": {},
        "D": {}
    }

    for (A_, B_, C_, D_), energy_list in energys.items():

        if A_ == 16 and C_ == 0:
            key = "A"
        elif A_ == 16 and C_ == 48:
            key = "B"
        elif A_ == 32 and C_ == 0:
            key = "C"
        elif A_ == 32 and C_ == 32:
            key = "D"
        else:
            continue

        combined = np.concatenate(energy_list)

        power_samples = combined

        mean = np.mean(power_samples)
        std = np.std(power_samples)

        data[key][B_] = (mean, std)


    #############################
    # POWER PLOT (W)
    #############################

    plt.figure(figsize=(3, 2))

    for letter in data:
        x, y = [], []
        for hw_second in sorted(data[letter].keys()):
            mean_val = data[letter][hw_second][0]
            x.append(hw_second)
            y.append(mean_val)
        plt.scatter(x, y, label=letter, s=3)

    plt.xlabel('HW of SECOND')
    plt.ylabel('Power (W)')
    plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig("./plot/power_ABCD.pdf", dpi=300)
    plt.show()
    plt.clf()
        
if __name__ == "__main__":
    main()
