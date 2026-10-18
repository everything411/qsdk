#
# Makefile for MMAP Telemetry
#
obj ?= .

MMAP_TELEMETRY_BASE_OBJS := mmap_telemetry.o

obj-m += mmap-telemetry.o

#
# Base files
#
mmap-telemetry-objs := $(MMAP_TELEMETRY_BASE_OBJS)

ccflags-y += -Werror -Wall -I$(obj) -I$(obj)/exports -DDEBUG_LEVEL=3
