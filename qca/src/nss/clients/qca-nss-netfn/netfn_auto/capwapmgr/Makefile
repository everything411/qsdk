# Makefile for netfn_auto_capwapmgr
ccflags-y := -Wall -Werror
ccflags-y += -I$(obj)/include -I$(obj)/../../exports -I$(obj)/../include -I$(obj)/../flowmgr/  -I$(obj)/..
obj-m += qca-nss-netfn-auto-capwapmgr.o

ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"

qca-nss-netfn-auto-capwapmgr-objs := netfn_auto_capwapmgr.o
