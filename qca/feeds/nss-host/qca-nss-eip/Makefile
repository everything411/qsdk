include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-eip
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

ifneq (, $(findstring $(subtarget),"ipq53xx","ipq54xx"))
NSS_EIP_DIR:=eip196
else
NSS_EIP_DIR:=eip197
endif

ifeq ($(CONFIG_PACKAGE_kmod-qca-nss-eip-crypto),y)
eip_crypto:=m
endif

define KernelPackage/qca-nss-eip
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
		+TARGET_ipq95xx:qca-nss-fw-eip-al \
		+TARGET_ipq95xx:kmod-qca-nss-ppe
  TITLE:=Kernel driver for NSS EIP driver
  FILES:=$(PKG_BUILD_DIR)/driver/$(NSS_EIP_DIR)/qca-nss-eip.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-eip)
endef

define KernelPackage/qca-nss-eip/Description
This package contains a NSS EIP driver for QCA chipset
endef

define KernelPackage/qca-nss-eip/install
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-eip
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/eip_dump.sh $(1)/usr/bin/
	$(CP) $(PKG_BUILD_DIR)/driver/exports/* $(1)/usr/include/qca-nss-eip
endef

define KernelPackage/qca-nss-eip-crypto
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq_ipq95xx_32||TARGET_ipq53xx||TARGET_ipq54xx \
		+kmod-qca-nss-eip \
		+kmod-crypto-authenc
  TITLE:=Kernel NSS EIP crypto client
  FILES:=$(PKG_BUILD_DIR)/clients/crypto/qca-nss-eip-crypto.ko
endef

define KernelPackage/qca-nss-eip-crypto/Description
This package contains a NSS EIP crypto client for QCA chipset
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-eip
	$(CP) $(PKG_BUILD_DIR)/clients/exports/* $(1)/usr/include/qca-nss-eip
	$(CP) $(PKG_BUILD_DIR)/driver/exports/* $(1)/usr/include/qca-nss-eip
endef

define KernelPackage/qca-nss-eip-ipsec
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
	  	+kmod-qca-nss-eip \
		kmod-qca-nss-ecm \
		+kmod-qca-nss-ppe-vp
  TITLE:=Kernel IPsec client for NSS EIP driver
  FILES:=$(PKG_BUILD_DIR)/clients/ipsec/qca-nss-eip-ipsec.ko
endef

define KernelPackage/qca-nss-eip-ipsec/Description
This package contains a IPsec offload client for EIP driver.
endef

define KernelPackage/qca-nss-eip-ipsec/install
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-eip
	$(CP) $(PKG_BUILD_DIR)/clients/exports/eip_ipsec.h $(1)/usr/include/qca-nss-eip
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-nss-ipsec $(1)/etc/init.d/qca-nss-ipsec
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-eip

EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ppe

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-eip-ipsec),)
EXTRA_CFLAGS+= -I$(PKG_BUILD_DIR)/clients/exports
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ecm
MAKE_OPTS+=eip_ipsec=m
endif

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		CC="$(TARGET_CC)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		NSS_EIP_DIR=$(NSS_EIP_DIR) \
		eip_crypto=$(eip_crypto) \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-eip))
$(eval $(call KernelPackage,qca-nss-eip-crypto))
$(eval $(call KernelPackage,qca-nss-eip-ipsec))
