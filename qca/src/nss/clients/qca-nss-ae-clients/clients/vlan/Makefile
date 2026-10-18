ccflags-y := -I$(obj)/../../exports -I$(obj)/.. -I$(obj)/exports
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

obj-m += qca-nss-ppe-vlan.o
qca-nss-ppe-vlan-objs := nss_ppe_vlan_mgr.o

ccflags-y += -DNSS_PPE_VLAN_MGR_DEBUG_LEVEL=0
ccflags-y += -Wall -Werror

ifeq ($(SoC),$(filter $(SoC),ipq95xx ipq53xx ipq54xx ipq96xx ipq52xx))
ccflags-y += -DNSS_VLAN_MGR_PPE_VP_TUN_SUPPORT
ccflags-y += -DNSS_VLAN_MGR_DEFAULT_ROLE_CORE
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq95xx ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_BASED_DSA_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_EXT_VLAN_FEATURE_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_DSA_QINQ_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_DSA_VP_FDB_LEARN_EN
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx))
ifndef PPE_SFU_PROFILE
ccflags-y += -DNSS_VLAN_VEIP_FEATURE_SUPPORT
endif
endif

ifneq (,$(filter $(CONFIG_BONDING),y m))
ccflags-y += -DBONDING_SUPPORT
endif
