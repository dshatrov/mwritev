KERNELDIR := /lib/modules/`uname -r`/build

.PHONY: all install clean

all:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

install:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean
	rm -f Module.symvers

