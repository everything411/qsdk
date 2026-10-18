include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=ppecfg
PKG_RELEASE:=1
PPECFG_BUILD_DIR:=$(TOPDIR)/qca/src/nss-userspace-oss/ppe

define Package/ppecfg
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Userspace utility for configuring PPE functions such as ACL and Policers
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +libpthread +libnl +kmod-qca-nss-ppe-netlink +libnl-ppe +libjson-c
endef

#TODO: Remove dependency on ppe-netlink module

define Package/ppecfg/description
Userspace utility for configuring PPE functions such as ACL and Policers
endef

define Build/Prepare
	$(CP) $(PPECFG_BUILD_DIR)/ppecfg/*  $(PKG_BUILD_DIR)
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

define Package/ppecfg/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/obj/ppecfg $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/ppecfg
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ppecfg_acl_config.json $(1)/etc/ppecfg/ppecfg_acl_config.json
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ppecfg_policer_config.json $(1)/etc/ppecfg/ppecfg_policer_config.json
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcast_policer_config.json $(1)/etc/ppecfg/policer_default_cfg.json

ifeq ($(CONFIG_TARGET_ipq95xx),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcast_acl_ipq95xx_config.json $(1)/etc/ppecfg/acl_default_cfg.json
endif
ifeq ($(CONFIG_TARGET_ipq53xx),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcast_acl_ipq53xx_config.json $(1)/etc/ppecfg/acl_default_cfg.json
endif
ifeq ($(CONFIG_TARGET_ipq54xx),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcast_acl_ipq54xx_config.json $(1)/etc/ppecfg/acl_default_cfg.json
endif

endef

$(eval $(call BuildPackage,ppecfg))
