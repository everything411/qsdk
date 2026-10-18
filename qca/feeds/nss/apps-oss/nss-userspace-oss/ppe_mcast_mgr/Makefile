include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=ppe_mcast_mgr
PKG_RELEASE:=1
PPE_MCAST_MGR_BUILD_DIR:=$(TOPDIR)/qca/src/nss/apps-oss/nss-userspace-oss/ppe

define Package/ppe_mcast_mgr
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Userspace utility for listening to Linux MDB events and configure multicast support in PPE
  DEPENDS:=@TARGET_ipq52xx +libpthread +libnl +kmod-qca-nss-ppe-netlink +libnl-ppe
endef

#TODO: Remove dependency on ppe-netlink module

define Package/ppe_mcast_mgr/description
Userspace utility for listening to Linux MDB events and configure multicast support in PPE
endef

define Build/Prepare
	$(CP) $(PPE_MCAST_MGR_BUILD_DIR)/ppe_mcast_mgr/*  $(PKG_BUILD_DIR)
endef

TOOL_CFLAGS:= $(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	      -I$(STAGING_DIR)/usr/include/libnl3 \
	      -I$(STAGING_DIR)/usr/include/libnl-ppe \

TOOL_LDFLAGS:= $(TARGET_LDFLAGS) -L$(STAGING_DIR)/ppenl_lib

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TOOL_CFLAGS)" \
		LD_LIBRARY_PATH="$(TOOL_LDFLAGS)"
endef

define Package/ppe_mcast_mgr/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/obj/ppe_mcast_mgr $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ppe_mcast_mgr.init $(1)/etc/init.d/ppe_mcast_mgr
endef

$(eval $(call BuildPackage,ppe_mcast_mgr))
