# Makefile for flow manager

ccflags-y := -I$(obj)/../../exports -I$(obj)/..

ifdef NETFN_FLOWMGR_AE_PPE_ENABLE
ccflags-y += -DNETFN_FLOWMGR_AE_PPE_ENABLE
endif

ifdef NETFN_FLOWMGR_AE_SFE_ENABLE
ccflags-y += -DNETFN_FLOWMGR_AE_SFE_ENABLE
endif

obj-m += qca-nss-flowmgr.o
qca-nss-flowmgr-objs := netfn_flowmgr.o
qca-nss-flowmgr-objs += netfn_flowmgr_stats.o
ifdef NETFN_FLOWMGR_AE_PPE_ENABLE
qca-nss-flowmgr-objs += aes/ppe/netfn_flowmgr_ppe.o
qca-nss-flowmgr-objs += aes/ppe/netfn_flowmgr_ppe_ipv4.o
qca-nss-flowmgr-objs += aes/ppe/netfn_flowmgr_ppe_ipv6.o
qca-nss-flowmgr-objs += aes/ppe/netfn_flowmgr_ppe_stats.o
endif
ifdef NETFN_FLOWMGR_AE_SFE_ENABLE
qca-nss-flowmgr-objs += aes/sfe/netfn_flowmgr_sfe.o
qca-nss-flowmgr-objs += aes/sfe/netfn_flowmgr_sfe_ipv4.o
qca-nss-flowmgr-objs += aes/sfe/netfn_flowmgr_sfe_ipv6.o
qca-nss-flowmgr-objs += aes/sfe/netfn_flowmgr_sfe_stats.o
endif

ccflags-y += -Wall -Werror
ccflags-y += -DNETFN_FLOWMGR_DEBUG_LEVEL=3
