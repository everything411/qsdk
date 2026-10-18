include $(TOPDIR)/rules.mk

PKG_NAME:=onu-hal-agent
PKG_VERSION:=v1.0.5
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/onu-hal

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=ipq gpon HAL
  DEPENDS:=+libubus +libubox
endef

define Package/$(PKG_NAME)/description
	ipq xpon module
endef

define Package/$(PKG_NAME)-mock/description
	ipq xpon module
endef

TARGET_LDFLAGS += -L$(STAGING_DIR) \
		  -lubox -lubus

ifeq ($(CONFIG_TARGET_ipq52xx),y)
	TARGET_CFLAGS += -DBOARD_HERMOSA
endif

define Build/Configure
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(LOCAL_SRC)/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/init.d
	$(CP) $(PKG_BUILD_DIR)/onu_hal_agent $(1)/usr/bin/onu_hal_agent
	$(INSTALL_BIN) ./onu_hal.init $(1)/etc/init.d/onu_hal
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
