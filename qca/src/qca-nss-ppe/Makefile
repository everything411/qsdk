# Makefile for PPE test modules

ccflags-y := -I$(obj) -I$(obj)/..
ccflags-y += -Wall -Werror

export BUILD_ID = \"Build Id: $(shell date +'%m/%d/%y, %H:%M:%S')\"
ccflags-y += -DNSS_PPE_BUILD_ID="$(BUILD_ID)"

ifeq ($(SoC),$(filter $(SoC),ipq95xx ipq53xx ipq54xx))
ccflags-y += -DNSS_VLAN_BASED_DSA_SUPPORT
endif

KERNELVERSION := $(word 1, $(subst ., ,$(KERNELVERSION))).$(word 2, $(subst ., ,$(KERNELVERSION)))

obj-y += drv/

obj-y += clients/
obj-$(netlink) += netlink/
obj-$(ppe-mirror-test) += test/
obj ?= .
