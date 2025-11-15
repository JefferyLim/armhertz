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

static __attribute__((noinline)) int victim(void *varg)
{
    struct args_t *arg = (struct args_t *)varg;

    int sel = arg->selector & 1;

    static uint8_t block_zero[16] = {0};
    static uint8_t block_rand[16] = {
        0x3a,0x1f,0x92,0x7b, 0xcc,0xa2,0x58,0x11,
        0x89,0x44,0x2b,0xd5, 0x77,0x30,0x6e,0xf9
    };

    // choose either a block zero or a random block
    // goal is to see if we can detect a difference between the two
    uint8_t *block = sel ? block_rand : block_zero;

    uint8x16_t key = vdupq_n_u8(0x42);
    uint8x16_t state = vld1q_u8(block);

    // Infinite AES workload
    for (;;) {
        state = vaeseq_u8(state, key);
        state = vaesmcq_u8(state);

        state = vaeseq_u8(state, key);
        state = vaesmcq_u8(state);

        state = vaeseq_u8(state, key);
        state = vaesmcq_u8(state);

        state = vaeseq_u8(state, key);
        state = vaesmcq_u8(state);

        state = vaeseq_u8(state, key);
        state = vaesmcq_u8(state);

        state = vaeseq_u8(state, key);
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
             "./out/aes_%02d_%06d.out", arg->selector, rept_index);
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
