include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-gretap
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-nss-ppe

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-nss-ppe-gretap
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE GRE tunnel (client)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx +kmod-qca-nss-ppe-tun +kmod-gre6 +kmod-gre
  FILES:=$(PKG_BUILD_DIR)/clients/gretap/qca-nss-ppe-gretap.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-ppe-gretap)
endef

define KernelPackage/qca-nss-ppe-gretap/Description
Kernel module for PPE GRE tunnel client
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-gretap,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-dp \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \

MAKE_OPTS+=gretap-mgr=y

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" $(strip $(MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-gretap))
