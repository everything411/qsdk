# Makefile for netfn_auto_flowcookie
ccflags-y := -Wall -Werror
ccflags-y += -I$(obj)/include -I$(obj)/../../exports -I$(obj)/../include  -I$(obj)/..
obj-m += qca-nss-netfn-auto-flowcookie.o

ccflags-y += -DNSS_CLIENT_BUILD_ID="$(BUILD_ID)"

qca-nss-netfn-auto-flowcookie-objs := netfn_auto_flowcookie.o
