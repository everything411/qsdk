include $(TOPDIR)/rules.mk

PKG_NAME:=6relayd
PKG_VERSION:=1.00
PKG_RELEASE:=1

# Downloaded from: github release tag:
#     https://github.com/HikaruDY/6relayd/archive/refs/tags/v1.00.tar.gz
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/HikaruDY/6relayd/archive/refs/tags/v1.00.tar.gz?
PKG_MD5SUM:=7331a9cc1acdbfc878ed255be10a3987

PKG_BUILD_PARALLEL=0
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/6relayd
  SUBMENU:=IPv6 Relaying
  SECTION:=net
  CATEGORY:=Network
  TITLE:=6relayd - Embedded DHCPv6/RA Server & Relay
  URL:=https://github.com/HikaruDY/6relayd/
endef

define Package/6relayd/description
6relayd is a daemon for serving and relaying IPv6 management protocols to configure clients
and downstream routers. It tries to follow the RFC 6204 requirements for IPv6 home routers.

6relayd provides server services for RA, stateless and stateful DHCPv6, prefix delegation
and can be used to relay RA, DHCPv6 and NDP between routed (non-bridged) interfaces in case
no delegated prefixes are available.

It is optimized for embedded Linux routers and compiles to <40 KB.
endef

define Package/6relayd/conffiles
# No conffiles
endef

TARGET_CFLAGS += -D_GNU_SOURCE \
		-fstack-protector \
		-Wno-error=int-conversion \
		-Wno-error=format-security

CONFIGURE_ARGS = \
        --host=$(GNU_HOST_NAME) \
        LDFLAGS="$(TARGET_LDFLAGS) -lresolv"

# In the submake unset $CONFIG as it may conflict with 6relayd:
define Build/Compile
	$(MAKE) CONFIG="" -C $(PKG_BUILD_DIR)
endef

define Package/6relayd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/6relayd $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,6relayd))
