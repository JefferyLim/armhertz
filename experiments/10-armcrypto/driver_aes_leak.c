#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <math.h>

#include <arm_neon.h>
#include <arm_acle.h>

#include "../../util/util.h"

volatile static int attacker_core_ID = 0;

#define TIME_BETWEEN_MEASUREMENTS 1000000L
#define STACK_SIZE 8192


struct args_t {
    uint64_t iters;
    int selector;
};


/* AES-128 key expansion -> 176 bytes (11 * 16) */
static const uint8_t sbox[256] = {
  /* full S-box (same as prior messages) -- copy your sbox here */
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
};

static const uint8_t Rcon[11] = { 0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36 };

static void RotWord(uint8_t *w) {
    uint8_t t = w[0]; w[0]=w[1]; w[1]=w[2]; w[2]=w[3]; w[3]=t;
}
static void SubWord(uint8_t *w) {
    w[0]=sbox[w[0]]; w[1]=sbox[w[1]]; w[2]=sbox[w[2]]; w[3]=sbox[w[3]];
}

/* Expand AES-128 master key (16 bytes) into 176 bytes (11*16) */
static void aes128_key_expand(const uint8_t key[16], uint8_t round_keys_raw[176]) {
    for (int i = 0; i < 16; ++i) round_keys_raw[i] = key[i];
    int bytes_generated = 16;
    int rcon_iter = 1;
    uint8_t temp[4];

    while (bytes_generated < 176) {
        for (int i = 0; i < 4; ++i)
            temp[i] = round_keys_raw[bytes_generated - 4 + i];

        if (bytes_generated % 16 == 0) {
            RotWord(temp);
            SubWord(temp);
            temp[0] ^= Rcon[rcon_iter++];
        }
        for (int i = 0; i < 4; ++i) {
            round_keys_raw[bytes_generated] = round_keys_raw[bytes_generated - 16] ^ temp[i];
            ++bytes_generated;
        }
    }
}

static __attribute__((noinline)) int victim(void *varg)
{
    struct args_t *arg = (struct args_t *)varg;

    static uint8_t block[16] = {0};
    block[0] = (uint8_t)arg->selector;   // vary only byte 0
    
    static uint8_t master_key[16] = {
        0xaa,0xbb,0xcc,0xdd,0x11,0x22,0x33,0x44,
        0x55,0x66,0x77,0x88,0x90,0x10,0x20,0x30
    };
    
    static uint8_t round_keys_raw[176];
    static int keys_inited = 0;
    if (!keys_inited) {
        aes128_key_expand(master_key, round_keys_raw);
        keys_inited = 1;
    }

    /* load round keys into NEON vectors for speed */
    volatile uint8x16_t rk[11];
    for (int i = 0; i < 11; ++i)
        rk[i] = vld1q_u8(&round_keys_raw[i * 16]);

    /* output buffer to prevent optimizing away */
    volatile uint8_t out[16];

    /* Continuous workload: encrypt the SAME block repeatedly */
    for (;;) {
        volatile uint8x16_t state = vld1q_u8(block);   // reload original block each iteration

        /* Initial AddRoundKey */
        state = veorq_u8(state, rk[0]);

        /* Rounds 1..9 */
        for (int r = 1; r <= 9; ++r) {
            state = vaeseq_u8(state, rk[r]);
            state = vaesmcq_u8(state);
        }

        /* Final round (10) -- no MixColumns */
        state = vaeseq_u8(state, rk[10]);

        vst1q_u8((uint8_t *)out, state); // write result out
    }

    return 0;
}

static __attribute__((noinline)) int monitor(void *in)
{
    static int rept_index = 0;

    struct args_t *arg = (struct args_t *)in;

    pin_to_core(attacker_core_ID);

    int mb = mbox_open();

    char output_filename[128];
    snprintf(output_filename, sizeof(output_filename),
             "./out/leak_%02d_%06d.out", arg->selector, rept_index);
    rept_index++;

    // Prepare output file
    FILE *output_file = fopen((char *)output_filename, "w");
    if (output_file == NULL) {
        perror("output file");
    }

        // Prepare
    uint64_t start_cc = read_pmccntr_el0();
    uint64_t start_vc = read_cntvct_el0();
    uint64_t prev_cc = start_cc;
    uint64_t prev_vc = start_vc;
    uint64_t cntfrq = read_cntfrq_el0();
    double energy = 0;
    double time = 0;
    struct timespec ts = {0, TIME_BETWEEN_MEASUREMENTS};

    // Collect measurements
    for (uint64_t i = 0; i < arg->iters; i++) {
        // Wait before next measurement
        	nanosleep(&ts, NULL);
        	// Collect measurement
		start_cc = read_pmccntr_el0();
		start_vc = read_cntvct_el0();

	        // Adds about 0.014 seconds
		energy = read_power(mb); // adds around 0.00374 seconds on average
        	//double hz = read_hz(mb); // adds 0.011 seconds
	        time += (double)(start_vc - prev_vc)/(double) cntfrq;
		double hz = (double)(start_cc - prev_cc)/((double)(start_vc - prev_vc)/(double) cntfrq);

        	fprintf(output_file, "%.15f %.15f %.15f\n", energy, hz, time);

		// Save current
		prev_cc = start_cc;
		prev_vc = start_vc;
    }

    fclose(output_file);
    return 0;
}

/* ---------------------
   Main Experiment Setup
   --------------------- */

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s <ntasks> <samples> <outer>\n", argv[0]);
        exit(1);
    }

    int ntasks;
    struct args_t arg;
    int outer;

    sscanf(argv[1], "%d", &ntasks);
    sscanf(argv[2], "%" PRIu64, &arg.iters);
    sscanf(argv[3], "%d", &outer);

    if (ntasks <= 0) {
        fprintf(stderr, "ntasks must be positive\n");
        exit(1);
    }

    FILE *sel_file = fopen("input.txt", "r");
    if (!sel_file) {
        perror("input.txt");
        exit(1);
    }

    int selectors[128];
    int num_sel = 0;

    while (fscanf(sel_file, "%d", &selectors[num_sel]) == 1) {
        num_sel++;
    }
    fclose(sel_file);

    setpriority(PRIO_PROCESS, 0, -20);

    attacker_core_ID = 0;

    char *stacks = mmap(NULL, (ntasks + 1) * STACK_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    for (int round = 0; round < outer * num_sel; round++) {

        arg.selector = selectors[round % num_sel];
   	 
	printf("%d\n", round);
	int a = warmup();
	printf("%d\n", a);

        int tids[ntasks];
        for (int t = 0; t < ntasks; t++) {
            tids[t] = clone(&victim,
                            stacks + (ntasks - t) * STACK_SIZE,
                            CLONE_VM | SIGCHLD,
                            &arg);
        }

        clone(&monitor,
              stacks + (ntasks + 1) * STACK_SIZE,
              CLONE_VM | SIGCHLD,
              &arg);

        wait(NULL);

        for (int t = 0; t < ntasks; t++) {
            syscall(SYS_tgkill, tids[t], tids[t], SIGTERM);
            wait(NULL);
        }
    }

    munmap(stacks, (ntasks + 1) * STACK_SIZE);
    return 0;
}
