ccflags-y=-I/usr/lib/gcc/x86_64-pc-linux-gnu/8.2.1/include 
EXTRA_CFLAGS := -I /home/andy/kernel-dev/linux-4.16.1/drivers/gpu/drm/i915  
obj-m += get_phys.o
all:
	make  -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
