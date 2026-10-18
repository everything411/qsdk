#
# OpenSync QSDK package
#
# Requires that OpenSync build root is in $(PKG_DIR)/src/
#

include $(TOPDIR)/rules.mk

PKG_NAME := opensync
PKG_VERSION := 6.4
PKG_RELEASE := 0
OPENSYNC_VERSION := $(PKG_VERSION).$(PKG_RELEASE)

ifeq (,$(subst ",,$(CONFIG_OPENSYNC_SRC))) #unqote string "
	OPENSYNC_SRC:=
else
	OPENSYNC_SRC:=$(CONFIG_OPENSYNC_SRC)
endif

ifneq ($(OPENSYNC_SRC),)
	PKG_REV=$(shell cd $(OPENSYNC_SRC)/core; git describe --dirty --long --always --abbrev=7| sed 's/.*-g//g')
	PKG_VERSION:=g$(PKG_REV)
	PKG_UNPACK=mkdir -p $(PKG_BUILD_DIR); $(CP) $(OPENSYNC_SRC) $(PKG_BUILD_DIR)/src
	PATCH_DIR:=no-patch-dir
endif

include $(INCLUDE_DIR)/package.mk

OPENSYNC_DEPS := openvswitch libev jansson zlib openssl-util libprotobuf-c libmosquitto libnl qca-cfg80211 kmod-qca-wifi
OPENSYNC_DEPS += libcurl libiw mxml libpcap qca-hapd-supp libnetfilter-conntrack libmnl
OPENSYNC_DEPS += libfdt libblkid
OPENSYNC_DEPS += libffi
OPENSYNC_DEPS += libzmq libpython3
OPENSYNC_DEPS += ubus
OPENSYNC_DEPS += @BUSYBOX_CONFIG_TIMEOUT
OPENSYNC_DEPS += busybox
OPENSYNC_DEVSHELL ?= n

OPENSYNC_DEPS := $(foreach X,$(OPENSYNC_DEPS),+$(X))
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_BUILD_DEPENDS:=protobuf-c/host python3-kconfiglib/host python3-pydot/host python-pyparsing/host python-jinja2/host
PKG_BUILD_DEPENDS+=openvswitch/host

define Package/opensync
  SECTION    := net
  CATEGORY   := OpenSync
  TITLE      := OpenSync QSDK package
  MAINTAINER := OpenSync <info@opensync.io>
  DEPENDS    := $(OPENSYNC_DEPS)
endef

define Package/opensync/description
  OpenSync QSDK package
endef

define Package/opensync/config
	source "$(SOURCE)/Config.in"
endef

define Build/Configure
	@echo === Build/Configure
endef

ifeq ($(OPENSYNC_SRC),)

define Download/core
	URL_FILE:=osync_$(OPENSYNC_VERSION).zip
	FILE:=opensync-core-osync_$(OPENSYNC_VERSION).zip
	URL:=https://github.com/plume-design/opensync/archive/
	HASH:=56687203a2c097dce22679f3877176dcb70276cec629344ccbdd97446f043859
endef
$(eval $(call Download,core))

define Download/platform
	URL_FILE:=osync_$(OPENSYNC_VERSION).zip
	FILE:=opensync-platform-osync_$(OPENSYNC_VERSION).zip
	URL:=https://github.com/plume-design/opensync-platform-qca/archive/
	HASH:=a52a4f6297f4f70f515e974674a03c0ca322fe9b6d342d89e9918f4bdb043dd1
endef
$(eval $(call Download,platform))

define Download/vendor
	URL_FILE:=osync_$(OPENSYNC_VERSION).zip
	FILE:=opensync-vendor-osync_$(OPENSYNC_VERSION).zip
	URL:=https://github.com/plume-design/opensync-vendor-qca-template/archive/
	HASH:=a138996ffbaf2d9beab10a7aa91a7e7a4e046fcf79cc8298884050792ed7f837
endef
$(eval $(call Download,vendor))

PKG_UNPACK=mkdir -p $(PKG_BUILD_DIR)/src; \
	unzip -q $(DL_DIR)/opensync-core-osync_$(OPENSYNC_VERSION).zip -d $(PKG_BUILD_DIR)/src && mv $(PKG_BUILD_DIR)/src/opensync-osync_$(OPENSYNC_VERSION) $(PKG_BUILD_DIR)/src/core; \
	mkdir -p $(PKG_BUILD_DIR)/src/platform; \
	unzip -q $(DL_DIR)/opensync-platform-osync_$(OPENSYNC_VERSION).zip -d $(PKG_BUILD_DIR)/src && mv $(PKG_BUILD_DIR)/src/opensync-platform-qca-osync_$(OPENSYNC_VERSION) $(PKG_BUILD_DIR)/src/platform/qca; \
	mkdir -p $(PKG_BUILD_DIR)/src/vendor; \
	unzip -q $(DL_DIR)/opensync-vendor-osync_$(OPENSYNC_VERSION).zip -d $(PKG_BUILD_DIR)/src && mv $(PKG_BUILD_DIR)/src/opensync-vendor-qca-template-osync_$(OPENSYNC_VERSION) $(PKG_BUILD_DIR)/src/vendor/qca-template;

endif

define Build/Prepare
	@echo === Build/Prepare
	@echo "PKG_BUILD_DIR: $(PKG_BUILD_DIR)"
	@echo "OPENSYNC_TARGET: $(CONFIG_OPENSYNC_TARGET)"
	@echo "STAGING_DIR: $(STAGING_DIR)"
	@echo "OPENSYNC_SRC: $(OPENSYNC_SRC)"

	$(Build/Prepare/Default)
endef

export MAKE_CMD := $(MAKE) -C $(PKG_BUILD_DIR)/src/core V=1 \
		TARGET=$(CONFIG_OPENSYNC_TARGET) \
		TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
		TARGET_CROSS=$(TARGET_CROSS) \
		TARGET_DIR_NAME=$(TARGET_DIR_NAME) \
		BACKHAUL_SSID=$(CONFIG_OPENSYNC_ONBOARD_SSID) \
		BACKHAUL_PASS=$(CONFIG_OPENSYNC_ONBOARD_PSK) \
		CONF_OPENSYNC_NL_SUPPORT=$(CONFIG_OPENSYNC_NL_SUPPORT) \
		ARCH_64BIT=$(CONFIG_ARCH_64BIT)

define Build/Compile
	@echo === Build/Compile
	@echo
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
	@echo "PKG_BUILD_DIR: $(PKG_BUILD_DIR)"
	@echo "OPENSYNC_TARGET: $(CONFIG_OPENSYNC_TARGET)"
	@echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>"

	test $(OPENSYNC_DEVSHELL) = n || { echo starting devshell...; $(SHELL); }
	eval $(MAKE_CMD) rootfs
	@echo
endef

define Package/opensync/install
	@echo ======Package/opensync/install
	$(MAKE) -C $(PKG_BUILD_DIR)/src/core rootfs-install-only \
		TARGET=$(CONFIG_OPENSYNC_TARGET) \
		INSTALL_ROOTFS_DIR=$(1)
endef

# Don't run RSTRIP as we need rpath for OpenSync lib
RSTRIP := @echo SKIP RSTRIP:

$(eval $(call BuildPackage,opensync))
