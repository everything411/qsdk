# Makefile for PPE test modules

ccflags-y := -I$(obj) -I$(obj)/..
ccflags-y += -Wall -Werror

export BUILD_ID = \"Build Id: $(shell date +'%m/%d/%y, %H:%M:%S')\"
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq95xx ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_BASED_DSA_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq53xx ipq54xx ipq96xx ipq52xx),y_$(SoC))
ccflags-y += -DNSS_ATH_HDR_BASED_DSA_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_PPE_DRV_HW_GRO
endif

#comment out below option for sixth tuple in need.
ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx ipq54xx))
ccflags-y += -DNSS_PPE_SIXTH_TUPLE
endif

KERNELVERSION := $(word 1, $(subst ., ,$(KERNELVERSION))).$(word 2, $(subst ., ,$(KERNELVERSION)))

obj-y += drv/

obj-$(netlink) += netlink/
obj-$(ppe-mirror-test) += test/
obj-y += nss_debug/
obj ?= .
