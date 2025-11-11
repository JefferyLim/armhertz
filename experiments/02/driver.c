// driver.c  -- updated to use new util library (../util/util.h)

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
#include "../../util/util.h"        // new util library (contains pin_to_core, mbox/gencmd helpers if present)

/* If your util.h provides another pin function name, adjust the call in monitor() to match. */

volatile static int attacker_core_ID;

#define TIME_BETWEEN_MEASUREMENTS 1000000L // 1 millisecond
#define STACK_SIZE 8192

struct args_t {
    uint64_t iters;
    int selector;
};

uint64_t binary_uint64(int pop_count)
{
    // Length always 64
    uint64_t final_uint = 0;
    for (int i = 0; i < pop_count; i++) {
        final_uint = final_uint + (uint64_t)(pow(2, i) * 1);
    }
    return final_uint;
}

static __attribute__((noinline)) int victim(void *varg)
{
    struct args_t *arg = varg;
    uint64_t my_uint64 = binary_uint64(arg->selector);
    volatile uint64_t count = 8;
    asm volatile(
        ".p2align 6\n"
        "loop:\n"
        "lsl x2,  x1, x0\n"
        "lsl x3,  x1, x0\n"
        "lsl x4,  x1, x0\n"
        "lsl x5,  x1, x0\n"
        "lsl x6,  x1, x0\n"
        "lsl x7,  x1, x0\n"
        "lsl x8,  x1, x0\n"
        "lsl x9,  x1, x0\n"
        "lsl x10, x1, x0\n"
        "lsl x11, x1, x0\n"
        "lsl x12, x1, x0\n"
        "lsl x13, x1, x0\n"
        "lsl x14, x1, x0\n"
        "lsl x15, x1, x0\n"
        "lsl x16, x1, x0\n"
        "lsl x17, x1, x0\n"
        "lsl x18, x1, x0\n"
        "lsl x19, x1, x0\n"
        "lsl x20, x1, x0\n"
        "lsl x21, x1, x0\n"
        "lsl x22, x1, x0\n"
        "lsl x23, x1, x0\n"
        "lsl x24, x1, x0\n"
        "lsl x25, x1, x0\n"
        "b loop\n"
        :
        : "r"(count), "r"(my_uint64)
        :   "x2","x3","x4","x5","x6","x7","x8","x9","x10","x11",
            "x12","x13","x14","x15","x16","x17","x18","x19","x20",
            "x21","x22","x23","x24","x25"
    );

    return 0;
}

// Collects traces
static __attribute__((noinline)) int monitor(void *in)
{
    static int rept_index = 0;
    struct args_t *arg = (struct args_t *)in;

    // Pin monitor to a single CPU
    // NOTE: your new util library exposes pin_to_core(int). Use that instead of pin_cpu.
    pin_to_core(attacker_core_ID);

    // Set filename
    char output_filename[64];
    sprintf(output_filename, "./out/freq_%02d_%06d.out", arg->selector, rept_index);
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
    // Collect measurements
    for (uint64_t i = 0; i < arg->iters; i++) {

	struct timespec ts = {0, TIME_BETWEEN_MEASUREMENTS};
        // Wait before next measurement
        nanosleep(&ts, NULL);

        // Collect measurementi
	start_cc = read_pmccntr_el0();
	start_vc = read_cntvct_el0();

        // Store measurement
        uint64_t cc_delta = start_cc - prev_cc;
        uint64_t vc_delta = start_vc - prev_vc;
        uint64_t hz =(uint64_t)((double) cc_delta / (double) vc_delta * (double) cntfrq);
        fprintf(output_file, "%" PRIu64 "\n", hz);

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
    if (line)
        free(line);
    fclose(selectors_file);

    // Set the scheduling priority to high to avoid interruptions
    setpriority(PRIO_PROCESS, 0, -20);

    // Prepare up monitor/attacker
    attacker_core_ID = 0;

    // Allocate memory for the threads
    char *tstacks = mmap(NULL, (ntasks + 1) * STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tstacks == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Run experiment once for each selector
    for (int i = 0; i < outer * num_selectors; i++) {

        // Set alternating selector
        arg.selector = selectors[i % num_selectors];

#if (SLEEP == 1)
        // Cool down
        sleep(30);
#endif

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
    return 0;
}

