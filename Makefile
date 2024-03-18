KDIR ?= /lib/modules/$(shell uname -r)/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	$(MAKE) -C $(KDIR) M=$$PWD $@
