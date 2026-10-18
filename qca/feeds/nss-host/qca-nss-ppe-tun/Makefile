include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-tun
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-nss-ppe

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

define KernelPackage/qca-nss-ppe-tun
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE Tunnel (tunnel driver)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-qca-nss-ppe +kmod-qca-nss-ppe-vp +kmod-qca-nss-ppe-rule
  FILES:=$(PKG_BUILD_DIR)/drv/ppe_tun/qca-nss-ppe-tun.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-ppe-tun)
endef

define KernelPackage/qca-nss-ppe-tun/Description
Kernel module for PPE Tunnel Driver
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-rule),)
EXTRA_CFLAGS+= -DPPE_TUN_RULE_MODULE_ENABLED
endif

MAKE_OPTS+=ppe-tun=y

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-tun))
