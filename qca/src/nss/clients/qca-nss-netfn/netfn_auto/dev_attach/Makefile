# Makefile for the netfn capwap engine

obj-m += nss-netfn-dev-attach.o
nss-netfn-dev-attach-objs += netfn_dev_attach.o
obj ?= .

ccflags-y += -I$(obj)/include -I$(obj)/../../exports -I$(obj)/../include -I$(obj)/../flowmgr/  -I$(obj)/..
ccflags-y += -DNSS_NETFN_BUILD_ID=\"'Build_ID - $(shell date +'%m/%d/%y, %H:%M:%S') SoC=$(SoC)'\"
ccflags-y += -Wall -Werror