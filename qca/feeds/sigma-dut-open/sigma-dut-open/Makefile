include $(TOPDIR)/rules.mk

PKG:=sigma-dut
PKG_NAME:=sigma-dut-open
PKG_VERSION:=20250224
PKG_RELEASE:=1
PKG_LICENSE:=Clear BSD, BSD-3-Clause and and ISC

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/qca/sigma-dut.git
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)-$(PKG_VERSION)
PKG_SOURCE_SUBMODULES:=skip
PKG_MIRROR_MD5SUM:=skip
PKG_MD5SUM:=skip

WIFI_ath9k:=PACKAGE_kmod-ath9k
WIFI_ath10k:=PACKAGE_kmod-ath10k
WIFI_ath11k:=PACKAGE_kmod-ath11k
WIFI_ath12k:=PACKAGE_kmod-ath12k
WIFI_wigig:=PACKAGE_kmod-qca-wigig-driver
WIFI_OPEN:=$(WIFI_ath9k)||$(WIFI_ath10k)||$(WIFI_ath11k)||$(WIFI_ath12k)||$(WIFI_wigig)

ifeq (y, $(filter y, $(CONFIG_TARGET_sdx85_cpe-v1) $(CONFIG_TARGET_echo_cpe) $(CONFIG_TARGET_sdx85_cpe)))
    include $(TOPDIR)/owrt-qti-ipq-open/include/local-development.mk
else
    include $(INCLUDE_DIR)/local-development.mk
endif

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

PKG_SOURCE_SUBDIR:=$(PKG)-$(PKG_VERSION)
PKG_SOURCE_VERSION=67009fbb9fc0d6b45fd12dbf863156a374346253
PKG_SOURCE:=$(PKG)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk

TARGET_CFLAGS+= -Wall -Werror
TARGET_CFLAGS+= -DNL80211_SUPPORT -I$(STAGING_DIR)/usr/include/libnl3
TARGET_LDFLAGS+= -lm -lnl-3 -lnl-genl-3 -lnl-route-3

ifeq (y, $(filter y, $(CONFIG_TARGET_sdx85_cpe-v1) $(CONFIG_TARGET_echo_cpe) $(CONFIG_TARGET_sdx85_cpe)))
    TARGET_CFLAGS+= -Wno-error=format-truncation
endif

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA software
  DEPENDS:=+libnl +libpthread @$(WIFI_OPEN)
  TITLE:=Sigma DUT
endef

define Package/$(PKG_NAME)/description
  $(PKG_NAME) is a control agent for automating Wi-Fi Alliance certification tests
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sigma_dut $(1)/usr/sbin
	$(INSTALL_BIN) ./files/sigma_dut.init $(1)/etc/init.d/$(PKG)
	$(INSTALL_BIN) ./files/$(PKG).config $(1)/etc/config/$(PKG)
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
