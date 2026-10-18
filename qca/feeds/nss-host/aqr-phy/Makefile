include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=aqr-phy
PKG_RELEASE:=1
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/Aquantia/linux-aqr-phy-only.git
PKG_SOURCE_VERSION:=5e182fbacb112c7adc366621d0097f89247d4265
PKG_SOURCE_SUBMODULES:=skip

include $(INCLUDE_DIR)/package.mk

define KernelPackage/aqr-phy
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:=Github AQR driver
  ifeq ($(CONFIG_AQUANTIA_PHY),)
	  FILES:=$(PKG_BUILD_DIR)/aquantia/aqr-phy.ko
  endif
  DEPENDS+=+kmod-hwmon-core
  AUTOLOAD:=$(call AutoLoad,07,aqr-phy)
  PROVIDES:=aqr-phy
endef

define KernelPackage/aqr-aqr/Description
This package contains aquantia PHY driver from github
endef

ifdef CONFIG_TARGET_NAME
AQR-PHY-CONFIG_OPTS+= \
	TARGET_NAME=$(CONFIG_TARGET_NAME)
endif

AQR-PHY-CONFIG_OPTS+= \
	LINUX_DIR=$(LINUX_DIR) \
	CROSS_COMPILE=$(TARGET_CROSS) \
	ARCH=$(LINUX_KARCH)

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) ./* $(PKG_BUILD_DIR)
	$(CP) $(PKG_BUILD_DIR)/src/Makefile $(PKG_BUILD_DIR)/aquantia/
endef

define Build/Compile
	$(KERNEL_MAKE) M=$(PKG_BUILD_DIR)/aquantia $(strip $(AQR-PHY-CONFIG_OPTS))
endef

define Build/InstallDev
endef

define KernelPackage/aqr-phy/install
endef
$(eval $(call KernelPackage,aqr-phy))
