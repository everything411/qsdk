include $(TOPDIR)/rules.mk

PKG_NAME:=util-ubus
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
 PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
 PKG_VERSION:=g$(PKG_REV)
endif

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=OPTIC_APPS
  CATEGORY:=OptiCore Packages
  SUBMENU:=Applications
  TITLE:=util ubus
  DEPENDS:=+libjson-c +libubox +libblobmsg-json +libubus
endef

define Package/$(PKG_NAME)/description
  Utility ubus
endef

CONFIGURE_VARS+= \
    CC="$(TOOLCHAIN_DIR)/bin/$(TARGET_CC)"

TARGET_CFLAGS =-Wall -I$(STAGING_DIR)/usr/include -I. -fpie
TARGET_LDFLAGS =-L$(STAGING_DIR)/usr/lib -pie -lubox -lblobmsg_json -ljson-c -lubus

define Package/$(PKG_NAME)/extra_provides
    echo "libubus.so.20210603";
endef

define Build/Compile
    $(MAKE) -C $(PKG_BUILD_DIR) \
        OPENWRT_BUILD_PLATFORM="ipq807x" \
        CC="$(TOOLCHAIN_DIR)/bin/$(TARGET_CC)" \
        CFLAGS="$(TARGET_CFLAGS)" \
        LIB="$(TARGET_LDFLAGS)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_uci $(1)/usr/bin/util_uci
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_ubus_event $(1)/usr/bin/util_ubus_event
	@[ -f $(PKG_BUILD_DIR)/net_monitor ] && $(INSTALL_BIN) $(PKG_BUILD_DIR)/net_monitor $(1)/usr/bin/net_monitor
	$(INSTALL_DIR) $(1)/etc/omci
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_event_rule.cfg $(1)/etc/omci
	$(INSTALL_DIR) $(1)/etc/omci/script/network
	@[ -f $(PKG_BUILD_DIR)/net_monitor_cb.sh ] && $(INSTALL_BIN) $(PKG_BUILD_DIR)/net_monitor_cb.sh $(1)/etc/omci/script/network
	#$(INSTALL_DIR) $(1)/usr/sbin
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_uci.script/*.sh $(1)/usr/sbin
	#$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/util_uci.rpcd/* $(1)/usr/libexec/rpcd
	#$(INSTALL_DIR) $(1)/etc/omci/00base
	#$(INSTALL_BIN) ./files/net_default* $(1)/etc/omci/00base
	#$(INSTALL_BIN) ./files/*.sh $(1)/usr/bin
	#$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	#$(INSTALL_DATA) ./files/uci.json $(1)/usr/share/rpcd/acl.d
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
