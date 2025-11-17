import matplotlib.pyplot as plt
import argparse
import os
import seaborn as sns
import glob
import numpy as np

def parse_file(fn):
    energy = []
    freq = []
    time = []
    with open(fn) as f:
        for line in f:
            c = line.strip().split()
            if len(c) >= 3:
                energy.append(float(c[0]))
                freq.append(float(c[1]) / 1e6)   # Hz → MHz
                time.append(float(c[2]))         # seconds
                
    return np.array(energy), np.array(freq), np.array(time)


def main():

    # Prepare output directory
    try:
        os.makedirs('plot')
    except:
        pass

    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('folder')
    args = parser.parse_args()
    in_dir = args.folder

    # Read data
    #    ./out/all_%s_%04d.out
    all_files = sorted(glob.glob(in_dir + "/all_*"), reverse=True)
    
    
    energys = {}
    freqs = {}
    times = {}
    xlabels = []
    
    for f in all_files:
        label = "_".join(os.path.splitext(os.path.basename(f))[0].split("_")[1:-1])
        rept_idx = os.path.splitext(os.path.basename(f))[0].split("_")[-1]
        energy, freq, time = parse_file(f)
        
        label_int = int(label)
        xlabels.append(label_int)
        # Create list per label if key doesn't exist yet
        if label_int not in energys:
            energys[label_int] = []
            freqs[label_int] = []
            times[label_int] = []
            
         # Append values for this file
        energys[label_int].append(energy)
        freqs[label_int].append(freq)
        times[label_int].append(time)
            
            
    x = list(set(xlabels))
    y = [np.mean(freqs[k]) for k in x]
    

    plt.figure(figsize=(3, 2))
    plt.scatter(x, y, s=3)
    plt.xlabel('Hamming weight')
    plt.ylabel('Frequency (GHz)')
    # plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig("./plot/" + "freq.pdf", dpi=300)


    x = list(set(xlabels))
    y = [np.mean(energys[k]) for k in x]
    
    
    
    plt.figure(figsize=(3, 2))
    plt.scatter(x, y, s=3)
    plt.xlabel('Hamming weight')
    plt.ylabel('Power (W)')
    # plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig("./plot/" + "energy.pdf", dpi=300)


if __name__ == "__main__":
    main()
    
    