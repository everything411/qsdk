include $(TOPDIR)/rules.mk

PKG:=qca-rptr-mgr
PKG_NAME:=$(PKG)
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

PKG_BUILD_DEPENDS:=libnl openssl PACKAGE_wpad-mesh-openssl:hostapd ucode libubus libucode ucode-mod-fs +ucode-mod-nl80211 ucode-mod-rtnl ucode-mod-ubus ucode-mod-uloop libblobmsg-json libudebug

define Package/$(PKG_NAME)
  SECTION:=QTI
  CATEGORY:=QTI software
  URL:=http://www.qualcomm.com
  MAINTAINER:=Qualcomm Technologies, Inc.
  TITLE:=QCA Repeater manager daemon
  DEPENDS:=+wpad-mesh-openssl +libpthread \
           +libubox +libubus +libucode \
           +ucode-mod-ubus +ucode-mod-uloop +ucode-mod-fs
endef

define Package/$(PKG_NAME)/config
source "$(SOURCE)/Config.in"
endef

define Package/$(PKG_NAME)/description
  User Space application to manage interfaces
endef

# Base CFLAGS
TARGET_CFLAGS += \
		  -I$(STAGING_DIR)/usr/include/ \
		  -I$(STAGING_DIR)/usr/include/qca-cfg80211/ \
		  -I$(STAGING_DIR)/usr/include/libnl \
		  -I$(STAGING_DIR)/usr/include/libnl3 \
		  -I$(STAGING_DIR)/usr/include/ucode \
		  -I$(STAGING_DIR)/usr/include/libubox \
		  -fpie -fcommon

# Hostapd/wpa_supplicant headers from hostapd-wpad-mesh-openssl build-dir
HOSTAPD_WPAD_BUILD_DIR := $(firstword $(wildcard $(BUILD_DIR)/hostapd-wpad-mesh-openssl/hostapd-*))

TARGET_CFLAGS += \
		  -I$(HOSTAPD_WPAD_BUILD_DIR)/src/utils \
		  -I$(HOSTAPD_WPAD_BUILD_DIR)/src \
		  -I$(HOSTAPD_WPAD_BUILD_DIR)/src/drivers \
		  -I$(HOSTAPD_WPAD_BUILD_DIR)/src/common

TARGET_LDFLAGS += \
		  -L$(STAGING_DIR)/usr/lib \
		  -L$(HOSTAPD_WPAD_BUILD_DIR)

ifeq ($(CONFIG_IFACE_SUPPORT_CFG80211),y)
TARGET_CFLAGS += -DIFACE_SUPPORT_CFG80211
endif

MAKEOPTS+= \
	$(TARGET_CONFIGURE_OPTS) \
	CFLAGS="$(TARGET_CFLAGS)" \
	COPTS="$(TARGET_CFLAGS)" \
	WPA_PATH=$(HOSTAPD_WPAD_BUILD_DIR)/build/hostapd/src \
	LIBS="$(TARGET_LDFLAGS)" \
	TARGET_LDFLAGS="$(TARGET_LDFLAGS)"

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(MAKEOPTS)
endef

define Install/rptr_mgr
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/rptr_mgr
	$(INSTALL_DATA) ./files/rptr_mgr.uc $(1)/usr/share/rptr_mgr/
endef

define Package/$(PKG_NAME)/install
	$(call Install/rptr_mgr,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rptr-mgr $(1)/usr/sbin

	$(INSTALL_DIR) $(1)/lib/wifi
	$(INSTALL_DATA) ./files/rptr_mgr.sh $(1)/lib/wifi
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
