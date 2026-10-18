
include $(TOPDIR)/rules.mk

PKG:=qca-wifi-scripts
PKG_NAME:=$(PKG)
PKG_VERSION:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)

EXTERNAL_FILE_DIR:=$(TOPDIR)/openwrt-patches/package/kernel/mac80211
EXTERNAL_HOSTAP_FILE_DIR:=$(TOPDIR)/openwrt-patches/package/network/services/hostapd/

include $(INCLUDE_DIR)/package.mk
define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:= QCA wifi scripts for kernel5.4
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
endef

define Package/ath-wifi-scripts
  SECTION:=QCA
  CATEGORY:=QCA software
  TITLE:= ATH wifi scripts for kernel6.6
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
endef


define Package/ath-wifi-scripts/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/etc/hotplug.d/ieee80211
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/etc/hotplug.d/devcoredump
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/lib/netifd/wireless
	$(INSTALL_DIR) $(1)/lib/wifi/sawf
	$(INSTALL_DIR) $(1)/lib/wifi/sawf/telemetry
	$(INSTALL_DATA) $(EXTERNAL_FILE_DIR)/files/lib/wifi/def_service_classes.json $(1)/lib/wifi/sawf/
	$(INSTALL_DATA) $(EXTERNAL_FILE_DIR)/files/lib/wifi/service_classes.json $(1)/lib/wifi/sawf/
	$(INSTALL_DATA) $(EXTERNAL_FILE_DIR)/files/lib/wifi/telemetry/config.json $(1)/lib/wifi/sawf/telemetry/
	$(INSTALL_DATA) $(EXTERNAL_FILE_DIR)/files/lib/wifi/telemetry/sla.json $(1)/lib/wifi/sawf/telemetry/
	$(INSTALL_DATA) $(EXTERNAL_FILE_DIR)/files/lib/wifi/telemetry/sla_detect.json $(1)/lib/wifi/sawf/telemetry/
ifneq ($(wildcard $(EXTERNAL_FILE_DIR)/files/lib/wifi/pre_post_wifi_operations.sh),)
	$(INSTALL_DATA) $(EXTERNAL_FILE_DIR)/files/lib/wifi/pre_post_wifi_operations.sh $(1)/lib/wifi/
endif
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/performance.sh $(1)/lib
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/smp_affinity_settings.sh $(1)/lib
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/boost_performance.sh $(1)/lib
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/ds_enable.sh $(1)/lib
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/apsta_mode.sh $(1)/lib
ifeq ($(CONFIG_KERNEL_ATHMEMDEBUG),y)
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/memleak_check.sh $(1)/lib
endif
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/coredump.sh $(1)/etc/hotplug.d/devcoredump
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/wifi-config.sh $(1)/etc/hotplug.d/ieee80211/01-wifi-detect
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/etc/init.d/ath11k_nss_enable.sh $(1)/etc/init.d
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/etc/init.d/ath11k_uboot_mod_params.sh $(1)/etc/init.d
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/etc/init.d/qca-nss-pbuf $(1)/etc/init.d
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/etc/init.d/ath12k_dyn_dbg_enable.sh $(1)/etc/init.d
	$(INSTALL_DATA) $(EXTERNAL_HOSTAP_FILE_DIR)/files/dpp-hostapd-event-update.sh $(1)/lib/netifd/dpp-hostapd-event-update
	chmod 0755 $(1)/lib/netifd/dpp-hostapd-event-update
	$(INSTALL_DATA) $(EXTERNAL_HOSTAP_FILE_DIR)/files/dpp-supplicant-event-update.sh $(1)/lib/netifd/dpp-supplicant-event-update
	chmod 0755 $(1)/lib/netifd/dpp-supplicant-event-update
endef

define Package/ath-legacy-wifi-scripts
  SECTION:=QCA
  CATEGORY:=QCA software
  TITLE:= ATH legacy wifi scripts for kernel6.6
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
endef

define Package/ath-legacy-wifi-scripts/install
	$(INSTALL_DIR) $(1)/lib/wifi/
	$(INSTALL_DIR) $(1)/lib/netifd/
	$(INSTALL_DIR) $(1)/lib/netifd/wireless/
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/wifi/mac80211.sh $(1)/lib/wifi/mac80211.sh
	$(INSTALL_BIN) $(EXTERNAL_FILE_DIR)/files/lib/netifd-wlan/wireless/mac80211.sh $(1)/lib/netifd/wireless/
	$(INSTALL_DATA) $(EXTERNAL_HOSTAP_FILE_DIR)/files/hostapd.sh $(1)/lib/netifd/hostapd.sh
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/lib/preinit
	$(INSTALL_DIR) $(1)/etc/init.d/
ifeq ($(strip ${CONFIG_LOWMEM_FLASH}),y)
	$(INSTALL_DATA) ./files/read_caldata_to_fs_16m.sh $(1)/lib/read_caldata_to_fs.sh
else
	$(INSTALL_DATA) ./files/read_caldata_to_fs.sh $(1)/lib/
endif
	$(INSTALL_DATA) ./files/create_cfg_caldata.sh $(1)/lib/
	$(INSTALL_DATA) ./files/wifi_interface_helper.sh $(1)/lib
	$(INSTALL_DATA) ./files/update_smp_affinity.sh $(1)/lib
endef
Build/Compile=true
$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,ath-wifi-scripts))
$(eval $(call BuildPackage,ath-legacy-wifi-scripts))
