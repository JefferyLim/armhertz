#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>		/* ioctl */

#define DEVICE_FILE_NAME "/dev/vcio"
#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char *)

#define MAX_STRING 1024


// from:
// https://github.com/raspberrypi/utils/blob/master/vcgencmd/vcgencmd.c
/*
 * use ioctl to send mbox property message
 */

static int mbox_property(int file_desc, void *buf)
{
   int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

   if (ret_val < 0) {
      printf("ioctl_set_msg failed:%d\n", ret_val);
   }
   return ret_val;
}


static int mbox_open()
{
   int file_desc;

   // open a char device file used for communicating with kernel mbox driver
   file_desc = open(DEVICE_FILE_NAME, 0);
   if (file_desc < 0) {
      printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
      printf("Try creating a device file with: sudo mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
      exit(-1);
   }
   return file_desc;
}

static void mbox_close(int file_desc) {
  close(file_desc);
}


#define GET_GENCMD_RESULT 0x00030080

static unsigned gencmd(int file_desc, const char *command, char *result, int result_len)
{
   int i=0;
   unsigned p[(MAX_STRING>>2) + 7];
   int len = strlen(command);
   // maximum length for command or response
   if (len + 1 >= MAX_STRING)
   {
     fprintf(stderr, "gencmd length too long : %d\n", len);
     return -1;
   }
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = GET_GENCMD_RESULT; // (the tag id)
   p[i++] = MAX_STRING;// buffer_len
   p[i++] = 0; // request_len (set to response length)
   p[i++] = 0; // error repsonse

   memcpy(p+i, command, len + 1);
   i += MAX_STRING >> 2;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   result[0] = 0;
   strncat(result, (const char *)(p+6), result_len);

   return p[5];
}

double get_vcgencmd_value(char* buffer) {
    double value = -1;

    // Example output: temp=55.2'C  or frequency(1)=500000000
    char *eq = strchr(buffer, '=');
        
    if (eq) {
        value = atof(eq + 1); // Convert string after '=' to double
    }

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

    struct timespec ts = {0,  5 * 1000 * 100}; // 5ms // 500 us
    nanosleep(&ts, NULL);

    uint64_t end_cc = read_pmccntr();
    uint64_t end_vc = read_cntvct_el0();

    uint64_t delta_cc = end_cc - start_cc;
    uint64_t delta_vc = end_vc - start_vc;
    double time_elapsed = (double)delta_vc / (double)cntfrq;

    double freq_hz = (double)delta_cc/time_elapsed;//((double)delta_cc / ((double)delta_vc) * (double)cntfrq);
    return freq_hz; // Hz
}

// Function to read PMIC ADC values for currents and voltages
void read_pmic_adc(int mb) {
    char result[MAX_STRING] = {};
    // Define the list of ADC readings we want
    const char *pmic_commands[] = {
        "pmic_read_adc 3V7_WL_SW_A",
        "pmic_read_adc 3V3_SYS_A",
        "pmic_read_adc 3V3_SYS_V",
        "pmic_read_adc 1V8_SYS_A",
        "pmic_read_adc 1V8_SYS_V",
        "pmic_read_adc 1V1_SYS_A",
        "pmic_read_adc 1V1_SYS_V",
        "pmic_read_adc 0V8_SW_A",
        "pmic_read_adc 0V8_SW_V",
        "pmic_read_adc VDD_CORE_A",
        "pmic_read_adc VDD_CORE_V",
    };

    for (int i = 0; i < sizeof(pmic_commands) / sizeof(pmic_commands[0]); ++i) {
        char command[256];
        snprintf(command, sizeof(command), "%s", pmic_commands[i]);

        // Get the ADC value for each command
	
  	int ret = gencmd(mb, command, result, sizeof result);

        double adc_value = get_vcgencmd_value(result);

        if (adc_value >= 0) {
            printf("%s current: %.8f A\n", pmic_commands[i] + 14, adc_value);  // Skip the 'pmic_read_adc ' part in the command
        } else {
            printf("Error reading %s\n", pmic_commands[i] + 14);
        }
    }
}

int mb;

int main() {
    int core = 0;

    char command[MAX_STRING] = {};
    char result[MAX_STRING] = {};

    mb = mbox_open();

   int ret = gencmd(mb, "measure_clock arm", result, sizeof result);
   if (ret)
      printf( "vc_gencmd_read_response returned %d\n", ret );

    printf("%s\n", result);

    // Get and display CPU frequency using vcgencmd
    double vcgencmd_freq = get_vcgencmd_value(result);
    if (vcgencmd_freq != -1) {
        printf("Core clock from vcgencmd: %.2f Hz\n", vcgencmd_freq);
    } else {
        printf("Failed to retrieve clock frequency from vcgencmd.\n");
    }

    double cpu_freq = get_cpu_freq_mhz(core);

    printf("Core %d CPU frequency: %.2f MHz\n", core, cpu_freq);

    ret = gencmd(mb, "measure_temp", result, sizeof result);
    double cpu_temp = get_vcgencmd_value(result);

    ret = gencmd(mb, "measure_temp", result, sizeof result);
    double core_clk = get_vcgencmd_value(result);

    ret = gencmd(mb, "measure_volts", result, sizeof result);
    double core_volt = get_vcgencmd_value(result);

    printf("CPU Temperature: %.2f °C\n", cpu_temp);
    printf("Core Clock: %.2f MHz\n", core_clk);
    // Read and display PMIC ADC current values
    read_pmic_adc(mb);
    printf("Core: %.2fV\n", core_volt);
    return 0;
}

