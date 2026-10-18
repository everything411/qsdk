include $(TOPDIR)/rules.mk

PKG_NAME:=nss-tcp-st
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/nss-speedtest/nss-tcp-st/
include $(INCLUDE_DIR)/local-development.mk
include $(INCLUDE_DIR)/package.mk

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

define Package/libnl-tcpst
  SECTION:=Libs
  CATEGORY:=Libraries
  TITLE:=Library to facilitate communication from user to kernel for the TCP ST netlink families
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +libnl +kmod-qca-nss-netfn-tcpst
endef

define Package/libnl-tcpst/description
 Library for TCP Speedtest using TCP ST netlink family.
endef

EXTRA_CFLAGS:= -I$(STAGING_DIR)/usr/include/qca-nss-netfn \
              -I$(STAGING_DIR)/usr/include/libnl3
EXTRA_LDFLAGS:= -L$(STAGING_DIR)/nss-tcp-st

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		AR="$(TARGET_AR) " \
		CFLAGS="$(EXTRA_CFLAGS)" \
		LD_LIBRARY_PATH="$(EXTRA_LDFLAGS)"
endef

define Package/libnl-tcpst/install
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/obj/libnl-tcpst.so $(1)/lib
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/libnl-tcpst
	$(CP) $(PKG_BUILD_DIR)/obj/libnl-tcpst.so $(STAGING_DIR)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/include/* $(STAGING_DIR)/usr/include/libnl-tcpst
endef

$(eval $(call BuildPackage,libnl-tcpst))
