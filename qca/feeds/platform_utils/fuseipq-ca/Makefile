#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

include $(TOPDIR)/rules.mk

PKG_NAME:=fuseipq-ca
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/fuseipq-ca
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=FuseIPQ Client Application
  DEPENDS:=@TARGET_ipq52xx||TARGET_ipq96xx +optee-client
endef

define Package/fuseipq-ca/description
  Userspace OP-TEE Client Application for fuse provisioning on IPQ platforms.
  This application provides a userspace interface to blow fuses using the
  fuseipq Trusted Application (TA) running in OP-TEE secure world.
endef

TARGET_CFLAGS += -g3 -O0 -Werror -Wall \
	-I$(STAGING_DIR)/usr/include

TARGET_LDFLAGS += \
	-L$(STAGING_DIR)/usr/lib \
	-lteec -lpthread

define Package/fuseipq-ca/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fuseipq_ca $(1)/usr/bin/
	$(TARGET_CROSS)strip $(1)/usr/bin/fuseipq_ca
endef

$(eval $(call BuildPackage,fuseipq-ca))
