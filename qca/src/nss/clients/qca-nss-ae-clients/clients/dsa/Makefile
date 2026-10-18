ccflags-y := -I$(obj)/../../exports -I$(obj)/.. -I$(obj)/exports
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

obj-m += qca-nss-ppe-dsa-mgr.o
qca-nss-ppe-dsa-mgr-objs := nss_ppe_dsa_mgr.o

ccflags-y += -DNSS_PPE_DSA_MGR_DEBUG_LEVEL=3
ccflags-y += -Wall -Werror

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq95xx ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_BASED_DSA_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_ATH_HDR_BASED_DSA_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_ATH_DSA_VP_QUEUE_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_ATH_DSA_VP_FDB_LEARN_EN
endif
