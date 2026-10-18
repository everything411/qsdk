include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-cnss
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1
PKG_BUILD_ID:=1
PKG_LICENSE:=GPLv2

ifeq ($(TARGET_VARIANT),perf)
  COMPILE_MODE :=perf
else
  COMPILE_MODE :=debug
endif

ifeq ($(DUMP)$(PKG_VERSION),)
	PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk
SOURCE_DIR := $(TOPDIR)/src/ipq/qca-cnss/
EXTRA_ADDR :=lib/modules/$(LINUX_VERSION)-${COMPILE_MODE}/extra

define KernelPackage/$(PKG_NAME)
	SECTION:=kernel
	CATEGORY:=External Kernel modules
	SUBMENU:=Network Devices
	TITLE:=CNSS Platform Driver
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

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp -rp $(SOURCE_DIR)/* $(PKG_BUILD_DIR)
	$(Build/Patch)
endef

define Build/InstallDev
	mkdir -p $(1)/usr/include/qca-cnss/
	$(CP) $(PKG_BUILD_DIR)/include/cnss2.h $(1)/usr/include/qca-cnss/
endef

CONFIG_BUILD_QSDK=y
CNSS_BUS_AHB=n

define KernelPackage/$(PKG_NAME)-stream
	SECTION:=kernel
	ATEGORY:=Kernel modules
	SUBMENU:=Network Devices
	TITLE:=CNSS stream Platform Driver

	DEPENDS:=\
		+kmod-udptunnel4 \
		+IPV6:kmod-udptunnel6 \
		+kmod-qca-cnss
	FILES:=$(PKG_BUILD_DIR)/ipq_cnss2_stream.ko
endef

ifeq ($(strip ${CONFIG_LINUX_6_1}${CONFIG_LINUX_6_6}),y)
DRV_MAKE_OPTS+=QCA_CNSS_LINUX_6_x_SUPPORT=y
endif

define KernelPackage/$(PKG_NAME)/Description
	This package contains CNSS stream Platform Driver for QTI WLAN Chipsets
endef

DRV_MAKE_OPTS+=QCA_CNSS_PCI_SUPPORT=y
DRV_MAKE_OPTS+=QCA_CNSS_DEBUG_SUPPORT=y
DRV_MAKE_OPTS+=QCA_CNSS_STREAM_MOD=y
DRV_MAKE_OPTS+=CONFIG_MHI_BUS=y
DRV_MAKE_OPTS+=CONFIG_MHI_BUS_MISC=y
DRV_MAKE_OPTS+=CONFIG_IPC_LOGGING=y
DRV_MAKE_OPTS+=CONFIG_CNSS_OUT_OF_TREE=y
DRV_MAKE_OPTS+=CONFIG_BUILD_OWRT=y
ifeq ($(CONFIG_TARGET_sdx75), y)
DRV_MAKE_OPTS+=CONFIG_TARGET_SDX75=y
endif
ifeq ($(CONFIG_TARGET_sdx85), y)
DRV_MAKE_OPTS+=CONFIG_TARGET_SDX85=y
endif

define Build/Compile
	export CONFIG_BUILD_QSDK="$(CONFIG_BUILD_QSDK)"; \
	export CNSS_BUS_AHB="$(CNSS_BUS_AHB)"; \
	$(MAKE) -C "$(LINUX_DIR)" $(strip $(DRV_MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		modules
endef

define  KernelPackage/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/$(EXTRA_ADDR)/
	$(INSTALL_DIR) $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules

	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ipq_cnss2.ko $(1)/$(EXTRA_ADDR)/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ipq_cnss2.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
endef
$(eval $(call KernelPackage,$(PKG_NAME)))
$(eval $(call KernelPackage,$(PKG_NAME)-stream))
