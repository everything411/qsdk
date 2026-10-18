# Makefile for STANDBY EAWTP driver

obj-m += qca-net-standby-eawtp.o
qca-net-standby-eawtp-objs := eawtp_main.o
ccflags-y += -Wall -Werror
ccflags-y := -I$(obj)/include/
ccflags-y += -DCONFIG_NETSTANDBY=1
ccflags-y += -DQCA_NETSTANDBY_DEBUG_LEVEL=3
