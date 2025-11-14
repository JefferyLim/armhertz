import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

import argparse
import seaborn as sns
import glob
import os
import matplotlib.ticker as ticker
import matplotlib.pyplot as plt
import numpy as np
from multiprocessing import Process
import multiprocessing


def parse_file(fn):
    freq = []
    time = []
    with open(fn) as f:
        for line in f:
            c = line.strip().split()
            if len(c) >= 2:
                freq.append((float(c[0]) / 1e6))   # Hz → MHz
                time.append(float(c[1]))         # seconds

    
    cleaned = np.array(freq)

    last_valid = 1450

    for i, val in enumerate(cleaned):
        if 1450 <= val <= 2450:
            last_valid = val           # update last valid
        else:
            cleaned[i] = last_valid    # forward-fill outlier

    return np.array(cleaned), np.array(time)

# Density Plot and Histogram
# https://towardsdatascience.com/histograms-and-density-plots-in-python-f6bda88f5ac0
def plot_pdf(datas, labels):
    for data, label in zip(datas, labels):
        sns.distplot(data, label=label, hist=True, kde=True, bins=25)

def detect_drop(trace):
    best_x = 0
    max_diff = 0
    
    indices = np.where(trace < 1525.0)
    if indices[0].size > 0:
        drop_1500 = indices[0][0]
    
    indices = np.where(trace < 2325.0)
    if indices[0].size > 0:
        drop_2300 = indices[0][0]

    indices = np.where(trace < 2225.0)
    if indices[0].size > 0:
        drop_2200 = indices[0][0]   


    return drop_1500, drop_2200, drop_2300


def f_pool(secret_bit, i, trace, drop_1500s, drop_2200s, drop_2300s, time):
    trace = trace[:]
    freq_x_axis = time[:]

    # Save index of the drop
    drop_1500,drop_2200,drop_2300 = detect_drop(trace)
    
    print(drop_1500)
    print(drop_2200)
    print(drop_2300)

    # Exclude weird runs (and plot for debug)
    excluded = 0

    # Exclude runs that had a weird post-drop average
    # FIXME: Change this threshold for your CPU
    # threshold = 4450000
    # if post_drop_avg > threshold:
        # print("Excluded", secret_bit, i, "because frequency was higher than", threshold, "after the drop. You may need to change the threshold variable")
        # excluded = 1
        
    # FIXME: Change to True to plot drop in traces
    if (True):
        plt.figure(figsize=(20, 3.8))
        plt.plot(freq_x_axis, trace)
        plt.axvline(x=freq_x_axis[drop_1500], color='red')
        plt.axvline(x=freq_x_axis[drop_2200], color='green')
        plt.axvline(x=freq_x_axis[drop_2300], color='orange')
        plt.savefig("./plot/freq_%s_%d.png" % (secret_bit, i))
        plt.clf()
        plt.close()

    # Count all the other runs
    drop_1500s.append(drop_1500)
    drop_2200s.append(drop_2200)
    drop_2300s.append(drop_2300)

def calculate_time_spent_in_non_1500_states(time_trace, frequency_trace, target_frequency=1500, tolerance=50, section_length=5000):
    time_trace = np.array(time_trace)
    frequency_trace = np.array(frequency_trace)
    
    # List to store time intervals spent in non-1500 MHz states before transitioning to 1500 MHz
    time_intervals = []
    
    # Variable to count the number of transitions to 1500 MHz
    transition_count = 0
    
    # Variables to track the current state (non-1500 MHz) and start time
    current_state = frequency_trace[0]
    start_time = time_trace[0]
    in_1500_state = False  # To track if we are currently in the 1500 MHz state
    
    # Iterate over the frequency trace to detect transitions to and from 1500 MHz
    for i in range(1, len(frequency_trace)):
        if(i % section_length == 0):
            current_state = frequency_trace[i]
            start_time = time_trace[i]
            
            in_1500_state = False  # To track if we are currently in the 1500 MHz state
        # If we are in 1500 MHz, check if we exit it
        if target_frequency - tolerance <= frequency_trace[i] <= target_frequency + tolerance:
            if not in_1500_state:
                # We just entered 1500 MHz
                in_1500_state = True
                
                # If we were tracking a non-1500 MHz state before, calculate the time
                if current_state is not None:
                    time_interval = time_trace[i] - start_time
                    time_intervals.append(time_interval)
                    transition_count += 1
                
                # Reset tracking for non-1500 MHz state
                current_state = None
                start_time = None
        else:
            # If we exit 1500 MHz, we need to start tracking the new state
            if in_1500_state:
                # We just exited 1500 MHz
                in_1500_state = False
                
                # Start tracking the time for the new state
                current_state = frequency_trace[i]
                start_time = time_trace[i]
    
    # Return the time intervals and the transition count
    return time_intervals, transition_count

def main():

    # Prepare output directory
    try:
        os.makedirs('plot')
    except:
        pass

    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('--raw', action='store_true')
    parser.add_argument('--steady', action='store_true')
    parser.add_argument('--drops', action='store_true')
    parser.add_argument('folder')
    args = parser.parse_args()
    in_dir = args.folder
    plot_raw_freq = args.raw
    plot_steady = args.steady
    plot_drops = args.drops

    # For steady state experiment
    freq_label_dict = {}

    # For drop idxs experiment
    freq_label_trace = {}
    
    time_label_dict = {}

    section_length = 0

    # Read data
    #    ./out/freq_%s_%04d.out
    out_files = sorted(glob.glob(in_dir + "/freq_*"))
    for f in out_files:
        label = "_".join(os.path.splitext(os.path.basename(f))[0].split("_")[1:-1])
        rept_idx = os.path.splitext(os.path.basename(f))[0].split("_")[-1]
        freq_trace, time_trace = parse_file(f)
        section_length = len(freq_trace)
        # Plot raw frequency trace if needed (useful for debug)
        if (plot_raw_freq):
            plt.figure(figsize=(20, 3.8))
            plt.plot(freq_trace)
            plt.savefig("./plot/freq_%s_%s.png" % (label, rept_idx))
            plt.clf()
            plt.close()
            
        # because the clock bins are 1500, 1600, 1700, etc. we are going to round the data to the nearest 50 MHz 
        time_label_dict.setdefault(label, []).extend(time_trace)
        # For steady state experiment
        if plot_steady:
            freq_label_dict.setdefault(label, []).extend(freq_trace)

        # For drop idxs experiment
        if plot_drops:
            freq_label_trace.setdefault(label, []).append(freq_trace)
            
        # # Step 1: Calculate the time interval (sampling interval) between consecutive points
        # sampling_interval = np.mean(np.diff(time_trace))  # In seconds
        # sampling_frequency = 1 / sampling_interval  # In Hz (samples per second)

        # filtered_values = np.where(freq_trace > 1550.0, 0, freq_trace)        # Step 2: Perform the FFT
        # print(filtered_values)
        # fft_result = np.fft.fft(filtered_values)  # Compute the FFT of the signal
        # fft_magnitude = np.abs(fft_result)  # Get the magnitude of the FFT
        # fft_frequencies = np.fft.fftfreq(len(time_trace), sampling_interval)  # Frequency axis (in Hz)
        
        
        # print(f"Sampling Frequency: {sampling_frequency} Hz")

        # # Step 3: Only keep the positive frequencies (since the FFT result is symmetric)
        # positive_freqs = fft_frequencies[:len(fft_frequencies) // 2]
        # positive_magnitude = fft_magnitude[:len(fft_magnitude) // 2]

        # # Step 4: Plot the frequency spectrum
        # plt.plot(positive_freqs, positive_magnitude)
        # plt.title("FFT of Time Series Data")
        # plt.xlabel("Frequency (Hz)3")
        # plt.ylabel("Magnitude")
        # plt.grid(True)
        # plt.show()
        
    ###########################################################

    if plot_steady:
        print("\nFrequency Mean")

        # Prepare plot
        minimum = 100000
        maximum = 0
        datas = []
        labels = []
        weights = []

        # Parse data
        for label, trace in freq_label_dict.items():
            # Example data (replace with actual data)]

            trace = np.array(trace)
            plt.figure()
            plt.plot(trace)
            plt.show()
            # Get mean/std for each selector
            samples_mean = np.mean(trace)
            samples_std = np.std(trace)
            
            print(min(trace))
            print(np.argwhere(np.isnan(trace)))
            indices = np.where(trace == 0.0)
            print(indices)
            # Print mean
            print("%15s (%d samples): %d +- %d Hz (min %d max %d)" % (label, len(trace), samples_mean, samples_std, min(trace), max(trace)))
            # Calculate the average time between any state and 1500 MHz (within tolerance)
            average_time, transition_count = calculate_time_spent_in_non_1500_states(time_label_dict[label], trace, target_frequency=1500, tolerance=150, section_length=section_length)
            print(f"Average time spent oscillating to 1500 MHz (within tolerance): {np.mean(average_time)} seconds, {transition_count} transitions")

            # Filter outliers (for the plot only)
            samples_filtered = []
            for sample in trace:
                if abs(sample - samples_mean) <= 8 * samples_std:
                    samples_filtered.append(sample)    # the + 0.05 is because the hist takes [4.2, 4.3) as range and we want 4.299999 in 4.3

            # Store data for bins
            minimum = min(round(min(samples_filtered), 1), minimum)
            maximum = max(round(max(samples_filtered), 1), maximum)
            
            datas.append(trace)
            labels.append("hw={}".format(label))
            weights.append(np.ones_like(trace)/float(len(trace)))

        # Plot all data
        plt.figure(figsize=(3, 2))
        bins = np.arange(1400, 2400 + 100, 50)    # FIXME: adjust range
        _, bins, _ = plt.hist(datas, alpha=0.5, bins=bins, weights=weights, label=labels, align="left", density=False)
        
        plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(100))
        plt.xlabel('Frequency (MHz)')
        plt.ylabel('Probability density')
        plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.show()
        plt.savefig("./plot/hist-freq.pdf", dpi=300)
        plt.clf()
        plt.close()
        
        # If we separate the clock bins between low and high
        plt.figure(figsize=(3, 2))
        bins=[0, 2000, 3000]       
        _, bins, _ = plt.hist(datas, alpha=0.5, bins=bins, weights=weights, label=labels, align="left", density=False)
        plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(1000))
        print(bins)
        xticks = [(bins[i] + bins[i+1]) / 2 for i in range(len(bins)-1)]
        plt.xticks(bins, ["< 2000", "≥ 2000", ""])
        plt.xlabel('Frequency (MHz)')
        plt.ylabel('Probability density')
        plt.legend(fontsize=7)
        plt.tight_layout(pad=0.1)
        plt.show()
        plt.savefig("./plot/hist-freq_highlow.pdf", dpi=300)
        plt.clf()
        plt.close()
        
        
        
    ###########################################################

    print(time_label_dict.keys())
    if plot_drops:

        results = []
        datas1 = []
        datas2 = []
        datas3 = []
        labels = []

        for secret_bit, all_traces_for_bit in freq_label_trace.items():
            # Process each trace separately
            processes = []
            manager = multiprocessing.Manager()
            drop_1500s = manager.list()
            drop_2200s = manager.list()
            drop_2300s = manager.list()
            for i, trace in enumerate(all_traces_for_bit[:]):  # Exclude first 5 for warmup
                print(i)
                sample_len = len(all_traces_for_bit[i])
                p = Process(target=f_pool, args=(secret_bit, i, trace, drop_1500s, drop_2200s, drop_2300s, time_label_dict[secret_bit][sample_len*i:sample_len*(i+1)]))
                processes.append(p)
                p.start()

            # Wait for classifiers to end
            for p in processes:
                p.join()

            # Filter outliers (for the plot)
            drop_1500s_mean = np.mean(drop_1500s)
            drop_1500s_std = np.std(drop_1500s)
            
            drop_2200s_mean = np.mean(drop_2200s)
            drop_2200s_std = np.std(drop_2200s)
            
            drop_2300s_mean = np.mean(drop_2300s)
            drop_2300s_std = np.std(drop_2300s)

            # Compute mean of indices and mean
            results.append("[+] Case %s: drop to 1500 at index (%.03f +- %.03f); drop to 2200 at index (%.03f +- %.03f); drop to 2300 at index (%.03f +- %.03f);"
                           % (secret_bit, drop_1500s_mean, drop_1500s_std, drop_2200s_mean, drop_2200s_std, drop_2300s_mean, drop_2300s_std))

            print(list(drop_2300s))
            datas1.append(np.array(list(drop_1500s)))
            datas2.append(np.array(list(drop_2200s)))
            datas3.append(np.array(list(drop_2300s)))
            labels.append("hw=" + str(secret_bit))

        # Print results
        if len(results) > 0:
            print("\nFrequency")
            for result in results:
                print(result)

        # Plot histogram
        plt.figure(figsize=(3, 2))
        plot_pdf(datas1, labels)
        plt.xlabel('Seconds before steady state')
        plt.ylabel('Probability density')
        plt.legend(fontsize=7)
        #plt.tight_layout(pad=0.1)
        plt.savefig("./plot/hist-drop-idx15.pdf", dpi=300)
        plt.clf()
        
        plt.figure(figsize=(3, 2))
        plot_pdf(datas2, labels)
        plt.xlabel('Seconds before steady state')
        plt.ylabel('Probability density')
        plt.legend(fontsize=7)
        #plt.tight_layout(pad=0.1)
        plt.savefig("./plot/hist-drop-idx22.pdf", dpi=300)
        plt.clf()
        
        plt.figure(figsize=(3, 2))
        plot_pdf(datas3, labels)
        plt.xlabel('Seconds before steady state')
        plt.ylabel('Probability density')
        plt.legend(fontsize=7)
        #plt.tight_layout(pad=0.1)
        plt.savefig("./plot/hist-drop-idx23.pdf", dpi=300)
        plt.clf()


if __name__ == "__main__":
    main()