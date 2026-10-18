include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-cnss
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1
PKG_BUILD_ID:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
	PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

MKHASH ?= $(STAGING_DIR_HOST)/bin/mkhash
PKG_BUILD_ID:=$(shell date | $(MKHASH) md5)

define KernelPackage/$(PKG_NAME)
	SECTION:=kernel
	CATEGORY:=Kernel modules
	SUBMENU:=Network Devices
	TITLE:=CNSS Platform Driver
	KCONFIG:=CONFIG_MHI_BUS_RESERVED_DMA_POOL=y
	FILES:=$(PKG_BUILD_DIR)/ipq_cnss2.ko
endef

define KernelPackage/$(PKG_NAME)/Description
	This package contains CNSS Platform Driver for QTI WLAN Chipsets
endef

define KernelPackage/$(PKG_NAME)/config
	config CNSS2_MODULE
		bool "Support compilation of CNSS2 as DLKM"
		default n
		help
	This option enables compilation of CNSS2 as a Dynamically-loaded Kernel Module
endef

define Build/InstallDev
	mkdir -p $(1)/usr/include/qca-cnss/
	$(CP) $(PKG_BUILD_DIR)/include/cnss2.h $(1)/usr/include/qca-cnss/
endef

CONFIG_BUILD_QSDK=y

define KernelPackage/$(PKG_NAME)-stream
	SECTION:=kernel
	CATEGORY:=Kernel modules
	SUBMENU:=Network Devices
	TITLE:=CNSS stream Platform Driver

	DEPENDS:=\
		+kmod-udptunnel4 \
		+IPV6:kmod-udptunnel6 \
		+kmod-qca-cnss
	FILES:=$(PKG_BUILD_DIR)/ipq_cnss2_stream.ko
endef

define KernelPackage/$(PKG_NAME)/Description
	This package contains CNSS stream Platform Driver for QTI WLAN Chipsets
endef

DRV_MAKE_OPTS+=QCA_CNSS_KERNEL_DEPENDENCY=y
ifneq ($(strip ${CONFIG_LOWMEM_FLASH}),y)
DRV_MAKE_OPTS+=QCA_CNSS_STREAM_MOD=y
DRV_MAKE_OPTS+=QCA_CNSS_PCI_SUPPORT=y
DRV_MAKE_OPTS+=QCA_CNSS_DEBUG_SUPPORT=y
endif
ifeq ($(strip ${CONFIG_LINUX_6_1}${CONFIG_LINUX_6_6}),y)
DRV_MAKE_OPTS+=QCA_CNSS_LINUX_6_x_SUPPORT=y
endif

ifeq ($(strip ${CONFIG_KERNEL_IPQ_MEM_PROFILE}),256)
DRV_MAKE_OPTS+=QCA_CNSS_LOWMEM_PROFILE=y
endif

ifeq ($(strip ${CONFIG_TARGET_ipq53xx}${CONFIG_TARGET_ipq54xx}),y)
DRV_MAKE_OPTS+=ENABLE_FW_MOUNT_SUPPORT=y
endif

ifeq ($(strip ${CONFIG_TARGET_ipq53xx}),y)
DRV_MAKE_OPTS+=ENABLE_QCA5332_HEADER=y
endif

ifeq ($(strip ${CONFIG_TARGET_ipq54xx}),y)
DRV_MAKE_OPTS+=ENABLE_QCA5424_HEADER=y
endif

define Build/Compile
	export CONFIG_BUILD_QSDK="$(CONFIG_BUILD_QSDK)"; \
	$(KERNEL_MAKE) -C "$(LINUX_DIR)" $(strip $(DRV_MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		modules
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
ifneq ($(strip ${CONFIG_LINUX_6_1}${CONFIG_LINUX_6_6}),y)
$(eval $(call KernelPackage,$(PKG_NAME)-stream))
endif
