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

#define TIME_BETWEEN_MEASUREMENTS 5000000L // 5 millisecond

// Runs the given command
static void stress(void *command)
{
	system((char *)command);
}

struct args_t {
	uint64_t iters;
	char *selector;
};

// Collects traces
static __attribute__((noinline)) int monitor(void *in)
{
	static int rept_index = 0;

	struct args_t *arg = (struct args_t *)in;

	// Pin monitor to a single CPU
	pin_to_core(attacker_core_ID);
    int mb = mbox_open();
    
	// Set filename
	char energy_filename[64];
	sprintf(energy_filename, "./out/energy_%s_%06d.out", arg->selector, rept_index);
	char freq_filename[64];
	sprintf(freq_filename, "./out/freq_%s_%06d.out", arg->selector, rept_index);
	rept_index += 1;

	// Prepare output file
	FILE *energy_file = fopen((char *)energy_filename, "w");
	if (energy_file == NULL) {
		perror("output file");
	}
	FILE *freq_file = fopen((char *)freq_filename, "w");
	if (freq_file == NULL) {
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
		energy = read_power(mb); // adds around 0.00374 seconds on average
        hz = read_hz(mb); // adds 0.011 seconds
		//double hz = get_cpu_freq_hz(0); 
        time += (double)(start_vc - prev_vc)/(double) cntfrq;
        
        fprintf(freq_file, "%.15f %.15f\n", hz, time);
		fprintf(energy_file, "%.15f %.15f\n", energy, time);
	
		// Save current
		prev_cc = start_cc;
		prev_vc = start_vc;
	}

	// Clean up
	fclose(energy_file);
	fclose(freq_file);
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
	char *selectors[100];
	size_t len = 0;
	ssize_t read = 0;
	char *line = NULL;
	while ((read = getline(&line, &len, selectors_file)) != -1) {
		if (line[read - 1] == '\n')
			line[--read] = '\0';

		// Read selector
		selectors[num_selectors] = strdup(line);
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
		arg.selector = selectors[i % num_selectors];

		// Prepare for experiments
		pthread_t thread1, thread2;

		// Prepare background stress command
		char cpu_mask[16], command[256];
		sprintf(cpu_mask, "0-%d", ntasks - 1);
		sprintf(command, "taskset -c %s stress-ng -q --cpu %d --cpu-method %s -t 10m", cpu_mask, ntasks, selectors[i % num_selectors]);

		printf("Cooling... \n");
		// Cool down
		sleep(60);

		printf("Running: %s\n", command);
		// Start stress
		pthread_create(&thread1, NULL, (void *)&stress, (void *)command);

		// Start monitor
		pthread_create(&thread2, NULL, (void *)&monitor, (void *)&arg);

		// Wait for monitor to be done
		pthread_join(thread2, NULL);

		// Stop stress
		system("pkill -f stress-ng");

		// Join stress
		pthread_join(thread1, NULL);
	}
}

