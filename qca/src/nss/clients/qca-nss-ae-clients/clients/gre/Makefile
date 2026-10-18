#Makefile for PPE GRE client
ifeq ($(GRE_MGR_FE_PPE_ENABLE),y)
obj-m += qca-nss-ppe-gre.o
qca-nss-ppe-gre-y := gre_mgr.o
qca-nss-ppe-gre-y += frontends/ppe/ppe_gre_mgr.o frontends/ppe/ppe_gre_mgr_stats.o
ccflags-y += -I$(obj)/frontends/ppe
ccflags-y += -I$(obj)/../../exports -I$(obj)/exports
ccflags-y += -DGRE_MGR_FE_PPE_ENABLE
endif

ccflags-y += -I$(obj) -I$(obj)/../include
ccflags-y += -DGRE_MGR_DEBUG_LEVEL=0
ccflags-y += -Wall -Werror

obj ?= .
