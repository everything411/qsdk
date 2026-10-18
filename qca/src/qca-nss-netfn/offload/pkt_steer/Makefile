# Makefile for Packet Steer module

ccflags-y += -Wall -Werror
ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"
ccflags-y += -DNETFN_PKT_STEER_QUEUE_DEPTH=256

NETFN_PKT_STEER_MOD_NAME=qca-nss-netfn-pkt-steer
obj-m += $(NETFN_PKT_STEER_MOD_NAME).o
$(NETFN_PKT_STEER_MOD_NAME)-objs += netfn_pkt_steer.o

obj ?= .
