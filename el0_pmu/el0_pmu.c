#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>

#define ARMV8_PMCR_MASK         0x3f
#define ARMV8_PMCR_E            (1 << 0)  /* Enable all counters */
#define ARMV8_PMCR_P            (1 << 1)  /* Reset all counters */
#define ARMV8_PMCR_C            (1 << 2)  /* Cycle counter reset */
#define ARMV8_PMCR_D            (1 << 3)  /* CCNT counts every 64th CPU cycle */
#define ARMV8_PMCR_X            (1 << 4)  /* Export to ETM */
#define ARMV8_PMCR_DP           (1 << 5)  /* Disable CCNT if non-invasive debug */
#define ARMV8_PMUSERENR_EN_EL0  (1 << 0)  /* EL0 access enable */
#define ARMV8_PMUSERENR_CR      (1 << 2)  /* Cycle counter read enable */
#define ARMV8_PMUSERENR_ER      (1 << 3)  /* Event counter read enable */
#define ARMV8_PMCNTENSET_EL0_ENABLE (1 << 31)  /* Enable Perf count register */

static void enable_pmu(void *info)
{
    uint64_t val;

    // Enable user-mode access to PMU (set the relevant bits in pmuserenr_el0)
    asm volatile(
        "msr pmuserenr_el0, %0" 
        : 
        : "r"((uint64_t)(ARMV8_PMUSERENR_EN_EL0 | ARMV8_PMUSERENR_ER | ARMV8_PMUSERENR_CR))
        : "memory"
    );

    // Enable all counters, reset the cycle counter, and reset event counters (PMCR bits)
    val = ARMV8_PMCR_C | ARMV8_PMCR_P;  // Enable counters, reset counters

    isb();
    // Write the modified value back to pmcr_el0
    asm volatile(
        "msr pmcr_el0, %0"
        : 
        : "r"(val)
        : "memory"
    );

    asm volatile("msr pmintenset_el1, %0" : : "r" ((uint64_t)(0 << 31)));

    // Enable the cycle counter using pmcntenset_el0 (bit 31 enables the cycle counter)
    val = 1UL << 31;  // Set the enable bit for the cycle counter
    asm volatile(
        "msr pmcntenset_el0, %0" 
        : 
        : "r"(val) 
        : "memory"
    );

    // Read current pmcr_el0 value
    asm volatile(
        "mrs %0, pmcr_el0" 
        : "=r"(val) 
        :
        : "memory"
    );

    // Enable all counters, reset the cycle counter, and reset event counters (PMCR bits)
    val |= ARMV8_PMCR_E | ARMV8_PMCR_C | ARMV8_PMCR_P;  // Enable counters, reset counters
    isb();
    // Write the modified value back to pmcr_el0
    asm volatile(
        "msr pmcr_el0, %0"
        :
        : "r"(val)
        : "memory"
    );

    pr_info("PMU enabled on CPU %u\n", smp_processor_id());
}

static void disable_pmu(void *info)
{
    uint64_t val;

    // Disable user-mode access to PMU (clear pmuserenr_el0)
    asm volatile(
        "msr pmuserenr_el0, %0" 
        : 
        : "r"((uint64_t)0)
        : "memory"
    );

    // Disable the cycle counter using pmcntenclr_el0 (clear bit 31 for cycle counter)
    val = 1UL << 31;  // Disable the cycle counter
    asm volatile(
        "msr pmcntenclr_el0, %0"
        : 
        : "r"(val)
        : "memory"
    );

    // Disable counters in pmcr_el0 by clearing the enable bit
    asm volatile(
        "mrs %0, pmcr_el0" 
        : "=r"(val) 
        :
        : "memory"
    );

    val &= ~ARMV8_PMCR_E;  // Clear the enable bit to disable counters

    asm volatile(
        "msr pmcr_el0, %0" 
        : 
        : "r"(val)
        : "memory"
    );

    pr_info("PMU disabled on CPU %u\n", smp_processor_id());
}

static int __init enable_el0_counters_init(void)
{

            int cpu;
	for_each_online_cpu(cpu){
	smp_call_function(enable_pmu, NULL, 1);  // Run on all CPUs
	}
    
	pr_info("EL0 PMU and cycle counter enabled on all CPUs\n");
    return 0;
}

static void __exit disable_el0_counters_exit(void)
{

	        int cpu;
		for_each_online_cpu(cpu){
    smp_call_function(disable_pmu, NULL, 1);  // Run on all CPUs
					   
		}
    pr_info("EL0 PMU and cycle counter disabled on all CPUs\n");
}

module_init(enable_el0_counters_init);
module_exit(disable_el0_counters_exit);

MODULE_AUTHOR("Jeffery Lim");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Enable user-mode access to ARMv8 PMU counters");
MODULE_VERSION("0");

