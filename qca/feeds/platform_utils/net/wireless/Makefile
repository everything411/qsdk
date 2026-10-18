include $(TOPDIR)/rules.mk


PKG_NAME:=wififw_mount_script
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/wififw_mount_script
  TITLE:=QCA wififw_mount_script
  SECTION:=firmware mount
  CATEGORY:= QCA firmware mount
  DEPENDS:=@TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq50xx||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx

endef

define Package/wififw_mount_script/description
  This package contains a  wifi fw mount  script which can be used
  to mount WIFIFE partition in to RFS.
endef

Build/Compile=true

define Package/wififw_mount_script/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/wifi_fw_mount $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/wifi_fw_done $(1)/etc/init.d
endef

$(eval $(call BuildPackage,wififw_mount_script))
