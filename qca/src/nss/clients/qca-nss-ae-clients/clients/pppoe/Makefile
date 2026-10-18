# Makefile changes for PPE frontend
ifeq ($(PPPOE_MGR_FE_PPE_ENABLE),y)
obj-m += qca-nss-ppe-pppoe-mgr.o
qca-nss-ppe-pppoe-mgr-y := pppoe_mgr.o pppoe_stats.o
qca-nss-ppe-pppoe-mgr-y += frontends/ppe/ppe_pppoe_mgr.o
ccflags-y += -DPPPOE_MGR_FE_PPE_ENABLE
ccflags-y += -I$(obj)/../exports -I$(obj)/.. -I$(obj)/exports
ccflags-y += -I$(obj)/frontends/ppe -I$(obj)
endif

ccflags-y += -Wall -Werror
ccflags-y += -DPPPOE_MGR_DEBUG_LEVEL=0
ccflags-y += -I$(obj)/../include

ifneq (,$(filter $(CONFIG_BONDING),y m))
ccflags-y += -DBONDING_SUPPORT
endif

