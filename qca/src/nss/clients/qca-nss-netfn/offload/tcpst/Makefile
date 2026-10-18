# Makefile for the netfn tcp speedtest

ccflags-y += -Wall -Werror
ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"

NETFN_TCPST_MOD_NAME=qca-nss-netfn-tcpst
obj-m += $(NETFN_TCPST_MOD_NAME).o
$(NETFN_TCPST_MOD_NAME)-objs += netfn_tcpst.o
$(NETFN_TCPST_MOD_NAME)-objs += netfn_tcpst_ppe.o
$(NETFN_TCPST_MOD_NAME)-objs += netfn_tcpst_tx.o
$(NETFN_TCPST_MOD_NAME)-objs += netfn_tcpst_rx.o
$(NETFN_TCPST_MOD_NAME)-objs += netfn_tcpst_nl.o

obj ?= .
