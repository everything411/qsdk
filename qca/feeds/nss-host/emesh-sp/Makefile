include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=emesh-sp
PKG_RELEASE:=1
PKG_LICENSE:=ISC

ifeq ($(CONFIG_TARGET_sdx75),y)
-include $(TOPDIR)/owrt-qti-ipq-prop/include/local-development.mk
else
-include $(INCLUDE_DIR)/local-development.mk
endif

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_sdx75),y)
ifeq ($(TARGET_VARIANT),perf)
  COMPILE_MODE :=perf
else
  COMPILE_MODE :=debug
endif
  EXTRA_ADDR :=lib/modules/$(LINUX_VERSION)-${COMPILE_MODE}/extra
endif

define KernelPackage/emesh-sp
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  URL:=http://www.qualcomm.com
  MAINTAINER:=Qualcomm Technologies, Inc.
  TITLE:=QTI Service Prioritization Support
  KCONFIG:=CONFIG_NETFILTER=y CONFIG_BRIDGE_NETFILTER=y
  FILES:=$(PKG_BUILD_DIR)/emesh-sp.ko
  AUTOLOAD:=$(call AutoLoad,50,emesh-sp)
endef

define KernelPackage/emesh-sp/description
 This is the kernel module for Service Prioritization in EasyMesh
endef

EMESH_SP_HEADERS= \
            $(PKG_BUILD_DIR)/sp_api.h

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/emesh-sp
	$(foreach header_file,$(EMESH_SP_HEADERS), $(CP) $(header_file) $(1)/usr/include/emesh-sp;)
ifeq ($(CONFIG_TARGET_sdx75),y)
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/emesh-sp
	$(CP) $(PKG_BUILD_DIR)/Module.symvers $(STAGING_DIR)/usr/include/emesh-sp
endif
endef

define Build/Compile

ifeq ($(CONFIG_TARGET_sdx75),y)
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		modules
else
	$(KERNEL_MAKE) \
		M="$(PKG_BUILD_DIR)" \
		modules
endif
endef

ifeq ($(CONFIG_TARGET_sdx75),y)
define  KernelPackage/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/$(EXTRA_ADDR)/
	$(INSTALL_DIR) $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/emesh-sp.ko $(1)/$(EXTRA_ADDR)/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/emesh-sp.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
endef
endif

$(eval $(call KernelPackage,emesh-sp))
