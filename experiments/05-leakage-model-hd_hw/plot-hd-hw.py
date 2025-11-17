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



def main():

    # Prepare output directory
    try:
        os.makedirs('plot')
    except:
        pass

    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('--freq')
    parser.add_argument('--energy')
    parser.add_argument('figname')
    args = parser.parse_args()
    freq_file = args.freq
    energy_file = args.energy

    if (freq_file):
        data = {}
        with open(freq_file + "/frequency.txt") as f:
            for line in f:
                selector, mean, std = line.strip().split()
                selectors = selector.split("_")
                hw_first, hw_second, shift_first = int(selectors[0]), int(selectors[1]), int(selectors[2])

                if hw_first == 16 and shift_first == 0:
                    data.setdefault("A", {})
                    data["A"][hw_second] = (int(mean), int(std))
                if((hw_first == 16) and (shift_first == 48)):
                    data.setdefault("B", {})
                    data["B"][hw_second] = (int(mean), int(std))
                if((hw_first == 32) and (shift_first == 0)):
                    data.setdefault("C", {})
                    data["C"][hw_second] = (int(mean), int(std))
                if((hw_first == 32) and (shift_first == 32)):
                    data.setdefault("D", {})
                    data["D"][hw_second] = (int(mean), int(std))

        plt.figure(figsize=(3, 2))

        for letter in data:
            x, y = [], []
            for hamming in data[letter]:
                x.append(hamming)
                y.append(data[letter][hamming][0] / 1000000)  # / 1000000 is to convert to GHz
            plt.scatter(x, y, label=letter, s=3)

        plt.xlabel('HW of SECOND')
        plt.ylabel('Frequency (GHz)')
        plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.savefig("./plot/" + args.figname + ".pdf", dpi=300)
        plt.clf()

    if (energy_file):
        data = {}
        with open(energy_file + "/energy.txt") as f:
            for line in f:
                selector, mean, std = line.strip().split()
                selectors = selector.split("_")
                hw_first, hw_second, shift_first = int(selectors[0]), int(selectors[1]), int(selectors[2])

                if hw_first == 16 and shift_first == 0:
                    data.setdefault("A", {})
                    data["A"][hw_second] = (float(mean), float(std))
                if((hw_first == 16) and (shift_first == 48)):
                    data.setdefault("B", {})
                    data["B"][hw_second] = (float(mean), float(std))
                if((hw_first == 32) and (shift_first == 0)):
                    data.setdefault("C", {})
                    data["C"][hw_second] = (float(mean), float(std))
                if((hw_first == 32) and (shift_first == 32)):
                    data.setdefault("D", {})
                    data["D"][hw_second] = (float(mean), float(std))

        plt.figure(figsize=(3, 2))

        for letter in data:
            x, y = [], []
            for hamming in data[letter]:
                x.append(hamming)
                y.append(data[letter][hamming][0] / 0.001)		# 0.001 is to convert to power since we sample energy every 1ms
            plt.scatter(x, y, label=letter, s=3)

        plt.xlabel('HW of SECOND')
        plt.ylabel('Power (W)')
        plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.savefig("./plot/" + args.figname + ".pdf", dpi=300)
        plt.clf()


if __name__ == "__main__":
    main()
