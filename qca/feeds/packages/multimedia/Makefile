# ==============================================================================
# OpenWrt package Makefile for Qualcomm FastRPC
# ==============================================================================

# Include OpenWrt top-level build rules and helper macros
include $(TOPDIR)/rules.mk


# ------------------------------------------------------------------------------
# Package metadata
# ------------------------------------------------------------------------------

# OpenWrt package name, version and release number
PKG_NAME:=fastrpc
PKG_VERSION:=0.1
PKG_RELEASE:=1


# ------------------------------------------------------------------------------
# Source retrieval configuration
# ------------------------------------------------------------------------------
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/qualcomm/fastrpc.git
PKG_SOURCE_VERSION:=8572ae1c45d38a4dc8853b1b9b6738207ab1ce94
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_BRANCH:=main
PKG_HASH:=5fb011318b6c5630f5c67d0b452ae37727af7a62a0c6af6df4b62c8d01ebde5c


# ------------------------------------------------------------------------------
# Licensing
# ------------------------------------------------------------------------------

# SPDX license identifier
PKG_LICENSE:=BSD-3-Clause
# File used by OpenWrt to validate licensing
PKG_LICENSE_FILES:=src/fastrpc_apps_user.c
PKG_FIXUP:=autoreconf

# Include common OpenWrt package infrastructure
include $(INCLUDE_DIR)/package.mk


# ------------------------------------------------------------------------------
# Package definition visible to menuconfig
# ------------------------------------------------------------------------------
define Package/fastrpc
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Qualcomm FastRPC applications and library
	URL:=https://github.com/qualcomm/fastrpc
	DEPENDS:=@(TARGET_ipq96xx||TARGET_ipq95xx) +libyaml +libbsd
endef


# ------------------------------------------------------------------------------
# Package description
# ------------------------------------------------------------------------------
define Package/fastrpc/description
	Qualcomm FastRPC applications and library.
endef


# ------------------------------------------------------------------------------
# Configuration files
# ------------------------------------------------------------------------------
define Package/fastrpc/conffiles
	/etc/udev/rules.d/fastrpc.rules
endef


# ------------------------------------------------------------------------------
# Build preparation step
# ------------------------------------------------------------------------------
define Build/Prepare
	# Default OpenWrt prepare:
	# - unpack source
	# - apply patches (if any)
	$(call Build/Prepare/Default)
	$(CP) ./files/fastrpc.rules $(PKG_BUILD_DIR)/
	chmod +x $(PKG_BUILD_DIR)/autogen.sh
endef


#------------------------------------------------------------------------------
# Configure step
# ------------------------------------------------------------------------------
define Build/Configure
	cd $(PKG_BUILD_DIR); ./autogen.sh
	$(call Build/Configure/Default)
endef

# ------------------------------------------------------------------------------
# Install step (rootfs staging)
# ------------------------------------------------------------------------------
define Package/fastrpc/install
	# Create target directories
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc/udev/rules.d
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/rc.d

	# -------------------------
	# Install cdsprpcd daemon
	# -------------------------
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/cdsprpcd $(1)/usr/bin/

	# -------------------------
	# Install shared libraries
	# -------------------------
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/.libs/libcdsp*.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/.libs/libcdsp*.so.* $(1)/usr/lib/

	# -------------------------
	# Install init script (not auto-started at boot; run manually via /etc/init.d/cdsprpcd start)
	# -------------------------
	$(INSTALL_BIN) ./files/cdsprpcd.init $(1)/etc/init.d/cdsprpcd

	# -------------------------
	# Install udev rules
	# -------------------------
	$(INSTALL_DATA) ./files/fastrpc.rules $(1)/etc/udev/rules.d/
endef


# ------------------------------------------------------------------------------
# Post-install script (runs on target)
# ------------------------------------------------------------------------------
define Package/fastrpc/postinst
	#!/bin/sh
	grep -q '^fastrpc:' /etc/group || groupadd fastrpc
	/bin/id fastrpc 2>/dev/null || useradd -g fastrpc -M -N -s /bin/false fastrpc
	exit 0
endef


# ------------------------------------------------------------------------------
# Register package with OpenWrt build system
# ------------------------------------------------------------------------------
$(eval $(call BuildPackage,fastrpc))
