include $(TOPDIR)/rules.mk

PKG_NAME:=memory-dashboard
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Utility for memory dashboard
endef

define Package/$(PKG_NAME)/description
 A userspace utility for testing the memory subsystem for dashboard
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
