include $(TOPDIR)/rules.mk

PKG_NAME:=nss-tcp-st-cli
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/nss-speedtest/nss-tcp-st-cli/
include $(INCLUDE_DIR)/local-development.mk
include $(INCLUDE_DIR)/package.mk

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

define Package/nss-tcp-st-cli
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=NSS TCP SpeedTest
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +libnl +kmod-qca-nss-netfn-tcpst +libnl-tcpst +libcurl
endef

define Package/nss-tcp-st-cli/description
 Userspace utility for NSS TCP Speedtest compliant with TR-143.
endef

EXTRA_CFLAGS:= $(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include/qca-nss-netfn \
	      -I$(STAGING_DIR)/usr/include/libnl3 \
	      -I$(STAGING_DIR)/usr/include/libnl-tcpst \

EXTRA_LDFLAGS:= $(TARGET_LDFLAGS) -L$(STAGING_DIR)/nss-tcp-st

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(EXTRA_CFLAGS)" \
		LD_LIBRARY_PATH="$(EXTRA_LDFLAGS)"
endef

define Package/nss-tcp-st-cli/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/obj/nss-tcp-st $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,nss-tcp-st-cli))
