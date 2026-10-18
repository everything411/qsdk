include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-gre-mgr
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/nss/clients/qca-nss-ae-clients

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

define KernelPackage/qca-nss-ppe-gre-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE GRE tunnel (client)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx +kmod-qca-nss-ppe-tun +kmod-gre6 +kmod-gre
  FILES:=$(PKG_BUILD_DIR)/clients/gre/qca-nss-ppe-gre.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-ppe-gre)
endef

define KernelPackage/qca-nss-ppe-gre-mgr/Description
Kernel module for PPE GRE tunnel client
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-gre-mgr,$(1))
endef

ifneq ($(CONFIG_TARGET_ipq52xx)$(CONFIG_TARGET_ipq96xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx),)
EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-dp \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-gre-mgr),)
MAKE_OPTS+=gre-mgr=y
ifneq ($(CONFIG_TARGET_ipq52xx)$(CONFIG_TARGET_ipq96xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx),)
MAKE_OPTS+=GRE_MGR_FE_PPE_ENABLE=y
endif
endif

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-gre-mgr))
