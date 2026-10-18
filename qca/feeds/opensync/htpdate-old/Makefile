include $(TOPDIR)/rules.mk

PKG_NAME:=htpdate-old
PKG_VERSION:=1.0.4
PKG_RELEASE:=1

PKG_TAR_NAME:=htpdate
PKG_SOURCE:=$(PKG_TAR_NAME)-$(PKG_VERSION).tar.bz2
PKG_SOURCE_URL:=https://www.vervest.org/htp/archive/c/
PKG_MD5SUM:=a13ec89839c33965794ebf53c4e690db

include $(INCLUDE_DIR)/package.mk

define Package/htpdate-old
  SUBMENU:=Time Synchronization
  SECTION:=net
  CATEGORY:=Network
  TITLE:=an HTP (Hypertext Time Protocol) implementation
  URL:=https://www.vervest.org/htp/
endef

define Package/htpdate-old/description
	The HTTP Time Protocol (HTP) is used to synchronize a computer's time
	with web servers as reference time source. Htpdate will synchronize your
	computer's time by extracting timestamps from HTTP headers found
	in web server responses. Htpdate can be used as a daemon, to keep your
	computer synchronized.
endef

TARGET_CFLAGS += -fpie -fPIC -pie

define Build/Prepare
	tar --strip-components=1 -xvjf $(DL_DIR)/$(PKG_SOURCE) -C $(PKG_BUILD_DIR)
	$(Build/Prepare/Default)
endef

define Package/htpdate-old/conffiles
/etc/default/htpdate
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/htpdate-old/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/htpdate $(1)/usr/sbin/
	#$(INSTALL_DIR) $(1)/etc/default/
	#$(INSTALL_CONF) ./files/htpdate.default $(1)/etc/default/htpdate
	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/htpdate.init $(1)/etc/init.d/htpdate
endef

$(eval $(call BuildPackage,htpdate-old))

