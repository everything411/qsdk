include $(TOPDIR)/rules.mk

PKG_NAME:=pping
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/pollere/pping
PKG_SOURCE_VERSION:=6cc6a604916d29415bd2c8f51f8be8900f6c83c8
PKG_MIRROR_HASH:=c8f92ba20d85c52349c12566ba8aa7c134fd359244fd593de1b2f470e29c3322
PKG_SOURCE_SUBMODULES:=skip

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/pping
	SECTION:=net
	CATEGORY:=Network
	TITLE:=pping
	URL:=https://pollere.net/pping.html
	DEPENDS:=+libtins
endef

define Package/pping/description
Passive ping network monitoring utility
endef

define Package/pping/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/pping $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,pping))
