# Makefile for the PPE drivers

ccflags-y := -I$(obj) -I$(obj)/.. -I$(obj)/../..

ifneq ($(ppe-ds),)
obj-m += ppe_ds/
else
ifneq ($(ppe-vp),)
obj-m += ppe_vp/
else
ifneq ($(ppe-tun),)
obj-m += ppe_tun/
else
obj-m += ppe_drv/
endif
endif
endif
ifneq ($(ppe-rule),)
obj-m += ppe_rule/
endif

obj ?= .
