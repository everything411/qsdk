# SPDX-License-Identifier: BSD-3-Clause-Clear
obj-$(CPTCFG_ATH12K) += ath12k_wifi6.o
ath12k_wifi6-y += core.o \
		  pci.o \
		  wmi.o \
		  mhi.o \
		  ce.o \
		  hw.o \
		  hal_tx.o \
		  hal_rx.o \
		  dp_rx.o \
		  dp.o \
		  hal.o \
		  hal_qcn9074.o \
		  hal_qcn9160.o \
		  hal_mon.o \
		  dp_mon.o \
		  dp_mon_filter.o

ath12k_wifi6-$(CPTCFG_ATH12K_AHB) += ahb.o

ifeq ($(CONFIG_KERNEL_ATH_GCOV_PROFILE),)
GCOV_PROFILE:=y
endif
