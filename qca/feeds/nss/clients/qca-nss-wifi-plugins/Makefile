include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-wifi-plugins
PKG_LICENSE:=ISC
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/nss/clients/qca-nss-wifi-plugins

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-nss-wifi-plugins
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=NSS Plugin Module
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq54xx||TARGET_ipq53xx||TARGET_ipq52xx||TARGET_ipq96xx +kmod-qca-nss-ppe +kmod-qca-nss-ppe-vp +kmod-qca-nss-ppe-ds @!LINUX_3_18
  FILES:=$(PKG_BUILD_DIR)/qca-nss-wifi-plugins.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-wifi-plugins)
endef

define KernelPackage/qca-nss-wifi-plugins/Description
Kernel module for NSS-WiFi Plugins
endef


EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe-ds \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe-vp

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

define Build/InstallDev/qca-nss-wifi-plugins
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-wifi-plugins
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-wifi-plugins,$(1))
endef

$(eval $(call KernelPackage,qca-nss-wifi-plugins))

