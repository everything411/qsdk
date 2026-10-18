# Makefile for netfn-auto

ccflags-y := -I$(obj) -I$(obj)/..
ccflags-y += -Wall -Werror
ccflags-y += -DNETFN_AUTO_DEBUG_LEVEL=3

obj-$(netfn_auto_flowmgr)+= flowmgr/
obj-$(netfn_auto_capwapmgr)+= capwapmgr/
obj-$(netfn_auto_flowcookie)+= flowcookie/

obj ?= .
