include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-sxc-drv
PKG_SOURCE_PROTO:=git
PKG_RELEASE:=1
LOCAL_SRC:=$(TOPDIR)/qca/src/nss/base/qca-sxc-drv/

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

MAKE_OPTS :=
PKG_BUILD_DEPENDS :=

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-sdc-drv
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for smart device classification
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx +kmod-qca-mmap-telemetry
  FILES:=$(PKG_BUILD_DIR)/sdc/qca-sdc-drv.ko
endef

define KernelPackage/qca-sdc-drv/Description
Kernel driver for QCA Smart Device classification
endef


define Build/InstallDev
	$(call Build/InstallDev/qca-sdc-drv,$(1))
endef

define KernelPackage/qca-sac-drv
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for smart application classification
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx +kmod-qca-mmap-telemetry
  FILES:=$(PKG_BUILD_DIR)/sac/qca-sac-drv.ko
endef

define KernelPackage/qca-sac-drv/Description
Kernel driver for QCA Smart Application Classification
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-sac-drv,$(1))
endef

EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/mmap-telemetry

ifneq ($(CONFIG_PACKAGE_kmod-qca-sdc-drv),)
MAKE_OPTS+=qca-sdc-drv=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-sac-drv),)
MAKE_OPTS+=qca-sac-drv=y
endif

define KernelPackage/qca-sdc-drv/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-sxc-drv.init $(1)/etc/init.d/qca-sxc-drv
endef

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-sdc-drv))
$(eval $(call KernelPackage,qca-sac-drv))
