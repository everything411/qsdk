include $(TOPDIR)/rules.mk

PKG_NAME:=librdkafka
PKG_VERSION:=2.1.1
PKG_RELEASE:=1
PKG_SOURCE:=v$(PKG_VERSION).tar.gz

PKG_SOURCE_URL:=https://github.com/confluentinc/librdkafka/archive/refs/tags/
PKG_HASH:=7be1fc37ab10ebdc037d5c5a9b35b48931edafffae054b488faaff99e60e0108
CMAKE_INSTALL:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:= Libraries
  MAINTAINER:=Confluent
  TITLE:=Library implementing the client protocol for Kafka
  DEPENDS:= +libpthread +libcurl +zlib +libopenssl +libstdcpp +liblz4
endef

define Package/$(PKG_NAME)/description
 C library implementation of the Apache Kafka protocol, providing Producer, Consumer and Admin clients.
endef

CMAKE_OPTIONS += \
		-DRDKAFKA_BUILD_EXAMPLES=ON \
		-DRDKAFKA_BUILD_TESTS=ON \
		-DWITH_SSL=ON

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/librdkafka
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/include/librdkafka/*.h $(1)/usr/include/librdkafka/
	$(INSTALL_DIR) $(1)/usr/lib/pkgconfig
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/*.pc $(1)/usr/lib/pkgconfig/
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/librdkafka*.so* $(1)/usr/lib/
endef

define Package/${PKG_NAME}/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/librdkafka*.so.* $(1)/usr/lib
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
