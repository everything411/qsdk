ifeq ($(obj),)
obj := .
endif

DEPTH := ..

include $(obj)/$(DEPTH)/os/linux/Makefile-linux.common
obj-m += telemetry_agent.o
telemetry_agent-objs := src/telemetry_agent.o src/telemetry_agent_sawf.o

EXTRA_CFLAGS+= $(COPTS)
ifneq ($(QCA_CBT_INSTRUMENTATION),1)
EXTRA_CFLAGS += -Werror
endif

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
