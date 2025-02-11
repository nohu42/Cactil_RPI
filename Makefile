obj-m := ilps28qsw.o
ilps28qsw-y := ./ilps28qsw_main.o ./ilps28qsw-pid-master/ilps28qsw_reg.o 
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

all default: modules
install: modules_install

modules modules_install help clean:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) $@
