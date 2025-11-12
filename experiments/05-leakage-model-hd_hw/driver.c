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
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/resource.h>
#include <pthread.h>
#include <string.h>
#include "../../util/util.h"


volatile static int attacker_core_ID;

#define TIME_BETWEEN_MEASUREMENTS 1000000L // 1 millisecond

#define STACK_SIZE 16384

struct args_t {
	uint64_t iters;
	int shift_first;
	int shift_second;
	int hamming_first;
	int hamming_second;
};

uint64_t binary_uint64(int hamming)
{
	// Length always 64
	uint64_t final_uint = 0;
	for (int i = 0; i < hamming; i++) {
		final_uint = final_uint + (uint64_t)(pow(2, i) * 1);
	}
	return final_uint;
}

static __attribute__((noinline)) int victim(void *varg)
{
	struct args_t *arg = varg;

	int curr_shift_first = arg->shift_first;
	int curr_shift_second = arg->shift_second;
	int curr_hamming_first = arg->hamming_first;
	int curr_hamming_second = arg->hamming_second;

	uint64_t first, second;

	first = binary_uint64(curr_hamming_first);
	second = binary_uint64(curr_hamming_second);

	first = first << curr_shift_first;
	second = second << curr_shift_second;

/* 	asm volatile(
		"mov %0,%%r8\n\t"  // set register to operand
		"mov %0,%%r9\n\t"  // set register to operand
		"mov %0,%%r10\n\t" // set register to operand
		"mov %0,%%r11\n\t" // set register to operand
		"mov %1,%%r12\n\t" // set register to operand
		"mov %1,%%r13\n\t" // set register to operand
		"mov %1,%%r14\n\t" // set register to operand
		"mov %1,%%r15\n\t" // set register to operand

		".align 64\n\t"
		"loop:\n\t"

		"or %0,%%r8\n\t"
		"or %0,%%r9\n\t"
		"or %0,%%r10\n\t"
		"or %0,%%r11\n\t"
		"or %1,%%r12\n\t"
		"or %1,%%r13\n\t"
		"or %1,%%r14\n\t"
		"or %1,%%r15\n\t"

		"jmp loop\n\t"
		:
		: "r"(first), "r"(second)
		: "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"); */
        
    asm volatile(
        "mov x8, %0\n\t"  // Set register x8 to first operand
        "mov x9, %0\n\t"  // Set register x9 to first operand
        "mov x10, %0\n\t" // Set register x10 to first operand
        "mov x11, %0\n\t" // Set register x11 to first operand
        "mov x12, %1\n\t" // Set register x12 to second operand
        "mov x13, %1\n\t" // Set register x13 to second operand
        "mov x14, %1\n\t" // Set register x14 to second operand
        "mov x15, %1\n\t" // Set register x15 to second operand

        ".balign 64\n\t"
        "loop:\n\t"

        "orr x8, x8, %0\n\t"   // OR first operand with x8
        "orr x9, x9, %0\n\t"   // OR first operand with x9
        "orr x10, x10, %0\n\t" // OR first operand with x10
        "orr x11, x11, %0\n\t" // OR first operand with x11
        "orr x12, x12, %1\n\t" // OR second operand with x12
        "orr x13, x13, %1\n\t" // OR second operand with x13
        "orr x14, x14, %1\n\t" // OR second operand with x14
        "orr x15, x15, %1\n\t" // OR second operand with x15

        "b loop\n\t"           // Branch to loop (equivalent to 'jmp loop' in x86-64)
        :
        : "r"(first), "r"(second)
        : "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15");
	return 0;
}

// Collects traces
static __attribute__((noinline)) int monitor(void *in)
{
	static int rept_index = 0;

	struct args_t *arg = (struct args_t *)in;

	// Pin monitor to a single CPU
	pin_to_core(attacker_core_ID);
    int mb = mbox_open();
    
	// Set filename
	// The format is, e.g., ./out/all_02_2330.out
	// where 02 is the selector and 2330 is an index to prevent overwriting files
	char output_filename[64];
	sprintf(output_filename, "./out/all_%02d_%02d_%02d_%02d_%06d.out", arg->hamming_first, arg->hamming_second, arg->shift_first, arg->shift_second, rept_index);
	rept_index += 1;

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
    double energy = read_power(mb);
    double prev_energy = energy;
	
    struct timespec ts = {0, TIME_BETWEEN_MEASUREMENTS};

    // Collect measurements
    for (uint64_t i = 0; i < arg->iters; i++) {
        // Wait before next measurement
        nanosleep(&ts, NULL);

        // Collect measurementi
		start_cc = read_pmccntr_el0();
		start_vc = read_cntvct_el0();

		energy = read_power(mb);

		// Store measurement
        uint64_t cc_delta = start_cc - prev_cc;
        uint64_t vc_delta = start_vc - prev_vc;
        double hz =((double) cc_delta / (double) vc_delta * (double) cntfrq);
        fprintf(output_file, "%.15f %.15f\n", energy, hz);
	
		// We only have the currrent power consumption, not total	

        // Save current
		prev_cc = start_cc;
		prev_vc = start_vc;
		prev_energy = energy;
	}

	// Clean up
	fclose(output_file);
	return 0;
}

int main(int argc, char *argv[])
{
	// Check arguments
	if (argc != 4) {
		fprintf(stderr, "Wrong Input! Enter: %s <ntasks> <samples> <outer>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Read in args
	int ntasks;
	struct args_t arg;
	int outer;
	sscanf(argv[1], "%d", &ntasks);
	if (ntasks < 0) {
		fprintf(stderr, "ntasks cannot be negative!\n");
		exit(1);
	}
	sscanf(argv[2], "%" PRIu64, &(arg.iters));
	sscanf(argv[3], "%d", &outer);
	if (outer < 0) {
		fprintf(stderr, "outer cannot be negative!\n");
		exit(1);
	}

	// Open the selector file
	FILE *selectors_file = fopen("input.txt", "r");
	if (selectors_file == NULL)
		perror("fopen error");

	// Read the selectors file line by line
	int num_selectors = 0;
	int shift_first[10000];
	int shift_second[10000];
	int hamming_first[10000];
	int hamming_second[10000];
	size_t len = 0;
	ssize_t read = 0;
	char *line = NULL;
	while ((read = getline(&line, &len, selectors_file)) != -1) {
		if (line[read - 1] == '\n')
			line[--read] = '\0';

		// Read selector
		sscanf(line, "%d %d %d %d", &(hamming_first[num_selectors]), &(hamming_second[num_selectors]), &(shift_first[num_selectors]), &(shift_second[num_selectors]));
		num_selectors += 1;
	}

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Prepare up monitor/attacker
	attacker_core_ID = 0;

	// Allocate memory for the threads
	char *tstacks = mmap(NULL, (ntasks + 1) * STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	// Run experiment once for each selector
	for (int i = 0; i < outer * num_selectors; i++) {

		// Set alternating selector
		arg.hamming_first = hamming_first[i % num_selectors];
		arg.hamming_second = hamming_second[i % num_selectors];
		arg.shift_first = shift_first[i % num_selectors];
		arg.shift_second = shift_second[i % num_selectors];

		// Start victim threads
		int tids[ntasks];
		for (int tnum = 0; tnum < ntasks; tnum++) {
			tids[tnum] = clone(&victim, tstacks + (ntasks - tnum) * STACK_SIZE, CLONE_VM | SIGCHLD, &arg);
		}

		// Start the monitor thread
		clone(&monitor, tstacks + (ntasks + 1) * STACK_SIZE, CLONE_VM | SIGCHLD, (void *)&arg);

		// Join monitor thread
		wait(NULL);

		// Kill victim threads
		for (int tnum = 0; tnum < ntasks; tnum++) {
			syscall(SYS_tgkill, tids[tnum], tids[tnum], SIGTERM);

			// Need to join o/w the threads remain as zombies
			// https://askubuntu.com/a/427222/1552488
			wait(NULL);
		}
	}

	// Clean up
	munmap(tstacks, (ntasks + 1) * STACK_SIZE);
}
