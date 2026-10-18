include $(TOPDIR)/rules.mk

PKG_NAME:=apps-scripts
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/$(PKG_NAME)
  SECTION:=OPTIC_APPS
  CATEGORY:=OptiCore Packages
  SUBMENU:=Applications
  TITLE:=base files
  DEPENDS:=
endef

define Package/$(PKG_NAME)/description
  Base files
endef

define Build/Prepare

endef


define Build/Compile

endef

define Package/$(PKG_NAME)/install
	@echo ""
	@echo "###############install file system $(1)#################"
	#[ ! -d $(1)/etc ] && $(INSTALL_DIR) $(1)/etc
	#[ ! -d $(1)/usr ] && $(INSTALL_DIR) $(1)/usr
	#[ ! -d $(1)/usr/sbin ] && $(INSTALL_DIR) $(1)/usr/sbin
	#[ ! -d $(1)/nvram ] && $(INSTALL_DIR) $(1)/nvram
	#[ ! -d $(1)/etc/init.d ] && $(INSTALL_DIR) $(1)/etc/init.d
	#[ ! -d $(1)/etc/pots ] && $(INSTALL_DIR) $(1)/etc/pots
	#[ ! -d $(1)/etc/omci ] && $(INSTALL_DIR) $(1)/etc/omci
	#$(INSTALL_BIN) files/etc/omci_done $(1)/etc/init.d/
	#cp -rf files/etc/pots/* $(1)/etc/pots
	#cp -rf files/etc/omci/* $(1)/etc/omci
	#cp -rf files/etc/init.d/* $(1)/etc/init.d
	#cp -rf files/usr/sbin/* $(1)/usr/sbin
endef

$(eval $(call BuildPackage,apps-scripts))
