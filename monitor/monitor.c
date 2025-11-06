#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double get_vcgencmd_value(const char *cmd) {
    char buffer[128];
    FILE *pipe;
    double value = -1;

    // Open a pipe to vcgencmd
    pipe = popen(cmd, "r");
    if (!pipe) {
        perror("popen failed");
        return -1;
    }

    // Read the output
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // Example output: temp=55.2'C  or frequency(1)=500000000
        char *eq = strchr(buffer, '=');
        if (eq) {
            value = atof(eq + 1); // Convert string after '=' to double
        }
    }

    pclose(pipe);
    return value;
}


// Read PMCCNTR_EL0
static inline uint64_t read_pmccntr(void) {
    uint64_t val;
    asm volatile("mrs %0, pmccntr_el0" : "=r"(val));
    return val;
}

// Read CNTVCT_EL0
static inline uint64_t read_cntvct_el0(void) {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

// Read CNTFRQ_EL0
static inline uint64_t read_cntfrq_el0(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

// Pin thread to a specific CPU core
void pin_to_core(int core_id) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        perror("sched_setaffinity");
    }
}

// Get instantaneous CPU frequency in MHz
double get_cpu_freq_mhz(int core_id) {
    pin_to_core(core_id);

    uint64_t cntfrq = read_cntfrq_el0();
    uint64_t start_cc = read_pmccntr();
    uint64_t start_vc = read_cntvct_el0();

    struct timespec ts = {0, 100000}; // 100 us
    nanosleep(&ts, NULL);

    uint64_t end_cc = read_pmccntr();
    uint64_t end_vc = read_cntvct_el0();

    uint64_t delta_cc = end_cc - start_cc;
    uint64_t delta_vc = end_vc - start_vc;

    double freq_hz = ((double)delta_cc / (double)delta_vc) * cntfrq;
    return freq_hz / 1e6; // MHz
}

int main() {
    int core = 0;

    double cpu_freq = get_cpu_freq_mhz(core);

    printf("Core %d CPU frequency: %.2f MHz\n", core, cpu_freq);

    double cpu_temp = get_vcgencmd_value("vcgencmd measure_temp");
    double core_clk = get_vcgencmd_value("vcgencmd measure_clock core");

    printf("CPU Temperature: %.2f °C\n", cpu_temp);
    printf("Core Clock: %.2f MHz\n", core_clk / 1e6); // Convert Hz to MHz

    return 0;
}

