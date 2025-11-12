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
		final_uint = final_uint + (uint64_t)(pow(2, 63 - i) * 1);
	}
	return final_uint;
}

static __attribute__((noinline)) int victim(void *varg)
{
	struct args_t *arg = varg;
	uint64_t operand = binary_uint64(arg->selector);

	uint64_t output;
	// uint64_t mask = 0x8080808080808080;
	// operand = operand | mask;

/* 	asm volatile(
		"mov $1,%%rbx\n\t" // set register to operand

		".rept 100\n\t"
		"mov %1,%%rcx\n\t" // set register to operand
		"mov %1,%%rdx\n\t" // set register to operand
		"mov %1,%%rsi\n\t" // set register to operand
		"mov %1,%%rdi\n\t" // set register to operand
		"mov %1,%%r8\n\t"  // set register to operand
		"mov %1,%%r9\n\t"  // set register to operand
		"mov %1,%%r10\n\t" // set register to operand
		"mov %1,%%r11\n\t" // set register to operand
		"mov %1,%%r12\n\t" // set register to operand
		"mov %1,%%r13\n\t" // set register to operand
		"mov %1,%%r14\n\t" // set register to operand
		"mov %1,%%r15\n\t" // set register to operand
		".endr\n\t"

		".align 64\n\t"
		"loop:\n\t"

		".rept 100\n\t"
		"mov %%rbx, %0\n\t"
		".endr\n\t"

		"jmp loop\n\t"

		"mov %%rcx, %0\n\t"
		"mov %%rdx, %0\n\t"
		"mov %%rsi, %0\n\t"
		"mov %%rdi, %0\n\t"
		"mov %%r8, %0\n\t"
		"mov %%r9, %0\n\t"
		"mov %%r10, %0\n\t"
		"mov %%r11, %0\n\t"
		"mov %%r12, %0\n\t"
		"mov %%r13, %0\n\t"
		"mov %%r14, %0\n\t"
		"mov %%r15, %0\n\t"

		: "=m"(output)
		: "r"(operand)
		: "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"); */
	
	
	asm volatile(
    // Initialize registers
    "mov x19, #1\n\t"         // equivalent to mov $1, %%rbx

    ".rept 100\n\t"
    "mov x20, %1\n\t"         // mov %1, %%rcx
    "mov x21, %1\n\t"         // mov %1, %%rdx
    "mov x22, %1\n\t"         // mov %1, %%rsi
    "mov x23, %1\n\t"         // mov %1, %%rdi
    "mov x24, %1\n\t"         // mov %1, %%r8
    "mov x25, %1\n\t"         // mov %1, %%r9
    "mov x26, %1\n\t"         // mov %1, %%r10
    "mov x27, %1\n\t"         // mov %1, %%r11
    "mov x28, %1\n\t"         // mov %1, %%r12
    "mov x29, %1\n\t"         // mov %1, %%r13
    "mov x30, %1\n\t"         // mov %1, %%r14
    ".endr\n\t"

    ".align 6\n\t"            // align 64 bytes (2^6)
    "loop:\n\t"

    ".rept 100\n\t"
    "str x19, [%0]\n\t"       // store x19 into memory at address %0
    ".endr\n\t"

    "b loop\n\t"              // infinite loop

    // Final register stores (matches x86 structure)
    "str x20, [%0]\n\t"
    "str x21, [%0]\n\t"
    "str x22, [%0]\n\t"
    "str x23, [%0]\n\t"
    "str x24, [%0]\n\t"
    "str x25, [%0]\n\t"
    "str x26, [%0]\n\t"
    "str x27, [%0]\n\t"
    "str x28, [%0]\n\t"
    "str x29, [%0]\n\t"
    "str x30, [%0]\n\t"

    :                         // outputs
    : "r"(&output), "r"(operand)
    : "x19","x20","x21","x22","x23","x24",
      "x25","x26","x27","x28","x29","x30","memory");

	
	
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
