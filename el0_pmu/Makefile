obj-m += el0_pmu.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
    
install:
    sudo insmod el0_pmu.ko
    
remove:
    sudo rmmod el0_pmu

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
