#
# Copyright (C) 2019 IOPSYS
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libbbfdm
PKG_VERSION:=1.0-2020-16-05
PKG_FIXUP:=autoreconf
PKG_SOURCE_URL:=https://dev.iopsys.eu/iopsys/bbf.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION=a3f4807ea6ae75e05eff18a519137927fb3dca9b
PKG_RELEASE=$(PKG_SOURCE_VERSION)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_MAINTAINER:=IOPSYS Dev <dev@iopsys.eu>
CONFIG_SRC_TREE_OVERRIDE:=y

PKG_SOURCE_SUBMODULES:=skip

PKG_LICENSE:=BSD 3-Clause
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/libbbfdm
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Library for broadband-forum data model
  DEPENDS:=+libuci +libubox +libubus +libblobmsg-json +libjson-c +libcurl +libopenssl
endef

define Package/libbbfdm/config
	source "$(SOURCE)/Config.in"
endef

define Package/libbbfdm/description
 Libbbfdm package with TR104 support
endef

ifeq ($(CONFIG_BBF_TR181),y)
CONFIGURE_ARGS += \
	--enable-tr181
endif

ifeq ($(CONFIG_BBF_TR104),y)
CONFIGURE_ARGS += \
	--enable-tr104
endif

ifeq ($(CONFIG_BBF_TR143),y)
CONFIGURE_ARGS += \
	--enable-tr143
endif

ifeq ($(CONFIG_BBF_VENDOR_EXTENSION),y)
CONFIGURE_ARGS += \
	--enable-vendor-extension

CONFIGURE_ARGS += \
	BBF_VENDOR_LIST="$(CONFIG_BBF_VENDOR_LIST)"

endif ##CONFIG_BBF_VENDOR_EXTENSION

TARGET_LDFLAGS += -lcrypto -lssl

define Package/libbbfdm/install
	$(INSTALL_DIR) $(1)/lib $(1)/sbin
	$(CP) $(PKG_BUILD_DIR)/ipkg-install/usr/usr/lib/libbbfdm.so* $(1)/lib/
	$(CP) $(PKG_BUILD_DIR)/ipkg-install/usr/usr/lib/libbbfdm-api.so* $(1)/lib/
	$(CP) $(PKG_BUILD_DIR)/ipkg-install/usr/usr/sbin/bbfdmd* $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/bbfdm
	$(INSTALL_DIR) $(1)/etc/bbfdm/dmmap
	$(INSTALL_DIR) $(1)/usr/share/bbfdm
	$(CP) $(PKG_BUILD_DIR)/libbbfdm/scripts/* $(1)/usr/share/bbfdm
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/usr/include/libbbfdm $(1)/usr/include/libbbfdm-api
	$(CP) $(PKG_BUILD_DIR)/libbbfdm/*.h $(1)/usr/include/libbbfdm/
	$(CP) $(PKG_BUILD_DIR)/libbbfdm-api/*.h $(1)/usr/include/libbbfdm-api/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/ipkg-install/usr/usr/sbin/* $(1)/usr/sbin/
	$(CP) $(PKG_BUILD_DIR)/ipkg-install/usr/usr/lib/libbbfdm.so* $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/ipkg-install/usr/usr/lib/libbbfdm-api.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libbbfdm))
