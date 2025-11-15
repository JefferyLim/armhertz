#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define UTIL_DEVICE_FILE_NAME "/dev/vcio"
#define UTIL_MAJOR_NUM 100
#define UTIL_MAX_STRING 256

/* IOCTL - same layout as your original code */
#define UTIL_IOCTL_MBOX_PROPERTY _IOWR(UTIL_MAJOR_NUM, 0, char *)

/* Global mailbox buffer (aligned for VideoCore) */
extern unsigned gencmd_buffer[(UTIL_MAX_STRING >> 2) + 7];

/* Open/close mailbox device. Returns file descriptor >=0 on success, -1 on error. */
int mbox_open(void);
void mbox_close(int fd);

/*
 * Send a gencmd via the mailbox driver.
 * fd: file descriptor from mbox_open
 * command: NUL-terminated command string (e.g. "measure_clock arm")
 * result: pointer to buffer to receive NUL-terminated response
 * result_len: length of result buffer (must be >0)
 * Returns: 0 on success, negative on error, or the raw response code from the mbox on success (non-negative).
 *
 * The function will NUL-terminate result and not write more than result_len bytes.
 */
int gencmd(int fd, const char *command, char *result, size_t result_len);

/*
 * Parses vcgencmd-style responses like:
 *   "temp=55.2'C" -> returns 55.2
 *   "frequency(1)=500000000" -> returns 500000000
 * If parsing fails returns -1.0
 */
double get_vcgencmd_value(const char *buffer);

/* Read performance / system counters (aarch64) */
uint64_t read_pmccntr_el0(void);   /* PMCCNTR_EL0 (physical cycle counter) */
uint64_t read_cntvct_el0(void);    /* CNTVCT_EL0 (virtual count) */
uint64_t read_cntfrq_el0(void);    /* CNTFRQ_EL0 (counter frequency) */

/*
 * Pin current thread to a CPU core.
 * Returns 0 on success, -1 on failure (and sets errno).
 */
int pin_to_core(int core_id);

/*
 * Estimate CPU frequency (Hz) for a pinned core.
 * Uses PMCCNTR_EL0 and CNTVCT_EL0 + nanosleep for a short interval.
 * Returns estimated frequency in Hz on success, negative on error.
 *
 * Note: function will call pin_to_core(core_id).
 */
double get_cpu_freq_hz(int core_id);

/*
 * Read a list of PMIC ADC values using vcgencmd.
 * fd: mailbox fd from mbox_open.
 * commands: array of NUL-terminated strings that contain vcgencmd pmic_read_adc commands.
 * n_commands: number of commands in array.
 *
 * For each command the function calls gencmd and passes the parsed double value into results[].
 * results must be an array of at least n_commands doubles.
 *
 * Returns 0 on success or -1 on error (partial results may still be written).
 */
int read_pmic_adc(int fd, const char *commands[], size_t n_commands, double results[]);

double read_power(int fd);

double read_hz(int fd);


void warmup();


#ifdef __cplusplus
}
#endif

#endif /* UTIL_H */

