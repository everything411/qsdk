include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=netfn
PKG_RELEASE:=1
NETFN_BUILD_DIR:=$(TOPDIR)/qca/src/nss-userspace-oss

define Package/netfn
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Userspace utility for configuring NETFN
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx +libpthread +libnl +jansson +libopenssl
endef

define Package/netfn/description
Userspace utility for configuring NETFN
endef

define Build/Prepare
	$(CP) $(NETFN_BUILD_DIR)/netfn/*  $(PKG_BUILD_DIR)
endef

TOOL_CFLAGS:= $(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include/qca-nss-netfn \
			-I$(STAGING_DIR)/usr/include/libnl3 \

TOOL_LDFLAGS:= $(TARGET_LDFLAGS)

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TOOL_CFLAGS)" \
		LD_LIBRARY_PATH="$(TOOL_LDFLAGS)"
endef

define Package/netfn/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/obj/netfn $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,netfn))
