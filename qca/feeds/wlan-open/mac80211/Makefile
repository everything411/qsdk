#
# Copyright (C) 2007-2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

include mac80211_package.mk

PKG_NAME:=$(MAC80211_PKG_NAME)

PKG_VERSION:=6.6.15
PKG_RELEASE:=2
PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=COPYING

PKG_BACKPORTS_VERSION:=965f73fc
PKG_BACKPORTS_SOURCE_URL:=git://git.kernel.org/pub/scm/linux/kernel/git/backports/backports.git
PKG_BACKPORTS_BRANCH:=master

PKG_KERNEL_VERSION:=$(MAC80211_PKG_KERNEL_VERSION)
PKG_KERNEL_SOURCE_URL:=http://git.codelinaro.org/clo/qsdk/kvalo/ath.git
PKG_KERNEL_BRANCH:=korg-kvalo/master

PKG_VERSION:=$(MAC80211_PKG_VERSION)
PKG_SOURCE:=$(MAC80211_PKG_SOURCE)
PKG_SUBDIR:=$(MAC80211_PKG_SUBDIR)
PKG_RELEASE:=1
PKG_BUILD_ID:=1

PKG_BUILD_DIR:=$(MAC80211_PKG_BUILD_DIR)/backports-6.1-$(MAC80211_PKG_KERNEL_VERSION)
PKG_BUILD_PARALLEL:=1

MKHASH ?= $(STAGING_DIR_HOST)/bin/mkhash
PKG_BUILD_ID:=$(shell date | $(MKHASH) md5)

ifeq (y, $(filter y, $(CONFIG_TARGET_echo) $(CONFIG_TARGET_sdx85)))
	SRCPREFIX:=
else
	SRCPREFIX:="qca/"
endif

WLAN_SRCPREFIX:=$(shell if [ -d $(TOPDIR)/src/ipq ]; then echo "src/ipq/"; else echo "qca/src/"; fi)

LOCAL_SRC:=$(TOPDIR)/$(WLAN_SRCPREFIX)mac80211/wlan-open/backports-6.1-$(MAC80211_PKG_KERNEL_VERSION)

ifeq ($(CONFIG_TARGET_sdx85),y)
	EXTERNAL_HOSTAP_FILE_DIR:=$(TOPDIR)/owrt-qti-ipq-open/feeds/hostapd/priv_patches/
else ifeq ($(CONFIG_TARGET_echo),y)
	EXTERNAL_HOSTAP_FILE_DIR:=$(TOPDIR)/owrt-qti-ipq-open/feeds/wlan-hostapd/hostapd/
else ifeq ($(CONFIG_USE_PRPLMESH_WHM),y)
	EXTERNAL_HOSTAP_FILE_DIR:=$(TOPDIR)/feed-qca/qca/feeds/wlan-hostapd/hostapd/
else
	EXTERNAL_HOSTAP_FILE_DIR:=$(TOPDIR)/qca/feeds/wlan-hostapd/hostapd/
endif

ifeq ($(CONFIG_TARGET_sdx85),y)
PKG_DRIVERS = \
	cfg80211 mac80211 mac80211-hwsim
else
PKG_DRIVERS = \
	mac80211-hwsim
endif

PKG_CONFIG_DEPENDS:= \
	CONFIG_PACKAGE_kmod-mac80211 \
	CONFIG_PACKAGE_CFG80211_TESTMODE \
	CONFIG_PACKAGE_MAC80211_MESSAGE_TRACING \
	CONFIG_PACKAGE_MAC80211_DEBUGFS \
	CONFIG_PACKAGE_MAC80211_MESH \
	CONFIG_PACKAGE_MAC80211_TRACING \
	CONFIG_PACKAGE_IWLWIFI_DEBUG \
	CONFIG_PACKAGE_IWLWIFI_DEBUGFS \
	CONFIG_PACKAGE_RTLWIFI_DEBUG \
	CONFIG_PACKAGE_QCN_EXTN \
	CONFIG_PACKAGE_QCA_LAB_TEST_FEATURES \
	CONFIG_PACKAGE_QCN_EXTN_MESH_SUPPORT \
	CONFIG_PACKAGE_ATH_UCAST_ENABLE_AST_OVERRIDE \

include $(INCLUDE_DIR)/package.mk

WMENU:=Wireless Drivers

define KernelPackage/mac80211/Default
  SUBMENU:=$(WMENU)
  URL:=https://wireless.wiki.kernel.org/
  MAINTAINER:=Felix Fietkau <nbd@nbd.name>
endef

# config_pacakge:
# 1 = OpenWrt KernelPackage name
# 2 = Optional list of variants for which the module may be selected:
#     If empty, the module is only selected with the first variant
#     Use $(ALL_VARIANTS) to allow module selection in all variants
config_package=$(if $(and $(CONFIG_PACKAGE_kmod-$(1)),$(call mac80211_variant_check,$(2))),m)
mac80211_variant_check=$(if $(ALL_VARIANTS),$(filter $(BUILD_VARIANT),$(if $(1),$(1),$(firstword $(ALL_VARIANTS)))),y)

config-y:= \
	WLAN \
	NL80211_TESTMODE \
	CFG80211_CERTIFICATION_ONUS \
	MAC80211_RC_MINSTREL \
	MAC80211_RC_MINSTREL_HT \
	MAC80211_RC_MINSTREL_VHT \
	MAC80211_RC_DEFAULT_MINSTREL \
	WLAN_VENDOR_ADMTEK \
	WLAN_VENDOR_ATH \
	WLAN_VENDOR_RSI \

config-$(call config_package,cfg80211,$(ALL_VARIANTS)) += CFG80211
config-$(CONFIG_PACKAGE_CFG80211_TESTMODE) += NL80211_TESTMODE

config-$(call config_package,mac80211,$(ALL_VARIANTS)) += MAC80211
config-$(CONFIG_PACKAGE_MAC80211_MESH) += MAC80211_MESH
config-$(CONFIG_PACKAGE_kmod-mac80211) += MAC80211_DEBUG_MENU MAC80211_STA_DEBUG MAC80211_MLME_DEBUG
include ath.mk

PKG_CONFIG_DEPENDS += \
	$(patsubst %,CONFIG_PACKAGE_kmod-%,$(PKG_DRIVERS))

define KernelPackage/cfg80211
  $(call KernelPackage/mac80211/Default)
  TITLE:=cfg80211 - wireless configuration API
  DEPENDS+= +iw-full +iwinfo +!USE_PRPLMESH_WHM:wifi-scripts +wireless-regdb +USE_RFKILL:kmod-rfkill
  DEPENDS+= +PACKAGE_kmod-qca-debug-uio:kmod-qca-debug-uio
  ABI_VERSION:=$(PKG_VERSION)-$(PKG_RELEASE)
  FILES:= \
	$(PKG_BUILD_DIR)/compat/compat.ko \
	$(PKG_BUILD_DIR)/net/wireless/cfg80211.ko
ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
  FILES+= $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath_debug/ath_debug.ko
endif
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
ifeq ($(CONFIG_USE_PRPLMESH_WHM),y)
	sed -i 's/OpenWrt/prplOS/g' ./files/lib/wifi/mac80211.sh
endif
	$(INSTALL_BIN) ./files/lib/wifi/mac80211.sh $(1)/lib/wifi/mac80211.sh
	$(INSTALL_BIN) ./files/lib/netifd-wlan/wireless/mac80211.sh $(1)/lib/netifd/wireless/
	$(INSTALL_DATA) $(EXTERNAL_HOSTAP_FILE_DIR)/files/hostapd.sh $(1)/lib/netifd/hostapd.sh
endef

define KernelPackage/cfg80211/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/etc/hotplug.d/ieee80211
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/etc/hotplug.d/devcoredump
	$(INSTALL_DIR) $(1)/lib/wifi $(1)/lib/netifd/wireless
	$(INSTALL_DIR) $(1)/lib/wifi/sawf
	$(INSTALL_DIR) $(1)/lib/wifi/sawf/telemetry
	$(INSTALL_DIR) $(1)/ini
	$(INSTALL_DIR) $(1)/ini/internal
	$(INSTALL_DATA) ./files/lib/wifi/def_service_classes.json $(1)/lib/wifi/sawf/
	$(INSTALL_DATA) ./files/lib/wifi/service_classes.json $(1)/lib/wifi/sawf/
	$(INSTALL_DATA) ./files/lib/wifi/telemetry/config.json $(1)/lib/wifi/sawf/telemetry/
	$(INSTALL_DATA) ./files/lib/wifi/telemetry/sla.json $(1)/lib/wifi/sawf/telemetry/
	$(INSTALL_DATA) ./files/lib/wifi/telemetry/sla_detect.json $(1)/lib/wifi/sawf/telemetry/
ifeq ($(CONFIG_KERNEL_FTRACE),y)
	$(INSTALL_DATA) ./files/lib/ftrace_enable_events.sh $(1)/lib
endif
ifneq ($(wildcard ./files/lib/wifi/pre_post_wifi_operations.sh),)
	$(INSTALL_DATA) ./files/lib/wifi/pre_post_wifi_operations.sh $(1)/lib/wifi/
endif
	$(INSTALL_BIN) ./files/lib/performance.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/smp_affinity_settings.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/boost_performance.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/mandatory_dp_stats.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/ds_enable.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/apsta_mode.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/kpi_stats.sh $(1)/lib
	$(INSTALL_BIN) ./files/lib/vow_stats.sh $(1)/lib
ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
	$(INSTALL_DIR) $(1)/lib/wifi/mount
	$(INSTALL_BIN) ./files/lib/wifi/mount/umount_fw_partition.sh $(1)/lib/wifi/mount/
	$(INSTALL_BIN) ./files/lib/wifi/mount/mount_fw_partition.sh $(1)/lib/wifi/mount/
endif
ifeq ($(CONFIG_KERNEL_ATHMEMDEBUG),y)
	$(INSTALL_BIN) ./files/lib/memleak_check.sh $(1)/lib
endif

ifneq ($(CONFIG_TARGET_sdx85),y)
	$(INSTALL_BIN) ./files/coredump.sh $(1)/etc/hotplug.d/devcoredump
endif
	$(INSTALL_BIN) ./files/lib/wifi-config.sh $(1)/etc/hotplug.d/ieee80211/01-wifi-detect
	$(INSTALL_BIN) ./files/etc/init.d/ath11k_nss_enable.sh $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/ath11k_uboot_mod_params.sh $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/qca-nss-pbuf $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/diag_socket_app $(1)/etc/init.d
	$(INSTALL_DATA) $(EXTERNAL_HOSTAP_FILE_DIR)/files/dpp-hostapd-event-update.sh $(1)/lib/netifd/dpp-hostapd-event-update
	chmod 0755 $(1)/lib/netifd/dpp-hostapd-event-update
	$(INSTALL_DATA) $(EXTERNAL_HOSTAP_FILE_DIR)/files/dpp-supplicant-event-update.sh $(1)/lib/netifd/dpp-supplicant-event-update
	chmod 0755 $(1)/lib/netifd/dpp-supplicant-event-update
	$(INSTALL_BIN) ./files/etc/init.d/ath12k_dyn_dbg_enable.sh $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/ath12k_dyn_module_add.sh $(1)/etc/init.d
	$(INSTALL_CONF) ./files/ini/*.ini $(1)/ini/
	$(INSTALL_CONF) ./files/ini/internal/*.ini $(1)/ini/internal/
ifeq (y, $(filter y, $(CONFIG_TARGET_echo) $(CONFIG_TARGET_sdx85)))
	$(INSTALL_DIR) $(1)/lib/modules/$(UNAME_VERSION)
	$(SIGN_KEY) $(PKG_BUILD_DIR)/compat/compat.ko
	$(SIGN_KEY) $(PKG_BUILD_DIR)/net/wireless/cfg80211.ko
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/compat/compat.ko $(1)/lib/modules/$(UNAME_VERSION)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/net/wireless/cfg80211.ko $(1)/lib/modules/$(UNAME_VERSION)
	# Export compat and cfg80211 to bin/targets
	$(INSTALL_DIR) $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/compat/compat.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/net/wireless/cfg80211.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
endif
ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
 ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
	if [ -f "$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/hostdrv_struct_layout.txt.lzma" ]; then \
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/hostdrv_struct_layout.txt.lzma $(1)/lib/wifi/; \
	fi
 endif
endif
endef

define KernelPackage/cfg80211/description
cfg80211 is the Linux wireless LAN (802.11) configuration API.
endef

define KernelPackage/cfg80211/config
  if PACKAGE_kmod-cfg80211

	config PACKAGE_CFG80211_TESTMODE
		bool "Enable testmode command support"
		default n
		help
		  This is typically used for tests and calibration during
		  manufacturing, or vendor specific debugging features

  endif
endef

define KernelPackage/mac80211
  $(call KernelPackage/mac80211/Default)
  TITLE:=Linux 802.11 Wireless Networking Stack
  # +kmod-crypto-cmac is a runtime only dependency of net/mac80211/aes_cmac.c
ifeq (y, $(filter y, $(CONFIG_TARGET_echo) $(CONFIG_TARGET_sdx85)))
  DEPENDS+= +kmod-cfg80211 +kmod-crypto-cmac +kmod-crypto-ccm +kmod-crypto-gcm +hostapd-common
else
  DEPENDS+= +kmod-cfg80211 +kmod-crypto-cmac +kmod-crypto-ccm +kmod-crypto-gcm +hostapd-common \
	+kmod-qca-nss-ppe \
	+kmod-qca-nss-ppe-vp \
	+kmod-qca-nss-ppe-ds \
	+kmod-qca-nss-wifi-plugins
endif
  KCONFIG:=\
	CONFIG_AVERAGE=y \
	CONFIG_QCOM_RPROC_DISABLE_MPD_SUPPORT=y

  FILES:= $(PKG_BUILD_DIR)/net/mac80211/mac80211.ko
  ABI_VERSION:=$(PKG_VERSION)-$(PKG_RELEASE)
  MENU:=1
endef

define KernelPackage/mac80211/config
  if PACKAGE_kmod-mac80211

        if PACKAGE_kmod-qca-nss-ppe-vp
	   config PACKAGE_MAC80211_PPE_SUPPORT
		   bool "Enable PPE support for IPQ platform"
		   default y
		   help
		     This option enables support for PPE in QCA boards
        endif

        if PACKAGE_kmod-qca-nss-ppe-ds
	   config PACKAGE_MAC80211_DS_SUPPORT
		   bool "Enable DS support for IPQ platform"
		   default y
		   help
		     This option enables support for ds netdev in QCA boards.
        endif

        if PACKAGE_kmod-qca-nss-sfe
	   config PACKAGE_MAC80211_SFE_SUPPORT
		   bool "Enable sfe support for IPQ platform"
		   default y
		   help
		     This option enables support for qca sfe in QCA boards
        endif

	config PACKAGE_MAC80211_ATHMEMDEBUG
		bool "Enable memory debug support"
		default n
		help
		  This option enables ATH memory debug support.

	config PACKAGE_MAC80211_DEBUGFS
		bool "Export mac80211 internals in DebugFS"
		select KERNEL_DEBUG_FS
		default y
		help
		  Select this to see extensive information about
		  the internal state of mac80211 in debugfs.

	config PACKAGE_MAC80211_TRACING
		bool "Enable tracing (mac80211 and supported drivers)"
		select KERNEL_FTRACE
		select KERNEL_ENABLE_DEFAULT_TRACERS
		default n
		help
		  Select this to enable tracing of mac80211 and
		  related wifi drivers (using trace-cmd).

	config PACKAGE_MAC80211_MESH
		bool "Enable 802.11s mesh support"
		default y

	config PACKAGE_ATHDEBUG
		bool "Enable athdebug module support"
		default y
		help
		  This option enables ATHDEBUG module support.

	config PACKAGE_QCN_EXTN
		bool "Enable QCN extensions support"
		default y

	config PACKAGE_QCA_LAB_TEST_FEATURES
		bool "Enable QCA lab test features"
		depends on PACKAGE_QCN_EXTN
		default y

	config PACKAGE_QCN_EXTN_MESH_SUPPORT
		bool "Enable QCN extension mesh feature support"
		default y

	config PACKAGE_ATH_UCAST_ENABLE_AST_OVERRIDE
		bool "Enable unicast AST override"
		default n

  endif
endef

define KernelPackage/mac80211/description
Generic IEEE 802.11 Networking Stack (mac80211)
endef

define KernelPackage/mac80211-hwsim
  $(call KernelPackage/mac80211/Default)
  TITLE:=mac80211 HW simulation device
  DEPENDS+= +kmod-mac80211 +@DRIVER_11AX_SUPPORT +@DRIVER_11AC_SUPPORT
  FILES:=$(PKG_BUILD_DIR)/drivers/net/wireless/virtual/mac80211_hwsim.ko
  AUTOLOAD:=$(call AutoProbe,mac80211_hwsim)
endef

ifeq (y, $(filter y, $(CONFIG_TARGET_echo) $(CONFIG_TARGET_sdx85)))
define KernelPackage/mac80211/install
	$(INSTALL_DIR) $(1)/lib/modules/$(UNAME_VERSION)
	$(SIGN_KEY) $(PKG_BUILD_DIR)/net/mac80211/mac80211.ko
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/net/mac80211/mac80211.ko $(1)/lib/modules/$(UNAME_VERSION)
	# Export mac80211.ko to bin/targets
	$(INSTALL_DIR) $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/net/mac80211/mac80211.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
endef

define KernelPackage/ath12k/install
	$(INSTALL_DIR) $(1)/lib/modules/$(UNAME_VERSION)
	$(SIGN_KEY) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath12k.ko
	$(SIGN_KEY) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/ath12k_wifi7.ko
	$(SIGN_KEY) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi8/ath12k_wifi8.ko
ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
	$(SIGN_KEY) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath_debug/ath_debug.ko
endif
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath12k.ko $(1)/lib/modules/$(UNAME_VERSION)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/ath12k_wifi7.ko $(1)/lib/modules/$(UNAME_VERSION)
ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath_debug/ath_debug.ko $(1)/lib/modules/$(UNAME_VERSION)
endif
	$(INSTALL_DIR) $(1)/lib/firmware
ifeq ($(CONFIG_TARGET_sdx85),y)
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX
	ln -sf /firmware/image/qcn9224 $(1)/lib/firmware
	ln -sf /firmware/image/qcn9224 $(1)/lib/firmware/ath12k/QCN92XX/hw1.0
else ifeq ($(CONFIG_TARGET_echo),y)
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN9625
	ln -sf /firmware/image/qcn9625 $(1)/lib/firmware
	ln -sf /firmware/image/qcn9625 $(1)/lib/firmware/ath12k/QCN9625/hw1.0
endif
	# Export ath12k modules to bin/targets
	$(INSTALL_DIR) $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath12k.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/ath12k_wifi7.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi8/ath12k_wifi8.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath_debug/ath_debug.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
endif
endef
endif

define KernelPackage/wlcore
  $(call KernelPackage/mac80211/Default)
  TITLE:=TI common driver part
  DEPENDS+= +kmod-mmc +kmod-mac80211
  FILES:= \
	$(PKG_BUILD_DIR)/drivers/net/wireless/ti/wlcore/wlcore.ko \
	$(PKG_BUILD_DIR)/drivers/net/wireless/ti/wlcore/wlcore_sdio.ko
  AUTOLOAD:=$(call AutoProbe,wlcore wlcore_sdio)
endef

ifdef CONFIG_PACKAGE_MAC80211_DEBUGFS
  config-y += \
	CFG80211_DEBUGFS \
	MAC80211_DEBUGFS
endif

ifdef CONFIG_PACKAGE_MAC80211_TRACING
  config-y += \
	IWLWIFI_DEVICE_TRACING
endif

config-$(CONFIG_PACKAGE_MAC80211_PPE_SUPPORT) += MAC80211_PPE_SUPPORT
config-$(CONFIG_PACKAGE_MAC80211_DS_SUPPORT) += MAC80211_DS_SUPPORT ATH12K_PPE_DS_SUPPORT
config-$(CONFIG_PACKAGE_MAC80211_SFE_SUPPORT) += MAC80211_SFE_SUPPORT
config-$(CONFIG_PACKAGE_MAC80211_MESSAGE_TRACING) += MAC80211_MESSAGE_TRACING ATH10K_TRACING ATH11K_TRACING ATH12K_TRACING
config-$(CONFIG_PACKAGE_MAC80211_DEBUG_MENU) += MAC80211_DEBUG_MENU
config-$(CONFIG_PACKAGE_MAC80211_VERBOSE_DEBUG) += MAC80211_VERBOSE_DEBUG
config-$(CONFIG_PACKAGE_MAC80211_PS_DEBUG) += MAC80211_PS_DEBUG
config-$(CONFIG_PACKAGE_MAC80211_ATHMEMDEBUG) += MAC80211_ATHMEMDEBUG
config-$(CONFIG_PACKAGE_QCN_EXTN) += QCN_EXTN
config-$(CONFIG_PACKAGE_QCA_LAB_TEST_FEATURES) += QCA_LAB_TEST_FEATURES
config-$(CONFIG_PACKAGE_QCN_EXTN_MESH_SUPPORT) += QCN_EXTN_MESH_SUPPORT

config-$(call config_package,mac80211-hwsim) += MAC80211_HWSIM

config-$(CONFIG_PACKAGE_ATHDEBUG) += ATHDEBUG

config-y += WL_TI WILINK_PLATFORM_DATA

config-$(CONFIG_LEDS_TRIGGERS) += MAC80211_LEDS

C_DEFINES=

ifeq ($(BUILD_VARIANT),smallbuffers)
	C_DEFINES+= -DCONFIG_ATH10K_SMALLBUFFERS
endif

CMA_TARGETS := $(CONFIG_TARGET_ipq96xx) \
	       $(CONFIG_TARGET_ipq52xx)

ifeq ($(filter y,$(CMA_TARGETS)),y)
ifeq ($(CONFIG_DMA_CMA),y)
	C_DEFINES+= -DATH12K_CMA_SUPPORT
endif
endif

ifeq ($(CONFIG_TARGET_sdx85),y)
ifeq ($(CONFIG_PACKAGE_EXT_IPA_OFFLOAD),y)
  C_DEFINES+= -DCPTCFG_EXT_IPA_OFFLOAD
endif
  LINUX_HDRS=$(TOPDIR)/src/kernel-$(LINUX_VERSION)/kernel_platform/temp_out_dir/msm-kernel
  EXTRA_MAKE_CFLAGS="-I$(PKG_BUILD_DIR)/include $(IREMAP_CFLAGS) $(C_DEFINES) -I$(TOPDIR)/$(WLAN_SRCPREFIX)qca-wifi/telemetry_agent/inc/ -Wall -DPLATFORM_SDX -Wno-unused-but-set-variable -Wno-int-in-bool-context -Wno-pointer-bool-conversion -Wno-tautological-constant-out-of-range-compare -Wno-unused-const-variable -Wno-sometimes-uninitialized -Wno-logical-not-parentheses -Wno-uninitialized -I$(PKG_BUILD_DIR)/../dataipa-1.0/drivers/platform/msm/include -I$(PKG_BUILD_DIR)/../dataipa-1.0/drivers/platform/msm/include/uapi"

  KBUILD_EXTRA_SYMBOLS_IPA="$(PKG_BUILD_DIR)/../dataipa-1.0/Module.symvers"
  MAKE_OPTS:= \
	-C $(LINUX_DIR) M="$(PKG_BUILD_DIR)" \
	KBUILD_EXTRA_SYMBOLS=$(KBUILD_EXTRA_SYMBOLS_IPA) \
	EXTRA_CFLAGS=$(EXTRA_MAKE_CFLAGS)
	KLIB_BUILD=$(LINUX_HDRS)

define Build/PreCompile
	echo "Pushing KLIB_BUILD before compilation"
	grep -q 'KLIB_BUILD=' $(PKG_BUILD_DIR)/Makefile.build || \
		sed -i '/export/i\\KLIB_BUILD=$(TOPDIR)/src/kernel-$(LINUX_VERSION)/kernel_platform/msm-kernel' $(PKG_BUILD_DIR)/Makefile.build
endef

else
  ifeq ($(CONFIG_TARGET_echo),y)
  EXTRA_MAKE_CFLAGS="-I$(PKG_BUILD_DIR)/include $(IREMAP_CFLAGS) $(C_DEFINES) -I$(TOPDIR)/$(WLAN_SRCPREFIX)qca-wifi/telemetry_agent/inc/ -Wall"
   MAKE_OPTS:= \
	$(subst -C $(LINUX_DIR),-C "$(PKG_BUILD_DIR)",$(KERNEL_MAKEOPTS)) \
	EXTRA_CFLAGS=$(EXTRA_MAKE_CFLAGS) \
	KLIB_BUILD="$(LINUX_DIR)" \
	MODPROBE=true \
	KLIB=$(TARGET_MODULES_DIR) \
	KERNEL_SUBLEVEL=$(lastword $(subst ., ,$(KERNEL_PATCHVER))) \
	KBUILD_LDFLAGS_MODULE_PREREQ=
	CFLAGS="-DPLATFORM_SDX"
  else
  EXTRA_MAKE_CFLAGS="-I$(PKG_BUILD_DIR)/include $(IREMAP_CFLAGS) $(C_DEFINES) -I$(STAGING_DIR)/usr/include/qca-nss-drv -I$(STAGING_DIR)/usr/include/qca-nss-ppe -I$(STAGING_DIR)/usr/include/qca-nss-clients -I$(TOPDIR)/$(WLAN_SRCPREFIX)qca-wifi/telemetry_agent/inc/ -Wall"

  MAKE_OPTS:= \
	$(subst -C $(LINUX_DIR),-C "$(PKG_BUILD_DIR)",$(KERNEL_MAKEOPTS)) \
	EXTRA_CFLAGS=$(EXTRA_MAKE_CFLAGS) \
	KLIB_BUILD="$(LINUX_DIR)" \
	MODPROBE=true \
	KLIB=$(TARGET_MODULES_DIR) \
	KERNEL_SUBLEVEL=$(lastword $(subst ., ,$(KERNEL_PATCHVER))) \
	KBUILD_LDFLAGS_MODULE_PREREQ=
  endif
endif

define ConfigVars
$(subst $(space),,$(foreach opt,$(config-$(1)),CPTCFG_$(opt)=$(1)
))
endef

define mac80211_config
$(call ConfigVars,m)$(call ConfigVars,y)
endef
$(eval $(call shexport,mac80211_config))

ifeq ($(CONFIG_TARGET_sdx85),y)
  SPATCH?=0
else
  SPATCH?=1
endif

define Build/Prepare
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	cp -rf $(LOCAL_SRC)/* $(PKG_BUILD_DIR)
ifdef CONFIG_PACKAGE_QCN_EXTN
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/subsys/src $(PKG_BUILD_DIR)/net/mac80211/qcn_extns
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/ath/ath12k/src $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qcn_extns
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/ath/wifi7/src $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/qcn_extns
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/ath/wifi6/src $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qcn_extns/wifi6
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/ath/wifi8/src $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi8/qcn_extns
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/subsys/src/cfg80211_dfs_extn.c $(PKG_BUILD_DIR)/net/wireless/cfg80211_dfs_extn.c
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/subsys/src/cfg80211_dfs_extn.h $(PKG_BUILD_DIR)/net/wireless/cfg80211_dfs_extn.h
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/subsys/src/cfg80211_scan_radio_extn.c $(PKG_BUILD_DIR)/net/wireless/cfg80211_scan_radio_extn.c
	$(CP) $(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/subsys/src/cfg80211_scan_radio_extn.h $(PKG_BUILD_DIR)/net/wireless/cfg80211_scan_radio_extn.h
endif
	$(CP) $(STAGING_DIR)/usr/include/qca-vendor.h $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qca-vendor.h

ifneq ($(CONFIG_DEBUG_MEM_USAGE),y)
 ifneq ($(CONFIG_PACKAGE_MAC80211_ATHMEMDEBUG),y)
  ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
   ifeq ($(CONFIG_QCA_MINIDUMP),y)
    ifeq ($(SPATCH),1)
	$(Build/refactor)
    endif
   endif
  endif
 endif
endif

endef

ifneq ($(CONFIG_PACKAGE_kmod-cfg80211),)
 define Build/Compile/kmod
	rm -rf $(PKG_BUILD_DIR)/modules
ifeq ($(CONFIG_TARGET_sdx85),y)
	$(call Build/PreCompile)
endif
	+$(MAKE) $(PKG_JOBS) $(MAKE_OPTS) modules
 endef
endif

EXTERNAL_PATCH_DIR:=$(TOPDIR)/openwrt-patches/package/kernel/mac80211/patches

define Quilt/Refresh/Package
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/build,build/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/subsys,subsys/)
ifdef CONFIG_PACKAGE_QCN_EXTN
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/subsys/patches,patches/)
endif
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath,ath/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath11k,ath11k/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR)/ath12k,ath12k/)
ifdef CONFIG_PACKAGE_QCN_EXTN
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/ath/ath12k/patches,patches/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(TOPDIR)/$(WLAN_SRCPREFIX)wlan-open-extns/ath/wifi7/patches,patches/)
endif
endef

define Build/refactor
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ahb.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ce.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/core.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/coredump.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/debugfs_htt_stats.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_htt.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_mon.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_peer.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dbring.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/debugfs.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/dp_rx.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/mac.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/peer.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qmi.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/reg.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wmi.c
	spatch -sp_file alloc.cocci --in-place $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wow.c
	spatch -cocci_file alloc.cocci --in-place -dir $(PKG_BUILD_DIR)/net/mac80211
	spatch -cocci_file alloc.cocci --in-place -dir $(PKG_BUILD_DIR)/net/wireless
	spatch -cocci_file alloc.cocci --in-place -dir $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7
	spatch -cocci_file alloc.cocci --in-place -dir $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi8
endef

define Build/Compile
	$(SH_FUNC) var2file "$(call shvar,mac80211_config)" $(PKG_BUILD_DIR)/.config
ifeq ($(CONFIG_TARGET_sdx85),y)
	$(MAKE) -C $(PKG_BUILD_DIR) KLIB_BUILD=$(LINUX_HDRS) allnoconfig
else
	$(MAKE) $(MAKE_OPTS) allnoconfig
endif
	$(call Build/Compile/kmod)
endef

define Build/InstallDev
	mkdir -p \
		$(1)/usr/include/mac80211 \
		$(1)/usr/include/mac80211-backport \
		$(1)/usr/include/mac80211/ath \
		$(1)/usr/include/net/mac80211
	$(CP) $(PKG_BUILD_DIR)/net/mac80211/*.h $(PKG_BUILD_DIR)/include/* $(1)/usr/include/mac80211/
	$(CP) $(PKG_BUILD_DIR)/backport-include/* $(1)/usr/include/mac80211-backport/
	$(CP) $(PKG_BUILD_DIR)/net/mac80211/rate.h $(1)/usr/include/net/mac80211/
	$(CP) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/*.h $(1)/usr/include/mac80211/ath/
	$(CP) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/qca-vendor.h $(1)/usr/include/mac80211/ath/
	rm -f $(1)/usr/include/mac80211-backport/linux/module.h

ifeq ($(CONFIG_PACKAGE_ATHDEBUG),y)
 ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
	if [ -f "$(TOPDIR)/qca/src/ath-tools/athstruct-parser/pahole" ]; then \
		$(TOPDIR)/qca/src/ath-tools/athstruct-parser/pahole \
			$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/ath12k.ko \
			$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi7/ath12k_wifi7.ko \
			$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi8/ath12k_wifi8.ko \
			$(PKG_BUILD_DIR)/net/wireless/cfg80211.ko \
			$(PKG_BUILD_DIR)/net/mac80211/mac80211.ko >> \
			$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/hostdrv_struct_layout.txt; \
		lzma e $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/hostdrv_struct_layout.txt \
			$(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/hostdrv_struct_layout.txt.lzma || true; \
		rm -f $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/hostdrv_struct_layout.txt; \
	fi
 endif
endif
endef

define KernelPackage/ath/install
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ath_sawf.h $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ath_fse.h $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ath_dp_accel_cfg.h $(STAGING_DIR)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/include/ath/ppe_public.h $(STAGING_DIR)/usr/include/
ifeq (y, $(filter y, $(CONFIG_TARGET_echo) $(CONFIG_TARGET_sdx85)))
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/ipa/wifi8/
	$(CP) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath12k/wifi8/qcn_extns/ipa/dp_ipa_fse.h $(STAGING_DIR)/usr/include/ipa/wifi8/
	$(INSTALL_DIR) $(1)/lib/modules/$(UNAME_VERSION)
	$(SIGN_KEY) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath.ko
ifeq ($(CONFIG_TARGET_sdx85),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/../dataipa-1.0/ipam.ko $(1)/lib/modules/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/../dataipa-1.0/gsim.ko $(1)/lib/modules/
else ifeq ($(CONFIG_TARGET_echo),y)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/../dataipa-1.0/drivers/platform/msm/ipa/ipam.ko $(1)/lib/modules/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/../dataipa-1.0/drivers/platform/msm/gsi/gsim.ko $(1)/lib/modules/
endif
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath.ko $(1)/lib/modules/$(UNAME_VERSION)

	# Export ath.ko to bin/targets
	$(INSTALL_DIR) $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/drivers/net/wireless/ath/ath.ko $(TOPDIR)/bin/targets/$(TARGET_VARIANT)/modules/
endif
endef

$(eval EXT_KERNEL_DIR:=$(CONFIG_EXTERNAL_KERNEL_TREE))
IS_EXT_KERNEL_TREE_GIT:=$(shell cd $(EXT_KERNEL_DIR) && git rev-parse --is-inside-work-tree)

ifeq ($(IS_EXT_KERNEL_TREE_GIT), true)
  $(eval _LINUX_SRC:=$(CONFIG_EXTERNAL_KERNEL_TREE))
else
  _LINUX_SRC=$(CONFIG_KERNEL_GIT_CLONE_URI)
endif

ifneq ($(call qstrip,$(_LINUX_SRC)),)
# --reference option doesn't work on git trees synced with "repo", so instead
# we're manually using clone & fetch to speed up sync time
  define FastCloneKernel
	git clone file://$(_LINUX_SRC) $(2) || \
	(rm -rf $(2) && \
	git clone file://$(_LINUX_SRC) $(2))
	(cd $(2); git remote add src $(1); git fetch src)
  endef
else
  define FastCloneKernel
	GIT_NAME=$$$$(echo $(1) | sed -e 's:.*//[^/]*.::g'); \
	git clone $(1) $(2) || \
	([ -n "${CONFIG_GIT_MIRROR}" ] && \
	rm -rf $(2) && \
	git clone $(CONFIG_GIT_MIRROR)$$$$GIT_NAME $(2))
  endef
endif


ifeq (,$(wildcard $(DL_DIR)/$(PKG_SOURCE)))
$(shell [ -f "$(NFS_MIRROR_SERVER)/$(PKG_SOURCE)" ] && \
	cp "$(NFS_MIRROR_SERVER)/$(PKG_SOURCE)" "$(TOPDIR)/dl/")
endif

$(eval $(foreach drv,$(PKG_DRIVERS),$(call KernelPackage,$(drv))))
ifneq ($(CONFIG_TARGET_sdx85),y)
$(eval $(call KernelPackage,cfg80211))
$(eval $(call KernelPackage,mac80211))
endif
$(eval $(call BuildPackage,ath-legacy-wifi-scripts))
