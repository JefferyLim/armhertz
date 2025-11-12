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
#include "../../util/util.h"        


volatile static int attacker_core_ID;

#define TIME_BETWEEN_MEASUREMENTS 1000000L // 1 millisecond

#define STACK_SIZE 8192

struct args_t {
	uint64_t iters;
	int selector;
};

static __attribute__((noinline)) int victim(void *varg)
{
	struct args_t *arg = varg;
	uint64_t my_uint64 = 0x0000FFFFFFFF0000;
	uint64_t count = (uint64_t)arg->selector;

	uint64_t left = my_uint64 >> count;
	uint64_t right = my_uint64 << count;

/* 	asm volatile(
		".align 64\t\n"
		"loop:\n\t"

		"shlx %0, %1, %%rbx\n\t"
		"shlx %0, %1, %%rcx\n\t"
		"shrx %0, %2, %%rsi\n\t"
		"shrx %0, %2, %%rdi\n\t"
		"shlx %0, %1, %%r8\n\t"
		"shlx %0, %1, %%r9\n\t"
		"shrx %0, %2, %%r10\n\t"
		"shrx %0, %2, %%r11\n\t"
		"shlx %0, %1, %%r12\n\t"
		"shlx %0, %1, %%r13\n\t"

		"shrx %0, %2, %%rbx\n\t"
		"shrx %0, %2, %%rcx\n\t"
		"shlx %0, %1, %%rsi\n\t"
		"shlx %0, %1, %%rdi\n\t"
		"shrx %0, %2, %%r8\n\t"
		"shrx %0, %2, %%r9\n\t"
		"shlx %0, %1, %%r10\n\t"
		"shlx %0, %1, %%r11\n\t"
		"shrx %0, %2, %%r12\n\t"
		"shrx %0, %2, %%r13\n\t"

		"jmp loop\n\t"
		:
		: "r"(count), "r"(left), "r"(right)
		: "rbx", "rcx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13");
 */
 
     asm volatile(
        ".align 64\n\t"
        "loop:\n\t"

        "lsl x0, x1, x0\n\t"   // Logical shift left: x0 = left << x1
        "lsl x0, x1, x2\n\t"   // Logical shift left: x0 = left << x2
        "lsr x0, x2, x3\n\t"   // Logical shift right: x0 = right >> x2
        "lsr x0, x2, x4\n\t"   // Logical shift right: x0 = right >> x4
        "lsl x0, x1, x5\n\t"   // Logical shift left: x0 = left << x5
        "lsl x0, x1, x6\n\t"   // Logical shift left: x0 = left << x6
        "lsr x0, x2, x7\n\t"   // Logical shift right: x0 = right >> x7
        "lsr x0, x2, x8\n\t"   // Logical shift right: x0 = right >> x8
        "lsl x0, x1, x9\n\t"   // Logical shift left: x0 = left << x9
        "lsl x0, x1, x10\n\t"  // Logical shift left: x0 = left << x10
        "lsl x0, x1, x11\n\t"  // Logical shift left: x0 = left << x11
        "lsl x0, x1, x12\n\t"  // Logical shift left: x0 = left << x12

        "lsr x0, x2, x13\n\t"  // Logical shift right: x0 = right >> x13
        "lsr x0, x2, x14\n\t"  // Logical shift right: x0 = right >> x14
        "lsl x0, x1, x15\n\t"  // Logical shift left: x0 = left << x15
        "lsl x0, x1, x16\n\t"  // Logical shift left: x0 = left << x16

        "b loop\n\t"            // Branch to loop (equivalent to jmp loop)

        :                       // No output operands
        : "r"(count), "r"(left), "r"(right)   // Input operands
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15"  // Clobbered registers
    );

	return 0;
}

// Collects traces
static __attribute__((noinline)) int monitor(void *in)
{
	static int rept_index = 0;

	struct args_t *arg = (struct args_t *)in;

	int mb = mbox_open();
    
	// Pin monitor to a single CPU
	pin_to_core(attacker_core_ID);

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
        fprintf(freq_file, "%.15f\n", hz);
	
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