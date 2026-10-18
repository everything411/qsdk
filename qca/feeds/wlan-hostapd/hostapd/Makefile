# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2006-2021 OpenWrt.org

include $(TOPDIR)/rules.mk

PKG_NAME:=hostapd
PKG_RELEASE:=2

PKG_SOURCE_URL:=https://git.w1.fi/hostap.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2025-11-11
PKG_SOURCE_VERSION:=8990591d07d823eca4d9be27491b660b67ba054e
PKG_MIRROR_HASH:=dc299f02af164bb88216eb1ceda3ecb4172d694f175e84c755e59d3f20a4e5e0
PKG_MAINTAINER:=Felix Fietkau <nbd@nbd.name>
PKG_LICENSE:=BSD-3-Clause
PKG_CPE_ID:=cpe:/a:w1.fi:hostapd
PKG_SOURCE_SUBMODULES:=skip

PKG_BUILD_PARALLEL:=1
PKG_ASLR_PIE_REGULAR:=1
PKG_BUILD_DEPENDS += PACKAGE_udbg-enhanced:udbg_enhanced

ifeq ($(CONFIG_USE_PRPLMESH_WHM),y)
	HOSTAPD_PATCH_DIR:=$(TOPDIR)/feed-qca/qca/feeds/wlan-hostapd/hostapd/openwrt_patches
	FILE_DIR_HOSTAPD:=$(TOPDIR)/feed-qca/qca/feeds/wlan-hostapd/hostapd/
	EXTERNAL_DIR:=$(TOPDIR)/feed-qca/qca/feeds/wlan-hostapd/hostapd/
	HOSTAPD_SRC_DIR:=$(TOPDIR)/qca/src/network/services/hostapd/
	EXTENSIONS_DIR:=$(TOPDIR)/qca/src/wlan-app-extns/hostapd
	QCN_APP_EXTNS_DIR:=$(TOPDIR)/qca/src/qcn-app-extns
else ifeq ($(CONFIG_TARGET_echo),y)
	HOSTAPD_PATCH_DIR:=$(TOPDIR)/owrt-qti-ipq-open/feeds/wlan-hostapd/hostapd/patches
	FILE_DIR_HOSTAPD:=$(TOPDIR)/owrt-qti-ipq-open/feeds/wlan-hostapd/hostapd/
	EXTERNAL_DIR:=$(TOPDIR)/owrt-qti-ipq-open/feeds/wlan-hostapd/hostapd/
	HOSTAPD_SRC_DIR:=$(TOPDIR)/src/network/services/hostapd/
	EXTENSIONS_DIR:=$(TOPDIR)/src/wlan-app-extns/hostapd
	QCN_APP_EXTNS_DIR:=$(TOPDIR)/src/qcn-app-extns
else
	HOSTAPD_PATCH_DIR:=$(TOPDIR)/qca/feeds/wlan-hostapd/hostapd/openwrt_patches
	FILE_DIR_HOSTAPD:=$(TOPDIR)/qca/feeds/wlan-hostapd/hostapd/
	EXTERNAL_DIR:=$(TOPDIR)/qca/feeds/wlan-hostapd/hostapd/
	HOSTAPD_SRC_DIR:=$(TOPDIR)/qca/src/network/services/hostapd/
	EXTENSIONS_DIR:=$(TOPDIR)/qca/src/wlan-app-extns/hostapd
	QCN_APP_EXTNS_DIR:=$(TOPDIR)/qca/src/qcn-app-extns
endif

ifeq ($(CONFIG_PACKAGE_udbg-enhanced),y)
UDBG_CLIENT_CORE_INC:=$(STAGING_DIR)/usr/include/udbg_enh/client
UDBG_CLIENT_CORE_LIB:=-L$(STAGING_DIR)/usr/lib -ludbg_client_core
UDBG_CLIENT_CORE_HEADER:=$(UDBG_CLIENT_CORE_INC)/core.h
UDBG_CLIENT_CORE_LIBRARY:=$(STAGING_DIR)/usr/lib/libudbg_client_core.so
UDBG_CLIENT_CORE_STAGED:=$(and $(wildcard $(UDBG_CLIENT_CORE_HEADER)),$(wildcard $(UDBG_CLIENT_CORE_LIBRARY)))
TARGET_LDFLAGS += $(UDBG_CLIENT_CORE_LIB)
UDBG_PACKAGE_MAKEOPTS:=CONFIG_PACKAGE_udbg-enhanced=y \
	UDBG_CLIENT_CORE_INC="$(UDBG_CLIENT_CORE_INC)" \
	UDBG_CLIENT_CORE_LIB="$(UDBG_CLIENT_CORE_LIB)"
ifeq ($(UDBG_CLIENT_CORE_STAGED),)
define Build/EnsureUdbgEnhanced
	@echo "Missing udbg-enhanced staging files; rebuilding udbg-enhanced before hostapd"
	rm -f "$(STAGING_DIR)/stamp/.udbg_enhanced_installed"
	+$(MAKE) -C "$(TOPDIR)" BUILD_VARIANT= ALL_VARIANTS= package/feeds/qca_wifi_apps/udbg_enhanced/compile V=s
endef
else
define Build/EnsureUdbgEnhanced
endef
endif
else
UDBG_PACKAGE_MAKEOPTS:=
define Build/EnsureUdbgEnhanced
endef
endif

PKG_CONFIG_DEPENDS:= \
	CONFIG_PACKAGE_hostapd \
	CONFIG_PACKAGE_hostapd-basic \
	CONFIG_PACKAGE_hostapd-mini \
	CONFIG_WPA_RFKILL_SUPPORT \
	CONFIG_DRIVER_11AC_SUPPORT \
	CONFIG_DRIVER_11AX_SUPPORT \
	CONFIG_WPA_ENABLE_WEP \
	CONFIG_WPA_MQTT_SUPPORT \
	CONFIG_PACKAGE_udbg-enhanced

PKG_BUILD_FLAGS:=gc-sections lto

EAPOL_TEST_PROVIDERS:=eapol-test eapol-test-openssl eapol-test-wolfssl

SUPPLICANT_PROVIDERS:=
HOSTAPD_PROVIDERS:=
TARGET_CPPFLAGS += -include linux/types.h

LOCAL_TYPE=$(strip \
		$(if $(findstring wpad,$(BUILD_VARIANT)),wpad, \
		$(if $(findstring supplicant,$(BUILD_VARIANT)),supplicant, \
		hostapd \
		)))

LOCAL_AND_LIB_VARIANT=$(patsubst hostapd-%,%,\
		      $(patsubst wpad-%,%,\
		      $(patsubst supplicant-%,%,\
		      $(BUILD_VARIANT)\
		      )))

LOCAL_VARIANT=$(patsubst %-internal,%,\
	      $(patsubst %-openssl,%,\
	      $(patsubst %-wolfssl,%,\
	      $(patsubst %-mbedtls,%,\
	      $(patsubst %-noextns,%,\
	      $(LOCAL_AND_LIB_VARIANT)\
	      )))))

SSL_VARIANT=$(strip \
		$(if $(findstring openssl,$(LOCAL_AND_LIB_VARIANT)),openssl,\
		$(if $(findstring wolfssl,$(LOCAL_AND_LIB_VARIANT)),wolfssl,\
		$(if $(findstring mbedtls,$(LOCAL_AND_LIB_VARIANT)),mbedtls,\
		internal\
		))))

CONFIG_VARIANT:=$(LOCAL_VARIANT)
ifeq ($(LOCAL_VARIANT),mesh)
  CONFIG_VARIANT:=full
endif
ifneq ($(filter noextns,$(subst -, ,$(BUILD_VARIANT))),)
  CONFIG_VARIANT:=$(CONFIG_VARIANT)-noextns
endif

ifneq ("$(wildcard $(FILE_DIR_HOSTAPD)/wlan-hostapd.mk)","")
   include $(FILE_DIR_HOSTAPD)/wlan-hostapd.mk
endif

include $(INCLUDE_DIR)/package.mk

STAMP_CONFIGURED:=$(STAMP_CONFIGURED)_$(CONFIG_WPA_MSG_MIN_PRIORITY)

ifneq ($(CONFIG_DRIVER_11AC_SUPPORT),)
  HOSTAPD_IEEE80211AC:=y
endif

ifneq ($(CONFIG_DRIVER_11AX_SUPPORT),)
  HOSTAPD_IEEE80211AX:=y
endif

CORE_DEPENDS = +ucode +libubus +libucode +ucode-mod-fs +ucode-mod-nl80211 +ucode-mod-rtnl +ucode-mod-ubus +ucode-mod-uloop +libblobmsg-json +libudebug +libmnl +WPA_MQTT_SUPPORT:libmosquitto-ssl
OPENSSL_DEPENDS = +PACKAGE_$(1):libopenssl +PACKAGE_$(1):libopenssl-legacy

ifeq ($(CONFIG_PACKAGE_udbg-enhanced),y)
  CORE_DEPENDS += +udbg-enhanced
endif

DRIVER_MAKEOPTS= \
	CONFIG_ACS=y CONFIG_DRIVER_NL80211=y \
	$(if $(HOSTAPD_IEEE80211AC),CONFIG_IEEE80211AC=$(HOSTAPD_IEEE80211AC)) \
	$(if $(HOSTAPD_IEEE80211AX),CONFIG_IEEE80211AX=$(HOSTAPD_IEEE80211AX)) \
	CONFIG_MBO=$(CONFIG_WPA_MBO_SUPPORT) \
	CONFIG_ATF_OFFLOAD=y \
	CONFIG_TESTING_OPTIONS=y \
	CONFIG_PROCESS_COORDINATION=y

ifneq ($(CONFIG_WPA_MQTT_SUPPORT),)
  DRIVER_MAKEOPTS += CONFIG_MQTT=y CONFIG_MQTT_TEST_APP_FORK=y
  TARGET_LDFLAGS += -lmosquitto
endif

ifdef CONFIG_PACKAGE_QCN_EXTN
  # Intended only for QCN lab test purpose, not for production builds as it may overrides regulatory requirements
  DRIVER_MAKEOPTS += CONFIG_QCA_LAB_TEST_FEATURES=y
endif

ifneq ($(CONFIG_USE_PRPLMESH_WHM),y)
  DRIVER_MAKEOPTS += CONFIG_UCODE=y
endif

ifeq ($(SSL_VARIANT),openssl)
  DRIVER_MAKEOPTS += CONFIG_TLS=openssl CONFIG_SAE=y
  TARGET_LDFLAGS += -lcrypto -lssl

  ifeq ($(LOCAL_VARIANT),basic)
    DRIVER_MAKEOPTS += CONFIG_OWE=y
  endif
  ifeq ($(LOCAL_VARIANT),mesh)
    DRIVER_MAKEOPTS += CONFIG_AP=y CONFIG_MESH=y
  endif
  ifeq ($(LOCAL_VARIANT),full)
    DRIVER_MAKEOPTS += CONFIG_OWE=y CONFIG_SUITEB192=y CONFIG_AP=y CONFIG_MESH=y
  endif
endif

ifeq ($(SSL_VARIANT),wolfssl)
  DRIVER_MAKEOPTS += CONFIG_TLS=wolfssl CONFIG_SAE=y
  TARGET_LDFLAGS += -lwolfssl

  ifeq ($(LOCAL_VARIANT),basic)
    DRIVER_MAKEOPTS += CONFIG_OWE=y
  endif
  ifeq ($(LOCAL_VARIANT),mesh)
    DRIVER_MAKEOPTS += CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
  ifeq ($(LOCAL_VARIANT),full)
    DRIVER_MAKEOPTS += CONFIG_OWE=y CONFIG_SUITEB192=y CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
endif

ifeq ($(SSL_VARIANT),mbedtls)
  DRIVER_MAKEOPTS += CONFIG_TLS=mbedtls CONFIG_SAE=y
  TARGET_LDFLAGS += -lmbedcrypto -lmbedx509 -lmbedtls

  ifeq ($(LOCAL_VARIANT),basic)
    DRIVER_MAKEOPTS += CONFIG_OWE=y
  endif
  ifeq ($(LOCAL_VARIANT),mesh)
    DRIVER_MAKEOPTS += CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
  ifeq ($(LOCAL_VARIANT),full)
    DRIVER_MAKEOPTS += CONFIG_OWE=y CONFIG_SUITEB192=y CONFIG_AP=y CONFIG_MESH=y CONFIG_WPS_NFC=1
  endif
endif

ifneq ($(LOCAL_TYPE),hostapd)
  ifdef CONFIG_WPA_RFKILL_SUPPORT
    DRIVER_MAKEOPTS += NEED_RFKILL=y
  endif
endif

DRV_DEPENDS:=+libnl-tiny


define Package/hostapd/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Authenticator
  URL:=http://hostap.epitest.fi/
  DEPENDS:=$(DRV_DEPENDS) +hostapd-common $(CORE_DEPENDS)
  EXTRA_DEPENDS:=hostapd-common (=$(PKG_VERSION)-r$(PKG_RELEASE))
  USERID:=network=101:network=101
  PROVIDES:=hostapd
  CONFLICTS:=$(HOSTAPD_PROVIDERS)
  HOSTAPD_PROVIDERS+=$(1)
endef

define Package/hostapd
$(call Package/hostapd/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=full-internal
endef

define Package/hostapd/description
 This package contains a full featured IEEE 802.1x/WPA/EAP/RADIUS
 Authenticator.
endef

define Package/hostapd-openssl
$(call Package/hostapd/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=full-openssl
  DEPENDS+=$(OPENSSL_DEPENDS)
endef

Package/hostapd-openssl/description = $(Package/hostapd/description)

define Package/wpad-macsec
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Auth/Supplicant
  TITLE+= (MACSEC)
  DEPENDS:=$(DRV_DEPENDS) +hostapd-common $(CORE_DEPENDS)
  DEPENDS+=$(OPENSSL_DEPENDS) +kmod-qca-nss-macsec +libnl +libnl-genl +libnl-route
  EXTRA_DEPENDS:=hostapd-common (=$(PKG_VERSION)-r$(PKG_RELEASE))
  USERID:=network=101:network=101
  URL:=http://hostap.epitest.fi/
  PROVIDES:=hostapd-macsec wpa-supplicant-macsec
  VARIANT:=wpad-macsec
endef

Package/wpad-macsec/description = $(Package/wpad/description)

define Package/wpad-macsec-noextns
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Auth/Supplicant
  TITLE+= (MACSEC, no QCN extn)
  DEPENDS:=$(DRV_DEPENDS) +hostapd-common $(CORE_DEPENDS)
  DEPENDS+=$(OPENSSL_DEPENDS) +kmod-qca-nss-macsec +libnl +libnl-genl +libnl-route
  EXTRA_DEPENDS:=hostapd-common (=$(PKG_VERSION)-r$(PKG_RELEASE))
  USERID:=network=101:network=101
  URL:=http://hostap.epitest.fi/
  PROVIDES:=hostapd-macsec wpa-supplicant-macsec
  VARIANT:=wpad-macsec-noextns
  DEFAULT:=m
endef

Package/wpad-macsec-noextns/description = $(Package/wpad-macsec/description)

define Package/hostapd-wolfssl
$(call Package/hostapd/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=full-wolfssl
  DEPENDS+=+PACKAGE_hostapd-wolfssl:libwolfssl
endef

Package/hostapd-wolfssl/description = $(Package/hostapd/description)

define Package/hostapd-mbedtls
$(call Package/hostapd/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=full-mbedtls
  DEPENDS+=+PACKAGE_hostapd-mbedtls:libmbedtls
endef

Package/hostapd-mbedtls/description = $(Package/hostapd/description)

define Package/hostapd-basic
$(call Package/hostapd/Default,$(1))
  TITLE+= (WPA-PSK, 11r, 11w)
  VARIANT:=basic
endef

define Package/hostapd-basic/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-basic-openssl
$(call Package/hostapd/Default,$(1))
  TITLE+= (WPA-PSK, 11r and 11w)
  VARIANT:=basic-openssl
  DEPENDS+=+PACKAGE_hostapd-basic-openssl:libopenssl
endef

define Package/hostapd-basic-openssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-basic-wolfssl
$(call Package/hostapd/Default,$(1))
  TITLE+= (WPA-PSK, 11r and 11w)
  VARIANT:=basic-wolfssl
  DEPENDS+=+PACKAGE_hostapd-basic-wolfssl:libwolfssl
endef

define Package/hostapd-basic-wolfssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-basic-mbedtls
$(call Package/hostapd/Default,$(1))
  TITLE+= (WPA-PSK, 11r and 11w)
  VARIANT:=basic-mbedtls
  DEPENDS+=+PACKAGE_hostapd-basic-mbedtls:libmbedtls
endef

define Package/hostapd-basic-mbedtls/description
 This package contains a basic IEEE 802.1x/WPA Authenticator with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/hostapd-mini
$(call Package/hostapd/Default,$(1))
  TITLE+= (WPA-PSK only)
  VARIANT:=mini
endef

define Package/hostapd-mini/description
 This package contains a minimal IEEE 802.1x/WPA Authenticator (WPA-PSK only).
endef


define Package/wpad/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Auth/Supplicant
  DEPENDS:=$(DRV_DEPENDS) +hostapd-common $(CORE_DEPENDS)
  EXTRA_DEPENDS:=hostapd-common
  USERID:=network=101:network=101
  URL:=http://hostap.epitest.fi/
  PROVIDES:=hostapd wpa-supplicant
  CONFLICTS:=$(HOSTAPD_PROVIDERS) $(SUPPLICANT_PROVIDERS)
  HOSTAPD_PROVIDERS+=$(1)
  SUPPLICANT_PROVIDERS+=$(1)
endef

define Package/wpad
$(call Package/wpad/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=wpad-full-internal
endef

define Package/wpad/description
 This package contains a full featured IEEE 802.1x/WPA/EAP/RADIUS
 Authenticator and Supplicant
endef

define Package/wpad-openssl
$(call Package/wpad/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=wpad-full-openssl
  DEPENDS+=$(OPENSSL_DEPENDS)
endef

Package/wpad-openssl/description = $(Package/wpad/description)

define Package/wpad-wolfssl
$(call Package/wpad/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=wpad-full-wolfssl
  DEPENDS+=+PACKAGE_wpad-wolfssl:libwolfssl
endef

Package/wpad-wolfssl/description = $(Package/wpad/description)

define Package/wpad-mbedtls
$(call Package/wpad/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=wpad-full-mbedtls
  DEPENDS+=+PACKAGE_wpad-mbedtls:libmbedtls
endef

Package/wpad-mbedtls/description = $(Package/wpad/description)

define Package/wpad-basic
$(call Package/wpad/Default,$(1))
  TITLE+= (WPA-PSK, 11r, 11w)
  VARIANT:=wpad-basic
endef

define Package/wpad-basic/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, 802.11r and 802.11w support.
endef

define Package/wpad-basic-openssl
$(call Package/wpad/Default,$(1))
  TITLE+= (OpenSSL, 11r, 11w)
  VARIANT:=wpad-basic-openssl
  DEPENDS+=$(OPENSSL_DEPENDS)
endef

define Package/wpad-basic-openssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, SAE (WPA3-Personal), 802.11r and 802.11w support.
endef

define Package/wpad-basic-wolfssl
$(call Package/wpad/Default,$(1))
  TITLE+= (wolfSSL, 11r, 11w)
  VARIANT:=wpad-basic-wolfssl
  DEPENDS+=+PACKAGE_wpad-basic-wolfssl:libwolfssl
endef

define Package/wpad-basic-wolfssl/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, SAE (WPA3-Personal), 802.11r and 802.11w support.
endef

define Package/wpad-basic-mbedtls
$(call Package/wpad/Default,$(1))
  TITLE+= (mbedTLS, 11r, 11w)
  VARIANT:=wpad-basic-mbedtls
  DEPENDS+=+PACKAGE_wpad-basic-mbedtls:libmbedtls
endef

define Package/wpad-basic-mbedtls/description
 This package contains a basic IEEE 802.1x/WPA Authenticator and Supplicant with WPA-PSK, SAE (WPA3-Personal), 802.11r and 802.11w support.
endef

define Package/wpad-mini
$(call Package/wpad/Default,$(1))
  TITLE+= (WPA-PSK only)
  VARIANT:=wpad-mini
endef

define Package/wpad-mini/description
 This package contains a minimal IEEE 802.1x/WPA Authenticator and Supplicant (WPA-PSK only).
endef

define Package/wpad-mesh
$(call Package/wpad/Default,$(1))
  DEPENDS+=@(!TARGET_uml||BROKEN)
  PROVIDES+=wpa-supplicant-mesh wpad-mesh
endef

define Package/wpad-mesh/description
 This package contains a minimal IEEE 802.1x/WPA Authenticator and Supplicant (with 802.11s mesh and SAE support).
endef

define Package/wpad-mesh-openssl
$(call Package/wpad-mesh,$(1))
  TITLE+= (OpenSSL, 11s, SAE)
  DEPENDS+=$(OPENSSL_DEPENDS)
  VARIANT:=wpad-mesh-openssl
  PROVIDES:=hostapd-openssl wpa-supplicant-openssl hostapd-qca-openssl wpa-supplicant-qca-openssl wpad-qca-mesh-openssl
endef

Package/wpad-mesh-openssl/description = $(Package/wpad-mesh/description)

define Package/wpad-mesh-openssl-noextns
$(call Package/wpad-mesh,$(1))
  TITLE+= (OpenSSL, 11s, SAE, no QCN extn)
  DEPENDS+=$(OPENSSL_DEPENDS)
  VARIANT:=wpad-mesh-openssl-noextns
  DEFAULT:=m
  PROVIDES:=hostapd-openssl wpa-supplicant-openssl hostapd-qca-openssl wpa-supplicant-qca-openssl wpad-qca-mesh-openssl
  CONFLICTS:=
endef

Package/wpad-mesh-openssl-noextns/description = $(Package/wpad-mesh/description)

define Package/wpad-mesh-wolfssl
$(call Package/wpad-mesh,$(1))
  TITLE+= (wolfSSL, 11s, SAE)
  DEPENDS+=+PACKAGE_wpad-mesh-wolfssl:libwolfssl
  VARIANT:=wpad-mesh-wolfssl
endef

Package/wpad-mesh-wolfssl/description = $(Package/wpad-mesh/description)

define Package/wpad-mesh-mbedtls
$(call Package/wpad-mesh,$(1))
  TITLE+= (mbedTLS, 11s, SAE)
  DEPENDS+=+PACKAGE_wpad-mesh-mbedtls:libmbedtls
  VARIANT:=wpad-mesh-mbedtls
endef

Package/wpad-mesh-mbedtls/description = $(Package/wpad-mesh/description)


define Package/wpa-supplicant/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=WPA Supplicant
  URL:=http://hostap.epitest.fi/wpa_supplicant/
  DEPENDS:=$(DRV_DEPENDS) +hostapd-common $(CORE_DEPENDS)
  EXTRA_DEPENDS:=hostapd-common (=$(PKG_VERSION)-r$(PKG_RELEASE))
  USERID:=network=101:network=101
  PROVIDES:=wpa-supplicant
  CONFLICTS:=$(SUPPLICANT_PROVIDERS)
  SUPPLICANT_PROVIDERS+=$(1)
endef

define Package/wpa-supplicant
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=supplicant-full-internal
endef

define Package/wpa-supplicant-openssl
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=supplicant-full-openssl
  DEPENDS+=$(OPENSSL_DEPENDS)
endef

define Package/wpa-supplicant-wolfssl
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=supplicant-full-wolfssl
  DEPENDS+=+PACKAGE_wpa-supplicant-wolfssl:libwolfssl
endef

define Package/wpa-supplicant-mbedtls
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=supplicant-full-mbedtls
  DEPENDS+=+PACKAGE_wpa-supplicant-mbedtls:libmbedtls
endef

define Package/wpa-supplicant/config
	source "$(SOURCE)/Config.in"
endef


define Package/wpa-supplicant-p2p
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (Wi-Fi P2P support)
  VARIANT:=supplicant-p2p-internal
endef

define Package/wpa-supplicant-mesh/Default
$(call Package/wpa-supplicant/Default,$(1))
  DEPENDS+=@(!TARGET_uml||BROKEN)
  PROVIDES+=wpa-supplicant-mesh
endef

define Package/wpa-supplicant-mesh-openssl
$(call Package/wpa-supplicant-mesh/Default,$(1))
  TITLE+= (OpenSSL, 11s, SAE)
  VARIANT:=supplicant-mesh-openssl
  DEPENDS+=$(OPENSSL_DEPENDS)
endef

define Package/wpa-supplicant-mesh-wolfssl
$(call Package/wpa-supplicant-mesh/Default,$(1))
  TITLE+= (wolfSSL, 11s, SAE)
  VARIANT:=supplicant-mesh-wolfssl
  DEPENDS+=+PACKAGE_wpa-supplicant-mesh-wolfssl:libwolfssl
endef

define Package/wpa-supplicant-mesh-mbedtls
$(call Package/wpa-supplicant-mesh/Default,$(1))
  TITLE+= (mbedTLS, 11s, SAE)
  VARIANT:=supplicant-mesh-mbedtls
  DEPENDS+=+PACKAGE_wpa-supplicant-mesh-mbedtls:libmbedtls
endef

define Package/wpa-supplicant-basic
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (11r, 11w)
  VARIANT:=supplicant-basic
endef

define Package/wpa-supplicant-mini
$(call Package/wpa-supplicant/Default,$(1))
  TITLE+= (minimal)
  VARIANT:=supplicant-mini
endef


define Package/hostapd-common
  TITLE:=hostapd/wpa_supplicant common support files
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
endef

define Package/hostapd-utils
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Authenticator (utils)
  URL:=http://hostap.epitest.fi/
  DEPENDS:=@$(subst $(space),||,$(foreach pkg,$(HOSTAPD_PROVIDERS),PACKAGE_$(pkg)))
  VARIANT:=wpad-mesh-openssl
  CONFLICTS:=hostapd-utils-noextns
endef

define Package/hostapd-utils/description
 This package contains a command line utility to control the
 IEEE 802.1x/WPA/EAP/RADIUS Authenticator (with QCN extensions).
endef

define Package/hostapd-utils-noextns
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  TITLE:=IEEE 802.1x Authenticator (utils, no QCN extn)
  URL:=http://hostap.epitest.fi/
  DEPENDS:=+PACKAGE_wpad-mesh-openssl-noextns:wpad-mesh-openssl-noextns
  VARIANT:=wpad-mesh-openssl-noextns
  DEFAULT:=m
  CONFLICTS:=hostapd-utils
endef

define Package/hostapd-utils-noextns/description
 This package contains a command line utility to control the
 IEEE 802.1x/WPA/EAP/RADIUS Authenticator (without QCN extensions).
endef

define Package/wpa-cli
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=WirelessAPD
  DEPENDS:=@$(subst $(space),||,$(foreach pkg,$(SUPPLICANT_PROVIDERS),PACKAGE_$(pkg)))
  TITLE:=WPA Supplicant command line control utility
  VARIANT:=*
endef

define Package/eapol-test/Default
  TITLE:=802.1x auth test utility
  SECTION:=net
  SUBMENU:=WirelessAPD
  CATEGORY:=Network
  DEPENDS:=$(DRV_DEPENDS) $(CORE_DEPENDS)
endef

define Package/eapol-test
  $(call Package/eapol-test/Default,$(1))
  TITLE+= (built-in full)
  VARIANT:=supplicant-full-internal
endef

define Package/eapol-test-openssl
  $(call Package/eapol-test/Default,$(1))
  TITLE+= (OpenSSL full)
  VARIANT:=supplicant-full-openssl
  CONFLICTS:=$(filter-out eapol-test-openssl ,$(EAPOL_TEST_PROVIDERS))
  DEPENDS+=$(OPENSSL_DEPENDS)
  PROVIDES:=eapol-test
endef

define Package/eapol-test-wolfssl
  $(call Package/eapol-test/Default,$(1))
  TITLE+= (wolfSSL full)
  VARIANT:=supplicant-full-wolfssl
  CONFLICTS:=$(filter-out eapol-test-openssl ,$(filter-out eapol-test-wolfssl ,$(EAPOL_TEST_PROVIDERS)))
  DEPENDS+=+PACKAGE_eapol-test-wolfssl:libwolfssl
  PROVIDES:=eapol-test
endef

define Package/eapol-test-mbedtls
  $(call Package/eapol-test/Default,$(1))
  TITLE+= (mbedTLS full)
  VARIANT:=supplicant-full-mbedtls
  CONFLICTS:=$(filter-out eapol-test-openssl ,$(filter-out eapol-test-mbedtls ,$(EAPOL_TEST_PROVIDERS)))
  DEPENDS+=+PACKAGE_eapol-test-mbedtls:libmbedtls
  PROVIDES:=eapol-test
endef

ifneq ($(wildcard $(PKG_BUILD_DIR)/.config_*),$(subst .configured_,.config_,$(STAMP_CONFIGURED)))
  define Build/Configure/rebuild
	$(FIND) $(PKG_BUILD_DIR) -name \*.o -or -name \*.a | $(XARGS) rm -f
	rm -f $(PKG_BUILD_DIR)/hostapd/hostapd
	rm -f $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant
	rm -f $(PKG_BUILD_DIR)/.config_*
	touch $(subst .configured_,.config_,$(STAMP_CONFIGURED))
  endef
endif

define Build/Patch
	$(CP) -r $(HOSTAPD_SRC_DIR)/* $(PKG_BUILD_DIR)/
ifdef CONFIG_PACKAGE_QCN_EXTN
ifeq ($(filter noextns,$(subst -, ,$(BUILD_VARIANT))),)
	$(CP) $(EXTENSIONS_DIR)/src $(PKG_BUILD_DIR)/qcn_extns
	$(if $(wildcard $(QCN_APP_EXTNS_DIR)/qacs/qacs.c), \
		$(CP) $(QCN_APP_EXTNS_DIR)/qacs $(PKG_BUILD_DIR)/qcn_extns
	)
endif
endif
endef

define Build/Configure
	$(Build/Configure/rebuild)
	$(if $(wildcard ./files/hostapd-$(CONFIG_VARIANT).config), \
		$(CP) ./files/hostapd-$(CONFIG_VARIANT).config $(PKG_BUILD_DIR)/hostapd/.config, \
		$(if $(wildcard ./files/hostapd-$(patsubst %-noextns,%,$(CONFIG_VARIANT)).config), \
			$(CP) ./files/hostapd-$(patsubst %-noextns,%,$(CONFIG_VARIANT)).config $(PKG_BUILD_DIR)/hostapd/.config \
		) \
	)
	$(if $(wildcard ./files/wpa_supplicant-$(CONFIG_VARIANT).config), \
		$(CP) ./files/wpa_supplicant-$(CONFIG_VARIANT).config $(PKG_BUILD_DIR)/wpa_supplicant/.config, \
		$(if $(wildcard ./files/wpa_supplicant-$(patsubst %-noextns,%,$(CONFIG_VARIANT)).config), \
			$(CP) ./files/wpa_supplicant-$(patsubst %-noextns,%,$(CONFIG_VARIANT)).config $(PKG_BUILD_DIR)/wpa_supplicant/.config \
		) \
	)

ifeq ($(CONFIG_USE_PRPLMESH_WHM),y)
	-sed -i 's/^CONFIG_UBUS=y/# CONFIG_UBUS is not set/' \
		$(PKG_BUILD_DIR)/hostapd/.config \
		$(PKG_BUILD_DIR)/wpa_supplicant/.config
endif

ifndef CONFIG_PACKAGE_QCN_EXTN
	-sed -i 's/^CONFIG_QCN_EXTN=y/# CONFIG_QCN_EXTN is not set/' \
		$(PKG_BUILD_DIR)/hostapd/.config \
		$(PKG_BUILD_DIR)/wpa_supplicant/.config
	-sed -i 's/^CONFIG_QCA_WPA_SUPPLICANT_BUILD=y/# CONFIG_QCA_WPA_SUPPLICANT_BUILD is not set/' \
		$(PKG_BUILD_DIR)/hostapd/.config \
		$(PKG_BUILD_DIR)/wpa_supplicant/.config
else ifneq ($(filter noextns,$(subst -, ,$(BUILD_VARIANT))),)
	-sed -i 's/^CONFIG_QCN_EXTN=y/# CONFIG_QCN_EXTN is not set/' \
		$(PKG_BUILD_DIR)/hostapd/.config \
		$(PKG_BUILD_DIR)/wpa_supplicant/.config
	-sed -i 's/^CONFIG_QCA_WPA_SUPPLICANT_BUILD=y/# CONFIG_QCA_WPA_SUPPLICANT_BUILD is not set/' \
		$(PKG_BUILD_DIR)/hostapd/.config \
		$(PKG_BUILD_DIR)/wpa_supplicant/.config
endif
endef

## Build libwpa_ctrl.a objects at top-level to avoid conditionals inside define
ifneq ($(LOCAL_VARIANT),macsec)
OBJS_LIBWPA_CTRL := \
	$(PKG_BUILD_DIR)/build/hostapd/src/common/wpa_ctrl.o \
	$(PKG_BUILD_DIR)/build/hostapd/src/utils/os_unix.o \
	$(PKG_BUILD_DIR)/build/hostapd/src/utils/wpa_debug.o \
	$(PKG_BUILD_DIR)/build/hostapd/src/utils/common.o \
	$(PKG_BUILD_DIR)/build/hostapd/src/utils/eloop.o
ifeq ($(CONFIG_UCODE),y)
OBJS_LIBWPA_CTRL += $(PKG_BUILD_DIR)/build/hostapd/src/utils/uloop.o
endif
endif

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DIR) $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/src/common/qca-vendor.h $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/src/drivers/nl80211_copy.h $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/src/common/wpa_ctrl.h $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/src/utils/os.h $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/src/utils/common.h $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/src/utils/wpa_debug.h $(1)/usr/include
	$(if $(strip $(OBJS_LIBWPA_CTRL)),ar rcs $(1)/usr/lib/libwpa_ctrl.a $(OBJS_LIBWPA_CTRL))
endef

TARGET_CPPFLAGS := \
	-I$(STAGING_DIR)/usr/include/libnl-tiny \
	-I$(PKG_BUILD_DIR)/src/crypto \
	$(TARGET_CPPFLAGS) \
	-DCONFIG_LIBNL20 \
	-D_GNU_SOURCE \
	$(if $(CONFIG_WPA_MSG_MIN_PRIORITY),-DCONFIG_MSG_MIN_PRIORITY=$(CONFIG_WPA_MSG_MIN_PRIORITY))

TARGET_LDFLAGS += -lubox -lubus -lblobmsg_json -lucode -lm -lnl-tiny -ludebug -lmnl
TARGET_CFLAGS += -fPIC -Wall -Werror -g
ifndef CONFIG_PACKAGE_QCN_EXTN
TARGET_CFLAGS += -DWPA_IGNORE_CONFIG_ERRORS
endif

ifdef CONFIG_WPA_ENABLE_WEP
    DRIVER_MAKEOPTS += CONFIG_WEP=y
endif

ifneq ($(findstring macsec,$(BUILD_VARIANT)),)
	TARGET_LDFLAGS += -lcrypto -lssl -lfal -L$(STAGING_DIR)/usr/lib -lnl-3 -lnl-genl-3 -lnl-route-3 -ldl -pie
	TARGET_LDFLAGS := $(filter-out -lnl-tiny $(UDBG_CLIENT_CORE_LIB), $(TARGET_LDFLAGS))
	TARGET_CFLAGS += -Wno-implicit-function-declaration -Wno-int-conversion -Wno-error=unused-function
	TARGET_CPPFLAGS := \
		-I$(STAGING_DIR)/usr/include \
		-I$(STAGING_DIR)/usr/include/qca-nss-macsec

	DRV_DEPENDS := +libnl +libnl-genl +libnl-route
	DRIVER_MAKEOPTS :=
	UDBG_PACKAGE_MAKEOPTS :=
	CORE_DEPENDS := $(filter-out +ucode-mod-nl80211 +udbg-enhanced,$(CORE_DEPENDS))
endif

define Build/RunMake
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS)" \
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/$(1) \
		$(TARGET_CONFIGURE_OPTS) \
		$(DRIVER_MAKEOPTS) \
		$(UDBG_PACKAGE_MAKEOPTS) \
		LIBS="$(TARGET_LDFLAGS)" \
		LIBS_c="$(TARGET_LDFLAGS_C)" \
		AR="$(TARGET_CROSS)gcc-ar" \
		BCHECK= \
		$(if $(findstring s,$(OPENWRT_VERBOSE)),V=1) \
		$(2)
endef

define Build/Compile/wpad
	echo ` \
		$(call Build/RunMake,hostapd,-s MULTICALL=1 dump_cflags); \
		$(call Build/RunMake,wpa_supplicant,-s MULTICALL=1 dump_cflags) | \
		sed -e 's,-n ,,g' -e 's^$(TARGET_CFLAGS)^^' \
	` > $(PKG_BUILD_DIR)/.cflags
	sed -i 's/"/\\"/g' $(PKG_BUILD_DIR)/.cflags
	+$(call Build/RunMake,hostapd, \
		CFLAGS="$$$$(cat $(PKG_BUILD_DIR)/.cflags)" \
		MULTICALL=1 \
		hostapd_cli hostapd_multi.a \
	)
	+$(call Build/RunMake,wpa_supplicant, \
		CFLAGS="$$$$(cat $(PKG_BUILD_DIR)/.cflags)" \
		MULTICALL=1 \
		wpa_cli wpa_supplicant_multi.a \
	)
	+export MAKEFLAGS="$(MAKE_JOBSERVER)"; $(TARGET_CC) -o $(PKG_BUILD_DIR)/wpad \
		$(TARGET_CFLAGS) \
		./files/multicall.c \
		$(PKG_BUILD_DIR)/hostapd/hostapd_multi.a \
		$(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant_multi.a \
		$(TARGET_LDFLAGS)
	$(if $(CONFIG_WPA_MQTT_SUPPORT),$(if $(findstring macsec,$(BUILD_VARIANT)),,+$(call Build/RunMake,hostapd,mqtt_test$(if $(filter noextns,$(subst -, ,$(BUILD_VARIANT))),, external_hif_mqtt_test_client))))
endef

define Build/Compile/hostapd
	+$(call Build/RunMake,hostapd, \
		hostapd hostapd_cli \
		$(if $(CONFIG_WPA_MQTT_SUPPORT),mqtt_test$(if $(filter noextns,$(subst -, ,$(BUILD_VARIANT))),, external_hif_mqtt_test_client)) \
	)
endef

define Build/Compile/supplicant
	+$(call Build/RunMake,wpa_supplicant, \
		wpa_cli wpa_supplicant \
	)
endef

define Build/Compile/supplicant-full-internal
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile/supplicant-full-openssl
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile/supplicant-full-wolfssl
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile/supplicant-full-mbedtls
	+$(call Build/RunMake,wpa_supplicant, \
		eapol_test \
	)
endef

define Build/Compile
	$(call Build/EnsureUdbgEnhanced)
	$(Build/Compile/$(LOCAL_TYPE))
	$(Build/Compile/$(BUILD_VARIANT))
endef

define Install/hostapd/full
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/config $(1)/etc/radius
	ln -sf hostapd $(1)/usr/sbin/hostapd-radius
	$(INSTALL_BIN) ./files/radius.init $(1)/etc/init.d/radius
	$(INSTALL_DATA) ./files/radius.config $(1)/etc/config/radius
	$(INSTALL_DATA) ./files/radius.clients $(1)/etc/radius/clients
	$(INSTALL_DATA) ./files/radius.users $(1)/etc/radius/users
endef

define Package/hostapd-full/conffiles
/etc/config/radius
/etc/radius
endef

ifneq ($(filter full full-noextns,$(CONFIG_VARIANT)),)
Package/wpad-mesh-openssl/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad-mesh-openssl-noextns/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad-mesh-wolfssl/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad-mesh-mbedtls/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad-openssl/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad-wolfssl/conffiles = $(Package/hostapd-full/conffiles)
Package/wpad-mbedtls/conffiles = $(Package/hostapd-full/conffiles)
Package/hostapd/conffiles = $(Package/hostapd-full/conffiles)
Package/hostapd-openssl/conffiles = $(Package/hostapd-full/conffiles)
Package/hostapd-wolfssl/conffiles = $(Package/hostapd-full/conffiles)
Package/hostapd-mbedtls/conffiles = $(Package/hostapd-full/conffiles)
endif

#Select hostapd.uc file from external hostapd dir if available
ifneq ("$(wildcard $(FILE_DIR_HOSTAPD)/files/hostapd.uc)","")
define Install/hostapd
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/hostap
	$(INSTALL_DATA) $(FILE_DIR_HOSTAPD)/files/hostapd.uc $(1)/usr/share/hostap/
	$(if $(findstring full,$(CONFIG_VARIANT)),$(Install/hostapd/full))
endef
else
define Install/hostapd
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/hostap
	$(INSTALL_DATA) ./files/hostapd.uc $(1)/usr/share/hostap/
	$(if $(findstring full,$(CONFIG_VARIANT)),$(Install/hostapd/full))
endef
endif

#Select wpa_supplicant.uc file from external hostapd dir if available
ifneq ("$(wildcard $(FILE_DIR_HOSTAPD)/files/wpa_supplicant.uc)","")
define Install/supplicant
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/hostap
	$(INSTALL_DATA) $(FILE_DIR_HOSTAPD)/files/wpa_supplicant.uc $(1)/usr/share/hostap/
endef
else
define Install/supplicant
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/hostap
	$(INSTALL_DATA) ./files/wpa_supplicant.uc $(1)/usr/share/hostap/
endef
endif

define Package/hostapd-common/install
	$(INSTALL_DIR) $(1)/etc/capabilities $(1)/etc/rc.button $(1)/etc/hotplug.d/ieee80211 $(1)/etc/init.d $(1)/lib/netifd  $(1)/usr/share/acl.d $(1)/usr/share/hostap
	$(INSTALL_BIN) ./files/wpad.init $(1)/etc/init.d/wpad
	$(INSTALL_BIN) ./files/wps-hotplug.sh $(1)/etc/rc.button/wps
	$(INSTALL_DATA) ./files/wpad_acl.json $(1)/usr/share/acl.d
	$(INSTALL_DATA) ./files/wpad.json $(1)/etc/capabilities
endef

define Package/hostapd/install
	$(call Install/hostapd,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd $(1)/usr/sbin/
	$(if $(CONFIG_WPA_MQTT_SUPPORT),$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/mqtt_test $(1)/usr/sbin/,)
	$(if $(and $(CONFIG_WPA_MQTT_SUPPORT),$(if $(filter noextns,$(subst -, ,$(BUILD_VARIANT))),,x)),$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/external_hif_mqtt_test_client $(1)/usr/sbin/,)
endef
Package/hostapd-basic/install = $(Package/hostapd/install)
Package/hostapd-basic-openssl/install = $(Package/hostapd/install)
Package/hostapd-basic-wolfssl/install = $(Package/hostapd/install)
Package/hostapd-basic-mbedtls/install = $(Package/hostapd/install)
Package/hostapd-mini/install = $(Package/hostapd/install)
Package/hostapd-openssl/install = $(Package/hostapd/install)
Package/hostapd-wolfssl/install = $(Package/hostapd/install)
Package/hostapd-mbedtls/install = $(Package/hostapd/install)

define Package/wpad-macsec/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpad $(1)/usr/sbin/wpad-macsec
	$(LN) wpad-macsec $(1)/usr/sbin/hostapd-macsec
	$(LN) wpad-macsec $(1)/usr/sbin/wpa_supplicant-macsec
	$(INSTALL_DIR) $(1)/etc/802.1x
	$(INSTALL_BIN) ./files/8021x-client $(1)/usr/sbin/8021x-client
	$(INSTALL_BIN) ./files/8021x_action.sh $(1)/etc/802.1x/8021x_action.sh
	$(INSTALL_BIN) ./files/dhcp_wan_renew.sh $(1)/etc/802.1x/dhcp_wan_renew.sh
	$(INSTALL_DATA) ./files/wpa-wired-reference.conf $(1)/etc/802.1x/wpa-wired-reference.conf
endef

define Package/wpad-macsec-noextns/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpad $(1)/usr/sbin/wpad-macsec
	$(LN) wpad-macsec $(1)/usr/sbin/hostapd-macsec
	$(LN) wpad-macsec $(1)/usr/sbin/wpa_supplicant-macsec
	$(INSTALL_DIR) $(1)/etc/802.1x
	$(INSTALL_BIN) ./files/8021x-client $(1)/usr/sbin/8021x-client
	$(INSTALL_BIN) ./files/8021x_action.sh $(1)/etc/802.1x/8021x_action.sh
	$(INSTALL_BIN) ./files/dhcp_wan_renew.sh $(1)/etc/802.1x/dhcp_wan_renew.sh
	$(INSTALL_DATA) ./files/wpa-wired-reference.conf $(1)/etc/802.1x/wpa-wired-reference.conf
endef

ifneq ($(LOCAL_TYPE),supplicant)
ifneq ($(LOCAL_VARIANT),macsec)
  define Package/hostapd-utils/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd_cli $(1)/usr/sbin/
  endef

  define Package/hostapd-utils-noextns/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd_cli $(1)/usr/sbin/
  endef
endif
endif

define Package/wpad/install
	$(call Install/hostapd,$(1))
	$(call Install/supplicant,$(1))
	$(INSTALL_DIR) $(1)/etc/sysctl.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpad $(1)/usr/sbin/
	$(LN) wpad $(1)/usr/sbin/hostapd
	$(LN) wpad $(1)/usr/sbin/wpa_supplicant
	$(INSTALL_BIN) $(EXTERNAL_DIR)/files/hostapd.sysctl $(1)/etc/sysctl.d/hostapd.conf
	$(if $(CONFIG_WPA_MQTT_SUPPORT),$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/mqtt_test $(1)/usr/sbin/,)
	$(if $(and $(CONFIG_WPA_MQTT_SUPPORT),$(if $(filter noextns,$(subst -, ,$(BUILD_VARIANT))),,x)),$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/external_hif_mqtt_test_client $(1)/usr/sbin/,)
endef
Package/wpad-basic/install = $(Package/wpad/install)
Package/wpad-basic-openssl/install = $(Package/wpad/install)
Package/wpad-basic-wolfssl/install = $(Package/wpad/install)
Package/wpad-basic-mbedtls/install = $(Package/wpad/install)
Package/wpad-mini/install = $(Package/wpad/install)
Package/wpad-openssl/install = $(Package/wpad/install)
Package/wpad-wolfssl/install = $(Package/wpad/install)
Package/wpad-mbedtls/install = $(Package/wpad/install)
Package/wpad-mesh-openssl/install = $(Package/wpad/install)
Package/wpad-mesh-wolfssl/install = $(Package/wpad/install)
Package/wpad-mesh-mbedtls/install = $(Package/wpad/install)

define Package/wpa-supplicant/install
	$(call Install/supplicant,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_supplicant $(1)/usr/sbin/
endef
Package/wpa-supplicant-basic/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-mini/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-p2p/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-openssl/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-wolfssl/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-mbedtls/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-mesh-openssl/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-mesh-wolfssl/install = $(Package/wpa-supplicant/install)
Package/wpa-supplicant-mesh-mbedtls/install = $(Package/wpa-supplicant/install)


ifneq ($(LOCAL_TYPE),hostapd)
ifneq ($(LOCAL_VARIANT),macsec)
  define Package/wpa-cli/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/wpa_cli $(1)/usr/sbin/
  endef
endif
endif

ifeq ($(BUILD_VARIANT),supplicant-full-internal)
  define Package/eapol-test/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

ifeq ($(BUILD_VARIANT),supplicant-full-openssl)
  define Package/eapol-test-openssl/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

ifeq ($(BUILD_VARIANT),supplicant-full-wolfssl)
  define Package/eapol-test-wolfssl/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

ifeq ($(BUILD_VARIANT),supplicant-full-mbedtls)
  define Package/eapol-test-mbedtls/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa_supplicant/eapol_test $(1)/usr/sbin/
  endef
endif

# Build hostapd-common before its dependents, to avoid
# spurious rebuilds when building multiple variants.
$(eval $(call BuildPackage,hostapd-common))
$(eval $(call BuildPackage,hostapd))
$(eval $(call BuildPackage,hostapd-basic))
$(eval $(call BuildPackage,hostapd-basic-openssl))
#$(eval $(call BuildPackage,hostapd-basic-wolfssl))
$(eval $(call BuildPackage,hostapd-basic-mbedtls))
$(eval $(call BuildPackage,hostapd-mini))
#$(eval $(call BuildPackage,hostapd-openssl))
#$(eval $(call BuildPackage,hostapd-wolfssl))
$(eval $(call BuildPackage,hostapd-mbedtls))
$(eval $(call BuildPackage,wpad))
$(eval $(call BuildPackage,wpad-mesh-openssl))
$(eval $(call BuildPackage,wpad-mesh-openssl-noextns))
#$(eval $(call BuildPackage,wpad-mesh-wolfssl))
$(eval $(call BuildPackage,wpad-mesh-mbedtls))
$(eval $(call BuildPackage,wpad-basic))
$(eval $(call BuildPackage,wpad-basic-openssl))
#$(eval $(call BuildPackage,wpad-basic-wolfssl))
$(eval $(call BuildPackage,wpad-basic-mbedtls))
$(eval $(call BuildPackage,wpad-mini))
$(eval $(call BuildPackage,wpad-openssl))
#$(eval $(call BuildPackage,wpad-wolfssl))
$(eval $(call BuildPackage,wpad-mbedtls))
$(eval $(call BuildPackage,wpa-supplicant))
$(eval $(call BuildPackage,wpa-supplicant-mesh-openssl))
#$(eval $(call BuildPackage,wpa-supplicant-mesh-wolfssl))
$(eval $(call BuildPackage,wpa-supplicant-mesh-mbedtls))
$(eval $(call BuildPackage,wpa-supplicant-basic))
$(eval $(call BuildPackage,wpa-supplicant-mini))
$(eval $(call BuildPackage,wpa-supplicant-p2p))
#$(eval $(call BuildPackage,wpa-supplicant-openssl))
#$(eval $(call BuildPackage,wpa-supplicant-wolfssl))
$(eval $(call BuildPackage,wpa-supplicant-mbedtls))
$(eval $(call BuildPackage,wpa-cli))
$(eval $(call BuildPackage,hostapd-utils))
$(eval $(call BuildPackage,hostapd-utils-noextns))
$(eval $(call BuildPackage,eapol-test))
$(eval $(call BuildPackage,eapol-test-openssl))
#$(eval $(call BuildPackage,eapol-test-wolfssl))
$(eval $(call BuildPackage,eapol-test-mbedtls))
$(eval $(call BuildPackage,wpad-macsec))
$(eval $(call BuildPackage,wpad-macsec-noextns))
