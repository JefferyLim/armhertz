#define _GNU_SOURCE
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Internal helper for mailbox ioctl */
static int mbox_property(int file_desc, void *buf)
{
    int ret_val = ioctl(file_desc, UTIL_IOCTL_MBOX_PROPERTY, buf);
    if (ret_val < 0) {
        /* perror is avoided to keep library user in control of logging */
    }
    return ret_val;
}

int mbox_open(void)
{
    int fd = open(UTIL_DEVICE_FILE_NAME, O_RDWR);
    if (fd < 0) {
        /* Caller can inspect errno */
        return -1;
    }
    return fd;
}

void mbox_close(int fd)
{
    if (fd >= 0) close(fd);
}

/* Implementation of gencmd - similar layout to your original code */
int gencmd(int file_desc, const char *command, char *result, size_t result_len)
{
    //if (!command || !result || result_len == 0) return -1;

    //if (strlen(command) + 1 >= UTIL_MAX_STRING) return -1;

    /* buffer sized to (UTIL_MAX_STRING/4) + some slack so we can use as uint32 array */
    unsigned p[(UTIL_MAX_STRING>>2) + 7];
    int i = 0;
    int len = (int)strlen(command);

    p[i++] = 0; /* size placeholder */
    p[i++] = 0x00000000; /* process request */

    /* Tag */
    p[i++] = 0x00030080; /* GET_GENCMD_RESULT */
    p[i++] = UTIL_MAX_STRING; /* buffer len */
    p[i++] = 0; /* request len (0 on call; kernel will set response) */
    p[i++] = 0; /* error/response code */

    /* Place the command string where the tag expects it (p+6) */
    memcpy(&p[i], command, len + 1);
    i += UTIL_MAX_STRING >> 2;

    p[i++] = 0x00000000; /* end tag */
    p[0] = i * (int)sizeof *p;

    int ret = mbox_property(file_desc, p);
    if (ret < 0) return -1;

    /* p[5] contains response length / status in your earlier usage */
    /* Copy the response string (starts at p+6) into result (bounded) */
    result[0] = '\0';
    strncat(result, (const char *)(p + 6), result_len - 1);

    return p[5]; /* return response code / length (same as earlier code) */
}

double get_vcgencmd_value(const char *buffer)
{
    if (!buffer) return -1.0;
    const char *eq = strchr(buffer, '=');
    if (!eq) return -1.0;
    /* Use strtod for robust parsing */
    char *endptr = NULL;
    double v = strtod(eq + 1, &endptr);
    if (endptr == eq + 1) return -1.0; /* no number parsed */
    return v;
}

/* --- aarch64 counter reads --- */
/* Read PMCCNTR_EL0 (cycle counter) */
uint64_t read_pmccntr_el0(void)
{
    uint64_t val;
    /* PMCCNTR_EL0 is accessible when user has performance counters enabled.
       If not available, reads may trap. */
    asm volatile("mrs %0, pmccntr_el0" : "=r"(val));
    return val;
}

/* Read virtual count CNTVCT_EL0 */
uint64_t read_cntvct_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

/* Read counter frequency CNTFRQ_EL0 */
uint64_t read_cntfrq_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

int pin_to_core(int core_id)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        return -1;
    }
    return 0;
}

/*
 * Estimate CPU frequency returned in Hz -- uses a short sleep and counter deltas.
 * Returns negative value on error.
 */
double get_cpu_freq_hz(int core_id)
{
    if (pin_to_core(core_id) != 0) return -1.0;

    uint64_t cntfrq = read_cntfrq_el0();
    uint64_t start_cc = read_pmccntr_el0();
    uint64_t start_vc = read_cntvct_el0();

    /* Sleep 500 microseconds - small but measurable interval */
    struct timespec ts = {0, 500000}; /* 500 us */
    nanosleep(&ts, NULL);

    uint64_t end_cc = read_pmccntr_el0();
    uint64_t end_vc = read_cntvct_el0();

    uint64_t delta_cc = end_cc - start_cc;
    uint64_t delta_vc = end_vc - start_vc;

    if (delta_vc == 0) return -1.0;

    /* frequency (Hz) = (delta_cc / delta_vc) * cntfrq */
    double freq_hz = ((double)delta_cc / (double)delta_vc) * (double)cntfrq;
    return freq_hz;
}

int read_pmic_adc(int fd, const char *commands[], size_t n_commands, double results[])
{
    if (fd < 0 || !commands || !results) return -1;

    char result_buf[UTIL_MAX_STRING];

    for (size_t i = 0; i < n_commands; ++i) {
        const char *cmd = commands[i];
        if (!cmd) {
            results[i] = -1.0;
            continue;
        }
        int ret = gencmd(fd, cmd, result_buf, sizeof result_buf);
        if (ret < 0) {
            results[i] = -1.0;
            continue;
        }
        double v = get_vcgencmd_value(result_buf);
        results[i] = v;
    }
    return 0;
}


double read_power(int fd){
    double volts, amps = 0.0;
    char result_buf[UTIL_MAX_STRING] = {};
  
    gencmd(fd, "pmic_read_adc VDD_CORE_A VDD_CORE_V", result_buf, sizeof result_buf);
    const char *a_eq = memchr(result_buf, '=', strlen(result_buf));
    amps = strtod(a_eq + 1, NULL);
    const char *v_eq = memchr(a_eq ? a_eq + 1 : result_buf, '=', strlen(result_buf));
    volts = strtod(v_eq + 1, NULL);
	return amps*volts;

}

//https://github.com/raspberrypi/linux/blob/29653ef5475124316b9284adb6cbfc97e9cae48f/drivers/clk/bcm/clk-bcm2835.c#L1955-L1964
double read_hz(int fd){
    double hz;
    char result_buf[UTIL_MAX_STRING] = {};
  
    gencmd(fd, "measure_clock arm", result_buf, sizeof result_buf);
    hz = get_vcgencmd_value(result_buf);

	return hz;
}
