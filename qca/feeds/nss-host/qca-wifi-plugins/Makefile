include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-wifi-plugins
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-wifi-plugins

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-wifi-plugins
 SECTION:=kernel
 CATEGORY:=Kernel modules
 SUBMENU:=Network Devices
 TITLE:=Wifi Plugin module
 DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-emesh-sp @!LINUX_3_18 \
		  +PACKAGE_kmod-qca-wifi-unified-profile:kmod-qca-wifi \
		  +PACKAGE_kmod-qca-wifi-lowmem-profile:kmod-qca-wifi
 FILES:=$(PKG_BUILD_DIR)/qca-wifi-plugins.ko
 AUTOLOAD:=$(call Autoload,73,qca-wifi-plugins)
endef

define KernelPackage/qca-wifi-plugins/Description
Kernel module for wifi plugins
endef

EXTRA_CFLAGS+= \
			  -I$(STAGING_DIR)/usr/include/emesh-sp \
			  -I$(STAGING_DIR)/usr/include/qca-wifi

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

define Build/InstallDev/qca-wifi-plugins
	$(INSTALL_DIR) $(1)/usr/include/qca-wifi-plugins
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-wifi-plugins,$(1))
endef

$(eval $(call KernelPackage,qca-wifi-plugins))
