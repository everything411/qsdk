include $(TOPDIR)/rules.mk

PKG_NAME:=opensync-scripts
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/opensync-scripts
  SECTION:=net
  CATEGORY:=OpenSync
  TITLE:=OpenSync configuration scripts
  PKGARCH:=all
endef

define Package/opensync-scripts/description
 A set of scripts that handle setup and configuration for OpenSync devices.
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/opensync-scripts/install
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) ./files/osyncdump.conf $(1)/etc/config/osyncdump
	$(INSTALL_DIR) $(1)/etc/hotplug.d
	$(CP) ./files/00-osyncdump $(1)/etc/hotplug.d/osyncdump
	$(INSTALL_BIN) ./files/01-preboot $(1)/etc/hotplug.d/osyncdump
endef

$(eval $(call BuildPackage,opensync-scripts))
