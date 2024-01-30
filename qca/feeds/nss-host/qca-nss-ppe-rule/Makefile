include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-rule
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

define KernelPackage/qca-nss-ppe-rule
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE rule module
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx \
		+kmod-qca-nss-ppe +kmod-qca-nss-ppe-vp
  FILES:=$(PKG_BUILD_DIR)/drv/ppe_rule/qca-nss-ppe-rule.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-rule)
endef

define KernelPackage/qca-nss-ppe-rule/Description
   Kernel module for PPE rule driver
endef

define Build/InstallDev/qca-nss-ppe-rule
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ppe
	$(CP) $(PKG_BUILD_DIR)/drv/exports/* $(1)/usr/include/qca-nss-ppe/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-rule,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \

MAKE_OPTS+=ppe-rule=y
MAKE_OPTS+=PPE_RFS_ENABLED=y

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" $(strip $(MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-rule))
