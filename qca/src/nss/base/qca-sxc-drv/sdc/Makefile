# Makefile for PPE test modules

export BUILD_ID = \"Build Id: $(shell date +'%m/%d/%y, %H:%M:%S')\"
ccflags-y += -Wall -Werror
ccflags-y += -DSDC_BUILD_ID="$(BUILD_ID)"

obj-m += qca-sdc-drv.o

qca-sdc-drv-objs +=sdc.o
qca-sdc-drv-objs +=sdc_features_ext.o
qca-sdc-drv-objs +=sdc_macdb.o
qca-sdc-drv-objs +=sdc_telemetry.o

obj ?= .
