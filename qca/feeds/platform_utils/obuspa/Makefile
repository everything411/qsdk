include $(TOPDIR)/rules.mk

PKG_NAME:=obuspa
PKG_VERSION:=5.0.0-master
PKG_RELEASE:=1
PKG_LICENSE:=BSD-3-Clause

PKG_SOURCE:=v$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/BroadbandForum/obuspa/archive/refs/tags/
PKG_HASH:=05194cdebc91ea9d7e64aefda791fae4036e5488aee63eb31cebb0e9a0eb4f8e

PKG_FIXUP:=autoreconf

include $(INCLUDE_DIR)/package.mk

define Package/obuspa
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=OB-USP agent
  DEPENDS:=+libcoap +libopenssl +libcurl +libsqlite3 +libcares +libmosquitto
endef

define Package/obuspa/description
  Boardband forum USP Agent reference implementation
endef

CONFIGURE_ARGS += \
	--disable-websockets \
	--disable-stomp

TARGET_CFLAGS+= \
	-Wno-error=use-after-free

define Package/obuspa/conffiles
/etc/config/obuspa
endef

define Package/obuspa/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/obuspa $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/obuspa.config $(1)/etc/config/obuspa
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/obuspa.init $(1)/etc/init.d/obuspa
endef

$(eval $(call BuildPackage,obuspa))
