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
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/resource.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include "../../util/util.h"

volatile static int attacker_core_ID;

#define TIME_BETWEEN_MEASUREMENTS 1000000L // 1 millisecond

#define STACK_SIZE 16384

struct args_t {
	uint64_t iters;
	int selector;
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
	int selector = arg->selector;
	uint64_t operand_small[8];

	for (int i = 0; i < 8; i++) {
		operand_small[i] = binary_uint64(selector);
		// operand_small[i] = binary_uint64(i);
		// operand_small[i] = binary_uint64(8 - i);
	}

	uint64_t full_operand = (operand_small[7] << 56) |
							(operand_small[6] << 48) |
							(operand_small[5] << 40) |
							(operand_small[4] << 32) |
							(operand_small[3] << 24) |
							(operand_small[2] << 16) |
							(operand_small[1] << 8) |
							(operand_small[0] << 0);

/* 	asm volatile(
		"mov %0,%%rbx\n\t" // set register to operand
		"mov %0,%%rcx\n\t" // set register to operand
		"mov %0,%%rdx\n\t" // set register to operand
		"mov %0,%%rsi\n\t" // set register to operand
		"mov %0,%%rdi\n\t" // set register to operand
		"mov %0,%%r8\n\t"  // set register to operand
		"mov %0,%%r9\n\t"  // set register to operand
		"mov %0,%%r10\n\t" // set register to operand
		"mov %0,%%r11\n\t" // set register to operand
		"mov %0,%%r12\n\t" // set register to operand
		"mov %0,%%r13\n\t" // set register to operand
		"mov %0,%%r14\n\t" // set register to operand
		"mov %0,%%r15\n\t" // set register to operand

		".align 64\n\t"
		"loop:\n\t"

		"or %0,%%rbx\n\t"
		"or %0,%%rcx\n\t"
		"or %0,%%rdx\n\t"
		"or %0,%%rsi\n\t"
		"or %0,%%rdi\n\t"
		"or %0,%%r8\n\t"
		"or %0,%%r9\n\t"
		"or %0,%%r10\n\t"
		"or %0,%%r11\n\t"
		"or %0,%%r12\n\t"
		"or %0,%%r13\n\t"
		"or %0,%%r14\n\t"
		"or %0,%%r15\n\t"

		"or %0,%%rbx\n\t"
		"or %0,%%rcx\n\t"
		"or %0,%%rdx\n\t"
		"or %0,%%rsi\n\t"
		"or %0,%%rdi\n\t"
		"or %0,%%r8\n\t"
		"or %0,%%r9\n\t"
		"or %0,%%r10\n\t"
		"or %0,%%r11\n\t"
		"or %0,%%r12\n\t"
		"or %0,%%r13\n\t"
		"or %0,%%r14\n\t"
		"or %0,%%r15\n\t"

		"jmp loop\n\t"
		:
		: "r"(full_operand)
		: "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15");
 */
 
     asm volatile(
        "mov x1, %0\n\t"  // Set x1 to full_operand
        "mov x2, %0\n\t"  // Set x2 to full_operand
        "mov x3, %0\n\t"  // Set x3 to full_operand
        "mov x4, %0\n\t"  // Set x4 to full_operand
        "mov x5, %0\n\t"  // Set x5 to full_operand
        "mov x6, %0\n\t"  // Set x6 to full_operand
        "mov x7, %0\n\t"  // Set x7 to full_operand
        "mov x8, %0\n\t"  // Set x8 to full_operand
        "mov x9, %0\n\t"  // Set x9 to full_operand
        "mov x10, %0\n\t" // Set x10 to full_operand
        "mov x11, %0\n\t" // Set x11 to full_operand
        "mov x12, %0\n\t" // Set x12 to full_operand
        "mov x13, %0\n\t" // Set x13 to full_operand
        "mov x14, %0\n\t" // Set x14 to full_operand
        "mov x15, %0\n\t" // Set x15 to full_operand

        ".balign 64\n\t"
        "loop:\n\t"

        "orr x1, x1, %0\n\t"  // OR x1 with full_operand
        "orr x2, x2, %0\n\t"  // OR x2 with full_operand
        "orr x3, x3, %0\n\t"  // OR x3 with full_operand
        "orr x4, x4, %0\n\t"  // OR x4 with full_operand
        "orr x5, x5, %0\n\t"  // OR x5 with full_operand
        "orr x6, x6, %0\n\t"  // OR x6 with full_operand
        "orr x7, x7, %0\n\t"  // OR x7 with full_operand
        "orr x8, x8, %0\n\t"  // OR x8 with full_operand
        "orr x9, x9, %0\n\t"  // OR x9 with full_operand
        "orr x10, x10, %0\n\t" // OR x10 with full_operand
        "orr x11, x11, %0\n\t" // OR x11 with full_operand
        "orr x12, x12, %0\n\t" // OR x12 with full_operand
        "orr x13, x13, %0\n\t" // OR x13 with full_operand
        "orr x14, x14, %0\n\t" // OR x14 with full_operand
        "orr x15, x15, %0\n\t" // OR x15 with full_operand

        "orr x1, x1, %0\n\t"  // OR x1 again with full_operand
        "orr x2, x2, %0\n\t"  // OR x2 again with full_operand
        "orr x3, x3, %0\n\t"  // OR x3 again with full_operand
        "orr x4, x4, %0\n\t"  // OR x4 again with full_operand
        "orr x5, x5, %0\n\t"  // OR x5 again with full_operand
        "orr x6, x6, %0\n\t"  // OR x6 again with full_operand
        "orr x7, x7, %0\n\t"  // OR x7 again with full_operand
        "orr x8, x8, %0\n\t"  // OR x8 again with full_operand
        "orr x9, x9, %0\n\t"  // OR x9 again with full_operand
        "orr x10, x10, %0\n\t" // OR x10 again with full_operand
        "orr x11, x11, %0\n\t" // OR x11 again with full_operand
        "orr x12, x12, %0\n\t" // OR x12 again with full_operand
        "orr x13, x13, %0\n\t" // OR x13 again with full_operand
        "orr x14, x14, %0\n\t" // OR x14 again with full_operand
        "orr x15, x15, %0\n\t" // OR x15 again with full_operand

        "b loop\n\t"           // Branch back to loop (equivalent to jmp loop in x86-64)

        :                     // No outputs
        : "r"(full_operand)   // Input operand (full_operand)
        : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15" // Clobbered registers
    );

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
	sprintf(output_filename, "./out/all_%02d_%06d.out", arg->selector, rept_index);
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

		// We only have the currrent power consumption, not total	
		fprintf(output_file, "%.15f %.15f\n", energy, hz);
        
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
	int selectors[100];
	size_t len = 0;
	ssize_t read = 0;
	char *line = NULL;
	while ((read = getline(&line, &len, selectors_file)) != -1) {
		if (line[read - 1] == '\n')
			line[--read] = '\0';

		// Read selector
		sscanf(line, "%d", &(selectors[num_selectors]));
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
		arg.selector = selectors[i % num_selectors];

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
