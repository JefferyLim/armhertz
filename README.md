# armhertz

ARMHertz is the exploration of a Hertzbleed style attack on ARM processors. As far as we know, there has not been published work that explores this attack. 

We primarily target the Raspberry Pi5's BCM2712, which is based on the ARM Cortex-A76. 

We use the same frequency and power metrics from the original paper with some caveats. 

1. The Intel processors that Hertzbleed targetted had a very "binary" frequency state. As in the frequency fit in different frequency bins very cleanly. Modern AMD processors fit a very different profile. From our experiments, ARM also deviates quite differently 

2. There is no register that reports the wattage of the processor. The vcgencmd reports voltage on the Pi4, and the pmic on the Pi5 does report current. We utilized the VDD_CORE values as they reacted the most to a stress test. 


## el0_pmu

In order to read performance counters, you either need to rely on a kernel module, vcgencmd, or ARM's performance monitor unit (PMU)'s counters. vcgencmd or PMU require root access, which limits the feasibility of this attack.

## monitor

A monitor script that demonstrates the various metric gathering. 

## experiments

Original epxeriments from the Hertzbleed paper, but adjusted for ARM (specifically the Pi5). Experiments 7, 8, and 9 were not converted. Experiment 10 was added, targetting the ARMv8's cryptographic extensions. 