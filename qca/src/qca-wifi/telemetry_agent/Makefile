ifneq ($(BUILD_PROFILE),Open)
ifeq ($(obj),)
obj := .
endif

DEPTH := ..

include $(obj)/$(DEPTH)/os/linux/Makefile-linux.common

EXTRA_CFLAGS+= $(COPTS)
ifneq ($(QCA_CBT_INSTRUMENTATION),1)
EXTRA_CFLAGS += -Werror
EXTRA_CFLAGS += -Wall
endif
endif

obj-m += telemetry_agent.o
telemetry_agent-objs := src/telemetry_agent.o src/telemetry_agent_sawf.o

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
