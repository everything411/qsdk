include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-wifi-nss-plugins
PKG_LICENSE:=ISC
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2

ifeq ($(CONFIG_TARGET_echo_cpe),y)
LOCAL_SRC:=$(TOPDIR)/src/wlan-open-extns/ath/plugin/qca-wifi-nss-plugins
include $(TOPDIR)/owrt-qti-ipq-open/include/local-development.mk
else
LOCAL_SRC:=$(TOPDIR)/qca/src/wlan-open-extns/ath/plugin/qca-wifi-nss-plugins
include $(INCLUDE_DIR)/local-development.mk
endif

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-wifi-nss-plugins
 SECTION:=kernel
 CATEGORY:=Kernel modules
 SUBMENU:=Network Devices
 TITLE:=Wifi Plugin module
ifeq ($(CONFIG_TARGET_echo_cpe),y)
 DEPENDS:=+PACKAGE_kmod-ath12k:kmod-ath12k
else
 DEPENDS:=+kmod-qca-nss-ecm +kmod-qca-nss-ppe \
	  +PACKAGE_kmod-qca-wifi-unified-profile:kmod-qca-wifi \
	  +PACKAGE_kmod-qca-wifi-lowmem-profile:kmod-qca-wifi \
	  +PACKAGE_kmod-ath12k:kmod-ath12k
endif
 FILES:=$(PKG_BUILD_DIR)/qca-wifi-nss-plugins.ko
endef

define KernelPackage/qca-wifi-nss-plugins/Description
Kernel module for wifi plugins.
endef

EXTRA_CFLAGS+= \
			  -I$(STAGING_DIR)/usr/include/qca-wifi \
			  -I$(STAGING_DIR)/usr/include/qca-nss-ecm \
			  -I$(STAGING_DIR)/usr/include/qca-nss-ppe \
			  -I$(STAGING_DIR)/usr/include/mac80211/uapi/linux

ifeq ($(CONFIG_TARGET_echo_cpe),y)
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/
endif

ifneq ($(CONFIG_PACKAGE_kmod-ath),)
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_OPEN_PROFILE_ENABLE=y
endif

ifeq ($(CONFIG_TARGET_echo_cpe),y)
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_IPA_FSE=y
else
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_ECM_EMESH=y
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_ECM_FSE=y
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_ECM_NL=y
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER=y
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_MSCS=y
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_PPE=y
MAKE_OPTS+=QCA_WIFI_NSS_PLUGINS_PPEDS=y
EXTRA_CFLAGS+= -Wall -Werror
endif

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

define Build/InstallDev/qca-wifi-nss-plugins
	$(INSTALL_DIR) $(1)/usr/include/qca-wifi-nss-plugins
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-wifi-nss-plugins,$(1))
endef

$(eval $(call KernelPackage,qca-wifi-nss-plugins))
