import argparse
import glob
import os
from matplotlib.ticker import FormatStrFormatter
import matplotlib.ticker as ticker
import matplotlib.pyplot as plt
import numpy as np
from distutils.dir_util import remove_tree


# -------------------------------------------------------------------------------------------------------------------
# Parsing Functions
# -------------------------------------------------------------------------------------------------------------------

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

# -------------------------------------------------------------------------------------------------------------------

def main():

    # Prepare output directory
    try:
        remove_tree('plot')
    except:
        pass
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
    energy_files = sorted(glob.glob(in_dir + "/energy_*"), reverse=True)
    freq_files = sorted(glob.glob(in_dir + "/freq_*"), reverse=True)
    

    ylowf, ymaxf = 0, 0
    ylowe, ymaxe = 0, 0

    for f, g in zip(energy_files, freq_files):
        # Parse trace
        raw_energy_trace, raw_etime_trace = parse_energy(f)
        raw_freq_trace, raw_ftime_trace = parse_freq(g)

        # Exclude overflows in energy counters and convert to power
        power_trace = []
        prev_sample = 0
        thres = np.percentile(raw_energy_trace, 99)     # Exclude outliers
        for i, x in enumerate(raw_energy_trace[10:]):   # Exclude first 10 samples
            if x > 0 and x <= thres:
                power_sample = x  		# 0.005 is to convert to power since we sample energy every 5ms
                power_trace.append(power_sample)
                prev_sample = power_sample
            else:
                power_trace.append(prev_sample)

        # Returns in KHz
        freq_trace = (raw_freq_trace[10:])  	
        freq_time = (raw_ftime_trace[10:])

        # Generate figure for both freq and power
        plt.figure(figsize=(3.2, 2.6))

        # Plot frequency
        plt.subplot(2, 1, 1)
        color = "tab:blue"
        plt.ylabel('Frequency (GHz)')
        plt.gca().yaxis.set_major_locator(ticker.MultipleLocator(100))
        plt.plot(freq_time, freq_trace, linewidth=0.2, color=color, label="Frequency (MHz)")
        plt.legend(fontsize=7, loc='upper right')
        # plt.grid(axis='y')
        plt.tick_params(
            axis='x',          # changes apply to the x-axis
            which='both',      # both major and minor ticks are affected
            bottom=True,      # ticks along the bottom edge are off
            top=False,         # ticks along the top edge are off
            labelbottom=False)  # labels along the bottom edge are off

            
        for y in [1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400]:
            plt.axhline(y=y, color='gray', linestyle=':', linewidth=1)
            
        # Get yrange for the first plot
        if ylowf == 0:
            ylowf, ymaxf = plt.gca().get_ylim()
        else:
            plt.ylim(ylowf, ymaxf)

        # Plot power
        plt.subplot(2, 1, 2)
        color = "tab:red"
        plt.ylabel('Power (W)')
        plt.gca().yaxis.set_major_locator(ticker.MultipleLocator(0.5))
        plt.plot(raw_etime_trace[10:], power_trace, linewidth=0.2, color=color, label="Power (W)", linestyle="--")
        plt.legend(fontsize=7, loc='upper right')

        # Get yrange for the first plot
        if ylowe == 0:
            ylowe, ymaxe = plt.gca().get_ylim()
        else:
            plt.ylim(ylowe, ymaxe)
            
        # Shared x axis
        plt.gca().xaxis.set_major_formatter(FormatStrFormatter('%.0f'))  # No decimal places
        plt.xlabel('Time (s)')

        plt.show()
        # Save file
        plt.tight_layout(pad=0, w_pad=0.5, h_pad=.5)
        plt.savefig(os.path.join(
        "plot",
        "stress_%s.pdf" % "_".join(os.path.splitext(os.path.basename(f))[0].split("_")[1:])
        ))
        plt.close()
        plt.clf()
        
        rounded_data = np.round(freq_trace, -2)
        bin_centers = np.array([1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400])
        # Option 2: Use explicit bin edges between the centers
        # Create edges halfway between centers
        bin_edges = np.concatenate(([bin_centers[0] - 50],
                                    (bin_centers[:-1] + bin_centers[1:]) / 2,
                                    [bin_centers[-1] + 50]))

        plt.figure(figsize=(6, 4))
        plt.hist(rounded_data, bins=bin_edges, edgecolor='black', alpha=0.7)
        plt.xticks(bin_centers)
        plt.xlabel("Pstate MHz")
        plt.ylabel("Count")
        plt.title("Pstate Occurences")
        plt.show()

if __name__ == "__main__":
    main()

