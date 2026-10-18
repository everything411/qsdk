include $(TOPDIR)/rules.mk

PKG_NAME:=qti-golang
PKG_VERSION:=1.16.6
PKG_SOURCE:=go1.16.6.linux-amd64.tar.gz
PKG_SOURCE_URL:=https://mirrors.edge.kernel.org/caf_mirrored_source/quic/qsdk/
PKG_MD5SUM:=27e12a2136b45dfee3a3794483cb29ed
PKG_LICENSE:=BSD-3-Clause

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)/$(PKG_NAME)-$(PKG_VERSION)
HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/$(PKG_NAME)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=qti golang package
endef

define Package/$(PKG_NAME)/description
  qti golang package
endef

Build/Compile=true

Host/Compile=true

define Host/Install
	$(INSTALL_DIR) $(STAGING_DIR_HOST)/bin/qti-golang/
	$(CP) $(BUILD_DIR_HOST)/$(PKG_NAME)/go/* $(STAGING_DIR_HOST)/bin/qti-golang/
	ln -sf $(STAGING_DIR_HOST)/bin/qti-golang/bin/go $(STAGING_DIR_HOST)/bin/go
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call HostBuild))
