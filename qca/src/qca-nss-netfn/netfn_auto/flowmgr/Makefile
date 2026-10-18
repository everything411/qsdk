# Makefile for netfn_auto_flowmgr
ccflags-y := -Wall -Werror
ccflags-y += -I$(obj)/include -I$(obj)/../../exports -I$(obj)/../include  -I$(obj)/..
obj-m += qca-nss-netfn-auto-flowmgr.o

ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"

qca-nss-netfn-auto-flowmgr-objs := netfn_auto_flowmgr.o
