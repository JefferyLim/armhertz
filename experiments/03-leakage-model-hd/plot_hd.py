import matplotlib.pyplot as plt
import argparse
import os

def parse_energy(fn):
    energy = []
    time = []
    with open(fn) as f:
        for line in f:
            a = line.strip().split()
            if len(a) >= 2:
                energy.append(float(a[0]))
                time.append(float(a[1]))         
            
    return np.array(energy), np.array(time)

def parse_freq(fn):
    freq = []
    time = []
    with open(fn) as f:
        for line in f:
            c = line.strip().split()
            if len(c) >= 2:
                freq.append(float(c[0]) / 1e6)   # Hz → MHz
                time.append(float(c[1]))         # seconds
    return np.array(freq), np.array(time)


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
                data[int(selector)] = (int(mean), int(std))

        x, y = [], []
        for key, val in data.items():
            x.append(key)
            y.append(val[0] / 1000000)  # / 1000000 is to convert to GHz

        plt.figure(figsize=(3, 2))
        plt.scatter(x, y, s=3)
        plt.xlabel('COUNT')
        plt.ylabel('Frequency (GHz)')
        # plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.savefig("./plot/" + args.figname + ".pdf", dpi=300)

    if (energy_file):
        data = {}
        with open(energy_file + "/energy.txt") as f:
            for line in f:
                selector, mean, std = line.strip().split()
                data[int(selector)] = (float(mean), float(std))

        x, y = [], []
        for key, val in data.items():
            x.append(key)
            y.append(val[0] / 0.001)		# 0.001 is to convert to power since we sample energy every 1ms

        plt.figure(figsize=(3, 2))
        plt.scatter(x, y, s=3)
        plt.xlabel('COUNT')
        plt.ylabel('Power (W)')
        # plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.savefig("./plot/" + args.figname + ".pdf", dpi=300)


if __name__ == "__main__":
    main()