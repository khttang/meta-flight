# Out-of-tree module build. Yocto's module class runs "make" and "make modules_install"
# with KERNEL_SRC pointing at the kernel build tree.
obj-m := imu-sim.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod *.mod.c
	rm -f Module.markers Module.symvers modules.order
