omciibc-objs := \
	omciibc_main.o

obj-m += omciibc.o

# guess K_SRC location


# guess PLATFORM_DIR location


$(info $(MKNAME) CHIPSET=$(CHIPSET))
$(info $(MKNAME) K_SRC=$(K_SRC))
$(info $(MKNAME) PLATFORM_DIR=$(PLATFORM_DIR))
$(info $(MKNAME) TOOLPREFIX=$(TOOLPREFIX))
$(info $(MKNAME) EXTRA_CFLAGS=$(EXTRA_CFLAGS))
#$(info $(MAKEFILE_LIST))
#$(shell sleep 10)

all: modules

modules:
	#make mk_revision
	make -C $(K_SRC) CROSS_COMPILE=$(TOOLPREFIX) M=$(shell pwd) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
        KBUILD_EXTRA_SYMBOLS="$(STAGING_DIR)/usr/include/sw-hook/Module.symvers" modules
	#make -C $(K_SRC) CROSS_COMPILE=$(TOOLPREFIX) SUBDIRS=$(shell pwd) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

install: all
	make -C $(K_SRC) CROSS_COMPILE=$(TOOLPREFIX) SUBDIRS=$(shell pwd) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules_install
	#@for f in `find . -name "*.ko"`; do \
	#       $(TOOLPREFIX)strip -g -S $$f; \
	#done

clean:
	rm -f *.o *.ko
	rm -f *~ *.mod.c .*.o.flags .*.o.d .*.o.cmd .*.ko.cmd

