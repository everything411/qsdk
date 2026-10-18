# Makefile for the netfn capwapmgr

obj-m += qca-nss-netfn-capwapmgr.o

qca-nss-netfn-capwapmgr-objs := netfn_capwapmgr.o
qca-nss-netfn-capwapmgr-objs += netfn_capwapmgr_tun.o
qca-nss-netfn-capwapmgr-objs += netfn_capwapmgr_tunid.o
qca-nss-netfn-capwapmgr-objs += netfn_capwapmgr_legacy.o

ccflags-y += -Wall -Werror
ccflags-y += -DNETFN_CAPWAPMGR_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"
