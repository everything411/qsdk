# Makefile for PPE RULE module

ccflags-y := -I$(obj)/../exports -I$(obj)/..
ccflags-y += -DPPE_DRV_BUILD_ID="$(BUILD_ID)"
ccflags-y += -Wall -Werror
ccflags-y += -DPPE_RFS_DEBUG_LEVEL=3

obj-m += qca-nss-ppe-rule.o
qca-nss-ppe-rule-objs := ppe_rule.o
ifeq ($(PPE_RFS_ENABLED), y)
qca-nss-ppe-rule-objs += ppe_rfs/ppe_rfs.o
qca-nss-ppe-rule-objs += ppe_rfs/ppe_rfs_stats.o
endif

