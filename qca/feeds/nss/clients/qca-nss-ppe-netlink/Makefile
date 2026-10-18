include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-netlink
PKG_LICENSE:=ISC
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/nss/base/qca-nss-ppe

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

define KernelPackage/qca-nss-ppe-netlink
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=PPE netlink Manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq96xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq52xx +TARGET_ipq52xx:kmod-qca-nss-dp +TARGET_ipq96xx:kmod-qca-nss-dp +kmod-qca-nss-ppe +kmod-qca-nss-ppe-rule @!LINUX_3_18
  FILES:=$(PKG_BUILD_DIR)/netlink/qca-nss-ppe-netlink.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-ppe-netlink)
endef

define KernelPackage/qca-nss-ppe-netlink/Description
Kernel module for PPE netlink manager
endef

#TODO: Resolve the unnecessary dependency on ssdk

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init

ifeq ($(CONFIG_TARGET_ipq96xx)$(CONFIG_TARGET_ipq52xx), y)
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-dp
endif

MAKE_OPTS+=netlink=y

ifneq ($(CONFIG_TARGET_ipq52xx)$(CONFIG_TARGET_ipq96xx),)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
MAKE_OPTS+=PPE_MIRROR_ENABLED=y
endif
else
ifneq ($(CONFIG_LOWMEM_FLASH),y)
ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
MAKE_OPTS+=PPE_MIRROR_ENABLED=y
endif
endif
endif

ifneq ($(CONFIG_TARGET_ipq52xx)$(CONFIG_TARGET_ipq96xx),)
ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
MAKE_OPTS+=PPE_TUN_RPS_ENABLED=y
endif
endif
endif

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

define Build/InstallDev/qca-nss-ppe-netlink
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ppe
	$(CP) $(PKG_BUILD_DIR)/netlink/include/* $(1)/usr/include/qca-nss-ppe/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-netlink,$(1))
endef

$(eval $(call KernelPackage,qca-nss-ppe-netlink))
