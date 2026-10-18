include $(TOPDIR)/rules.mk

PKG_NAME:=optee-client
PKG_RELEASE:=1
PKG_VERSION:=4.6.0

PKG_SOURCE:=$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/OP-TEE/optee_client/archive/refs/tags
PKG_HASH:=a970338c9f69861901336716d89684646e4480b9970996a5b3581ae7d49fdaa3

PKG_BUILD_DIR:=$(BUILD_DIR)/optee_client-$(PKG_VERSION)

CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/optee-client
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=@TARGET_ipq96xx||TARGET_ipq52xx +libuuid
	TITLE:=Normal World Client side of the TEE
	URL:=https://github.com/OP-TEE/optee_client
endef

MAKE_FLAGS += EXPORT_DIR=$(STAGING_DIR)

define Package/optee-client/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/out/libteec/libteec.so* $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/out/libckteec/libckteec.so* $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/out/libteeacl/libteeacl.so* $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/out/libseteec/libseteec.so* $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/out/tee-supplicant/tee-supplicant $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/tee-supplicant.init $(1)/etc/init.d/tee-supplicant
endef

$(eval $(call BuildPackage,optee-client))
