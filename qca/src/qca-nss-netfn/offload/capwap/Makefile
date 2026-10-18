# Makefile for the netfn capwap engine

obj-m += qca-nss-netfn-capwap.o
qca-nss-netfn-capwap-objs += netfn_capwap.o
qca-nss-netfn-capwap-objs += netfn_capwap_tun.o
qca-nss-netfn-capwap-objs += netfn_capwap_tunid.o
qca-nss-netfn-capwap-objs += netfn_capwap_enc.o
qca-nss-netfn-capwap-objs += netfn_capwap_dec.o
obj ?= .

ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"
ccflags-y += -DNETFN_CAPWAP_TUN_MAX_REASM_WIN=128
ccflags-y += -Wall -Werror
