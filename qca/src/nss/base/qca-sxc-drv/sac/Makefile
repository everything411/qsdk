# Makefile for SAC driver modules

export BUILD_ID = \"Build Id: $(shell date +'%m/%d/%y, %H:%M:%S')\"
ccflags-y += -Wall -Werror
ccflags-y += -DSAC_BUILD_ID="$(BUILD_ID)"

obj-m += qca-sac-drv.o

qca-sac-drv-objs +=sac.o
qca-sac-drv-objs +=sac_feature.o
qca-sac-drv-objs +=sac_telemetry.o

obj ?= .
