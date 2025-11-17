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
        
    # --------------------------------------------------------------------
    # Part 1: Reconstruct the data structure used by the old script
    # --------------------------------------------------------------------
    #
    # data_freq[hw][shift] = (mean, std)
    # data_energy[hw][shift] = (mean, std)
    #
    # Here:
    #   hw = A = hamming_first
    #   shift = C = shift_first
    #

    data_freq = {}
    data_energy = {}

    for (A, B, C, D), freq_list in freqs.items():

        mean_f = np.mean(freq_list)
        std_f = np.std(freq_list)
        mean_e = np.mean(energys[(A, B, C, D)])
        std_e = np.std(energys[(A, B, C, D)])
        

        if A not in data_freq:
            data_freq[A] = {}
            data_energy[A] = {}

        data_freq[A][C] = (mean_f, std_f)
        data_energy[A][C] = (mean_e, std_e)

    #
    # --------------------------------------------------------------------
    # Part 2: Byte independence plot (frequency)
    # --------------------------------------------------------------------
    #

    if 100 in data_freq:

        independence = {}
        for x in range(256):

            bits = bitfield(x)

            for byte in range(8):
                seven = bits.copy()
                seven.pop(byte)
                seven = tuple(seven)

                if byte not in independence:
                    independence[byte] = {}
                if seven not in independence[byte]:
                    independence[byte][seven] = {}

                if bits[byte] == 0:
                    independence[byte][seven][0] = data_freq[x + 100][0]
                else:
                    independence[byte][seven][1] = data_freq[x + 100][0]

        byte_mean = {}
        byte_std = {}

        for byte in independence:
            deltas = []
            for seven_bytes in independence[byte]:
                f0 = independence[byte][seven_bytes][0][0]
                f1 = independence[byte][seven_bytes][1][0]
                deltas.append(f1 - f0)

            deltas.sort()
            filtered = deltas[10:-10]

            byte_mean[byte] = np.mean(filtered)
            byte_std[byte] = np.std(filtered)

        key = []
        mean = []
        std = []

        for b in byte_mean:
            key.append(7 - b)               # flip for MSB → LSB ordering
            mean.append(byte_mean[b] / 1e3) # MHz to GHz
            std.append(byte_std[b] / 1e3)

        plt.figure(figsize=(3, 2))
        plt.errorbar(key, mean, yerr=std, fmt='o', ms=3)
        plt.xlabel("Byte index")
        plt.ylabel("Δ Frequency (GHz)")
        plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(1))
        plt.tight_layout(pad=0.1)
        plt.savefig(f"./plot/freq-independence.pdf", dpi=300)
        plt.show()
        plt.clf()

    #
    # --------------------------------------------------------------------
    # Part 3: Hamming weight sweep (frequency)
    # --------------------------------------------------------------------
    #

    plt.figure(figsize=(3, 2))

    # From LSB (shift = 0)
    xvals = []
    yvals = []
    for hw in range(65):
        if hw in data_freq and 0 in data_freq[hw]:
            xvals.append(hw)
            yvals.append(data_freq[hw][0][0] / 1e3)  # MHz to GHz
    plt.scatter(xvals, yvals, s=3, label="From LSB")

    # From MSB (shift = 64 - hw)
    xvals = []
    yvals = []
    for hw in range(65):
        shift = 64 - hw
        if hw in data_freq and shift in data_freq[hw]:
            xvals.append(hw)
            yvals.append(data_freq[hw][shift][0] / 1e3)
    plt.scatter(xvals, yvals, s=3, label="From MSB")

    plt.xlabel("Hamming weight")
    plt.ylabel("Frequency (GHz)")
    plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig(f"./plot/freq.pdf", dpi=300)
    plt.show()
    plt.clf()
    
    print("Plots generated.")

if __name__ == "__main__":
    main()