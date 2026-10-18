# Makefile for PPE test modules

ccflags-y := -I$(obj) -I$(obj)/.. -I$(obj)/exports
ccflags-y += -Wall -Werror

export BUILD_ID = \"Build Id: $(shell date +'%m/%d/%y, %H:%M:%S')\"
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq95xx ipq53xx ipq54xx),y_$(SoC))
ccflags-y += -DNSS_VLAN_BASED_DSA_SUPPORT
endif

ifeq ($(CONFIG_NET_DSA)_$(filter $(SoC),ipq53xx ipq54xx),y_$(SoC))
ccflags-y += -DNSS_ATH_HDR_BASED_DSA_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_PPE_DRV_HW_GRO
endif

KERNELVERSION := $(word 1, $(subst ., ,$(KERNELVERSION))).$(word 2, $(subst ., ,$(KERNELVERSION)))

# AE tree hosts PPE client manager modules.
obj-y += clients/
obj ?= .
