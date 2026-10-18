# Makefile for sk offload module

obj-m += qca-nss-netfn-sk-offload.o
qca-nss-netfn-sk-offload-objs += netfn_sk_offload.o
qca-nss-netfn-sk-offload-objs += netfn_sk_offload_db.o
qca-nss-netfn-sk-offload-objs += netfn_sk_offload_rx.o
qca-nss-netfn-sk-offload-objs += netfn_sk_offload_stats.o

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNETFN_SK_OFFLOAD_HW_GRO
qca-nss-netfn-sk-offload-objs += netfn_sk_offload_rx_pages.o
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx))
ifndef PPE_SFU_PROFILE
ccflags-y += -DNETFN_SK_OFFLOAD_VEIP_SUPPORT
endif
endif

ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"
ccflags-y += -Wall -Werror
