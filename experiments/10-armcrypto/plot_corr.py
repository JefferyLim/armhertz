import matplotlib.pyplot as plt
import argparse
import os
import seaborn as sns
import glob
import numpy as np

## KEY IS:
    # static uint8_t master_key[16] = {
        # 0xaa,0xbb,0xcc,0xdd,0x11,0x22,0x33,0x44,
        # 0x55,0x66,0x77,0x88,0x90,0x10,0x20,0x30
    # };


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
    
SBOX = [
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
    ]
    
def xtime(a):
    """ GF(2^8) multiply by x """
    return ((a << 1) ^ 0x1B) & 0xFF if (a & 0x80) else (a << 1)
    
def AESRoundOperation(state_bytes, key_bytes):
    # 1. AddRoundKey
    s = [(state_bytes[i] ^ key_bytes[i]) for i in range(16)]

    # 2. SubBytes
    s = [SBOX[b] for b in s]

    # 3. ShiftRows
    s = [
        s[0], s[5], s[10], s[15],
        s[4], s[9], s[14], s[3],
        s[8], s[13], s[2], s[7],
        s[12], s[1], s[6], s[11]
    ]

    # 4. MixColumns (do for each column)
    out = []
    for col in range(4):
        block = s[col*4:(col+1)*4]
        out.extend(mix_single_column(block))

    return out
    
def HW(x):
    return bin(x).count("1")

def predictor_AES_round(p, kguess):
    # attack byte 0 only
    P = [0]*16
    P[0] = p

    key = [0]*16
    key[0] = kguess

    round_state = AESRoundOperation(P, key)

    # target byte = index 0 (depends on MixColumns)
    return HW(round_state[0])
    
def mix_single_column(a):
    """ Multiply a 4-byte column by AES MixColumns matrix """
    c0 = a[0]
    c1 = a[1]
    c2 = a[2]
    c3 = a[3]

    a0 = xtime(c0) ^ xtime(c1) ^ c1 ^ c2 ^ c3
    a1 = c0 ^ xtime(c1) ^ xtime(c2) ^ c2 ^ c3
    a2 = c0 ^ c1 ^ xtime(c2) ^ xtime(c3) ^ c3
    a3 = xtime(c0) ^ c0 ^ c1 ^ c2 ^ xtime(c3)

    return [a0 & 0xFF, a1 & 0xFF, a2 & 0xFF, a3 & 0xFF]


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
    all_files = sorted(glob.glob(in_dir + "/leak_*"), reverse=True)
    
    
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
    plt.xlabel('COUNT')
    plt.ylabel('Mean Frequency (GHz)')
    # plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig("./plot/" + "freq.pdf", dpi=300)
    plt.show()
    plt.close()


    x = list(set(xlabels))
    y = [np.mean(energys[k]) for k in x]
    
    
    plt.figure(figsize=(3, 2))
    plt.scatter(x, y, s=3)
    plt.xlabel('COUNT')
    plt.ylabel('Mean Power (W)')
    # plt.legend(fontsize=7)
    plt.tight_layout(pad=0.1)
    plt.savefig("./plot/" + "energy.pdf", dpi=300)
    plt.show()
    plt.close()


    mean_freq = np.array([np.mean(freqs[p]) for p in range(256)])

    HW = [bin(x).count("1") for x in range(256)]

    corr_hw = np.zeros(256)
    pred_HD = [[0]*256 for _ in range(256)]

    for kguess in range(256):
        # Prediction model: HW(SBOX[p ^ kguess])
        predicted = np.array([HW[SBOX[p ^ kguess]] for p in range(256)])

        # Pearson correlation
        corr_hw[kguess] = np.corrcoef(predicted, mean_freq)[0, 1]

    best_hw = np.argmax(np.abs(corr_hw))
    print("HD best guess:", hex(best_hw), "corr =", corr_hw[best_hw])
    plt.figure(figsize=(12,4))
    plt.plot(np.abs(corr_hw))
    plt.title("CPA Correlation for Key Guesses")
    plt.xlabel("Key Guess (0-255)")
    plt.ylabel("Correlation")
    plt.grid(True)
    plt.show()
    
    # CORRELATION USING HD MODEL
    corr_hd = np.zeros(256)

    for kguess in range(256):
        pred_hd = []
        for p in range(256):
            prev = p ^ kguess
            new  = SBOX[prev]
            pred_hd.append(bin(prev ^ new).count("1"))   # HD
        pred_hd = np.array(pred_hd)
        corr_hd[kguess] = np.corrcoef(pred_hd, mean_freq)[0, 1]

    best_hd = np.argmax(np.abs(corr_hd))
    print("HD best guess:", hex(best_hd), "corr =", corr_hd[best_hd])

    plt.figure(figsize=(12,4))
    plt.plot(np.abs(corr_hd))
    plt.title("Correlation (HD leakage model)")
    plt.xlabel("Key Guess (0–255)")
    plt.ylabel("|correlation|")
    plt.grid(True)
    plt.show()
    
    # ---- TOP 10 GUESSES (HW model) ----
    sorted_hw = np.argsort(np.abs(corr_hw))[::-1]   # sort descending by |corr|
    top10_hw = sorted_hw[:10]

    print("\nTop 10 HW key guesses:")
    for rank, k in enumerate(top10_hw, 1):
        print(f"{rank:2d}. key = 0x{k:02X}, corr = {corr_hw[k]: .6f}")


    # ---- TOP 10 GUESSES (HD model) ----
    sorted_hd = np.argsort(np.abs(corr_hd))[::-1]
    top10_hd = sorted_hd[:10]

    print("\nTop 10 HD key guesses:")
    for rank, k in enumerate(top10_hd, 1):
        print(f"{rank:2d}. key = 0x{k:02X}, corr = {corr_hd[k]: .6f}")
        
    corr = np.zeros(256)

    for g in range(256):
        pred = np.array([ predictor_AES_round(p, g) for p in range(256) ])
        corr[g] = np.corrcoef(pred, mean_freq)[0,1]
        
        
    # ---- TOP 10 GUESSES (HW model) ----
    sorted_aes = np.argsort(np.abs(corr))[::-1]   # sort descending by |corr|
    top10_aes = sorted_aes[:10]
    
    print("\nTop 10 AES Round key guesses:")
    for rank, k in enumerate(top10_aes, 1):
        print(f"{rank:2d}. key = 0x{k:02X}, corr = {corr[k]: .6f}")
        
    plt.figure(figsize=(12,4))
    plt.plot(np.abs(corr))
    plt.title("Correlation (HD leakage model)")
    plt.xlabel("Key Guess (0–255)")
    plt.ylabel("|correlation|")
    plt.grid(True)
    plt.show()
    
    
    ##########################################
    #        SECOND-ORDER CPA (HW Model)
    ##########################################

    # centered leakage
    leak = mean_freq
    leak_centered = leak - np.mean(leak)
    leak2 = leak_centered**2   # second-order measurement

    corr2 = np.zeros(256)

    for g in range(256):
        # first-order prediction
        pred1 = np.array([HW[SBOX[p ^ g]] for p in range(256)])
        pred1_centered = pred1 - np.mean(pred1)

        # second-order predictor
        pred2 = pred1_centered**2

        # correlation
        corr2[g] = np.corrcoef(pred2, leak2)[0, 1]


    # ---- TOP 10 GUESSES (HW model) ----
    sorted_aes = np.argsort(np.abs(corr2))[::-1]   # sort descending by |corr|
    top10_aes = sorted_aes[:10]
    
    print("\nTop 10 2nd Order CPA guesses:")
    for rank, k in enumerate(top10_aes, 1):
        print(f"{rank:2d}. key = 0x{k:02X}, corr = {corr2[k]: .6f}")

    plt.figure(figsize=(12,4))
    plt.plot(np.abs(corr2))
    plt.title("Second-Order CPA Correlation (HW model)")
    plt.xlabel("Key Guess (0-255)")
    plt.ylabel("|Correlation|")
    plt.grid(True)
    plt.show()
    
if __name__ == "__main__":
    main()