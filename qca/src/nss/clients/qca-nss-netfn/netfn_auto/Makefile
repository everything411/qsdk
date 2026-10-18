# Makefile for netfn-auto

ccflags-y := -I$(obj) -I$(obj)/.. -I$(obj)/../offload/exports/
ccflags-y += -Wall -Werror
subdir-ccflags-y += -DNETFN_AUTO_DEBUG_LEVEL=3

obj-$(netfn_auto_flowmgr)+= flowmgr/
obj-$(netfn_auto_capwapmgr)+= capwapmgr/
obj-$(netfn_auto_flowcookie)+= flowcookie/
obj-$(nss_netfn_dev_attach)+= dev_attach/

obj ?= .
