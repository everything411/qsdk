include $(TOPDIR)/rules.mk

PKG_NAME:=qca-oss-pkcs11
PKG_RELEASE:=1
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-oss-pkcs11/
# Include OpenWRT package build definitions
include $(INCLUDE_DIR)/local-development.mk
include $(INCLUDE_DIR)/package.mk

TARGET_LDFLAGS += \
	-L$(STAGING_DIR)/usr/lib

TARGET_CFLAGS= -fPIC -Wall -Werror

# Define package
define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=QCA-OSS-PKCS11 library
endef

define Package/qca-oss-pkcs11/description
  qca-oss-pkcs11 is a PKCS#11 library that follows the 3.0 specification.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/qca-oss-pkcs11/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libqca-oss-pkcs11.so $(1)/usr/lib/qca-oss-pkcs11/
endef


# Define installation targets
define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqca-oss-pkcs11.so $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/pkcs11_test_app $(1)/usr/sbin
endef

# Register package
$(eval $(call BuildPackage,$(PKG_NAME)))
