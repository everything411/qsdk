#
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Simple Makefile for fuseipq_ca
# Used by OpenWrt build system

TARGET := fuseipq_ca
SRCS := fuseipq_ca.c

# Compiler flags will be passed by OpenWrt
CFLAGS += $(TARGET_CFLAGS) -Wall -Wextra -O2
LDFLAGS += $(TARGET_LDFLAGS) -fpie

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
