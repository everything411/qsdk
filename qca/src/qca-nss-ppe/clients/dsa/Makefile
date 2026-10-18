ccflags-y := -I$(obj)/../../exports -I$(obj)/.. -I$(obj)/../../drv/exports
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

obj-m += qca-nss-ppe-dsa-mgr.o
qca-nss-ppe-dsa-mgr-objs := nss_ppe_dsa_mgr.o

ccflags-y += -DNSS_PPE_DSA_MGR_DEBUG_LEVEL=4
ccflags-y += -Wall -Werror