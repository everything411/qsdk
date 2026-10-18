include $(TOPDIR)/rules.mk

PKG_NAME:=mem-pool
PKG_VERSION:=1.1.2
PKG_RELEASE:=1
PKG_SOURCE:=$(PKG_VERSION).tar.gz
LICENSE:=MIT
PKG_SOURCE_URL:=https://github.com/Isty001/mem-pool/archive/refs/tags
PKG_HASH:=42828cdfb545a4105f1c44de8e2faf9b98d8676d97bd221fe75a9a21d1750821

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:= Libraries
  MAINTAINER:=Confluent
  TITLE:=Library implementing the client protocol for Kafka
  DEPENDS:= +libpthread +libstdcpp
endef

define Package/$(PKG_NAME)/description
 C library implementation of the Apache Kafka protocol, providing Producer, Consumer and Admin clients.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/mem_pool
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/include/*.h $(STAGING_DIR)/usr/include/mem_pool/
	$(INSTALL_DIR) $(STAGING_DIR)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libmem_pool.so $(STAGING_DIR)/usr/lib/
endef

define Package/${PKG_NAME}/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libmem_pool.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
