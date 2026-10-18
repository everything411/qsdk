include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-debug
PKG_LICENSE:=ISC
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2

LOCAL_SRC:=$(TOPDIR)/qca/src/nss/base/qca-nss-ppe/nss_debug

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-nss-debug
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS debug infra
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx
  FILES:=$(PKG_BUILD_DIR)/qca-nss-debug.ko
  AUTOLOAD:=$(call AutoLoad,30,qca-nss-debug)
endef

define KernelPackage/qca-nss-debug/Description
Kernel module for NSS debug
endef

define KernelPackage/qca-nss-debug/install
        $(INSTALL_DIR) $(1)/usr/bin
endef

define Build/InstallDev/qca-nss-debug
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-debug
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-debug/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-debug,$(1))
endef

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-debug))
