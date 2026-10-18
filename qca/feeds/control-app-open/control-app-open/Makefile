include $(TOPDIR)/rules.mk

PKG:=control-app
PKG_NAME:=control-app-open
PKG_VERSION:=20230811
PKG_RELEASE:=1.0.6
PKG_LICENSE:=ISC

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/Wi-FiQuickTrack/Wi-FiQuickTrack-ControlAppC.git
PKG_SOURCE_SUBMODULES:=skip
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)-$(PKG_VERSION)
PKG_MIRROR_MD5SUM:=skip
PKG_MD5SUM:=skip

PKG_MAINTAINER:=WFA
PKG_LICENSE:=WFA

PKG_SOURCE_SUBDIR:=$(PKG)-$(PKG_VERSION)
PKG_SOURCE_VERSION=a6bb580896cf8a985
PKG_SOURCE:=$(PKG)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk

TARGET_CFLAGS+= -Werror $(TARGET_LDFLAGS)

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA software
  TITLE:=Control App from Wi-Fi Alliance QuickTrack Project
  URL:=http://wi-fi.org/
endef

define Package/$(PKG_NAME)/description
  $(PKG_NAME) is a control agent for automating Wi-Fi Alliance QuickTrack Project
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/control_app $(1)/usr/sbin/control_app
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
