# Makefile for STANDBY driver

obj-m += qca-net-standby.o
qca-net-standby-objs := netstandby_main.o netstandby_nl.o netstandby_nl_cmn.o netstandby_nss.o netstandby_init.o
ifeq ($(erp_phase_2),y)
qca-net-standby-objs += netstandby_algo.o
endif
ccflags-y += -Wall -Werror
ccflags-y := -I$(obj)/include/
ccflags-y += -DCONFIG_NETSTANDBY=1
ccflags-y += -DNETSTANDBY_DEBUG_LEVEL=3
