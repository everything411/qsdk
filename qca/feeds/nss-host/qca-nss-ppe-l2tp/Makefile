include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-l2tp
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

define KernelPackage/qca-nss-ppe-l2tp
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE L2TP tunnel (client)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-qca-nss-ppe-tun +kmod-ppp +kmod-l2tp +kmod-pppol2tp
  FILES:=$(PKG_BUILD_DIR)/clients/l2tp/qca-nss-ppe-l2tp.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-ppe-l2tp)
endef

define KernelPackage/qca-nss-ppe-l2tp/Description
Kernel module for PPE L2TP-V2 tunnel client
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-l2tp,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(LINUX_DIR)/net/l2tp/ \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \

MAKE_OPTS+=l2tp-mgr=y

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-l2tp))
