# Makefile for the PPE LAG manager

ccflags-y += -I$(obj)/../../exports -I$(obj)/exports
ccflags-y += -DNSS_PPE_CLIENT_BUILD_ID="$(BUILD_ID)"
ccflags-y += -DNSS_PPE_LAG_MGR_DEBUG_LEVEL=0
ccflags-y += -Wall -Werror

obj-m += qca-nss-ppe-lag.o
qca-nss-ppe-lag-objs := nss_ppe_lag.o
qca-nss-ppe-lag-objs += nss_ppe_lag_stats.o
qca-nss-ppe-lag-objs += nss_ppe_lag_dump.o

obj ?= .
