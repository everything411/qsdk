#
# Copyright (C) 2009-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=squashfs4
PKG_CPE_ID:=cpe:/a:phillip_lougher:squashfs
PKG_VERSION:=4.6.1
PKG_RELEASE=3

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/plougher/squashfs-tools
PKG_SOURCE_DATE:=2023-03-26
PKG_SOURCE_VERSION:=d8cb82d9840330f9344ec37b992595b5d7b44184
PKG_MIRROR_HASH:=e84026de1ab187f3f76d1b781a29259d818f887e1651225f850a62d6f90b1b9e

include $(INCLUDE_DIR)/package.mk


define Package/squashfs4
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=squashfs tools
  DEPENDS:=+liblzma +zlib
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/squashfs-tools \
		CC="$(TARGET_CC)" \
		XZ_SUPPORT=1 \
		LZMA_XZ_SUPPORT=1 \
		XATTR_SUPPORT= \
		EXTRA_CFLAGS="$(TARGET_CFLAGS) $(TARGET_CPPFLAGS)" \
		EXTRA_LDFLAGS="$(TARGET_LDFLAGS)" \
		mksquashfs unsquashfs
endef

define Package/squashfs4/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/squashfs-tools/mksquashfs $(1)/usr/sbin/mksquashfs4
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/squashfs-tools/unsquashfs $(1)/usr/sbin/unsquashfs4
endef

$(eval $(call BuildPackage,squashfs4))
