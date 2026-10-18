include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-mmap-telemetry
LOCAL_SRC:=$(TOPDIR)/qca/src/nss/base/qca-mmap-telemetry
PKG_RELEASE:=1
PKG_VERSION:=1

include $(INCLUDE_DIR)/local-development.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:= UIO driver for MMAP Telemetry
  DEPENDS:= @TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx
  FILES:=$(PKG_BUILD_DIR)/mmap-telemetry.ko
  AUTOLOAD:=$(call AutoLoad,08,mmap-telemetry)
endef

define KernelPackage/$(PKG_NAME)/Description
  uio driver for MMAP telemetry
endef

define KernelPackage/$(PKG_NAME)/install
  $(INSTALL_DIR) $(1)/usr/bin
endef

define Build/Compile
  $(MAKE) $(EXTRA_CFLAGS) -C "$(LINUX_DIR)" \
    CROSS_COMPILE="$(TARGET_CROSS)" \
    ARCH="$(LINUX_KARCH)" \
    M="$(PKG_BUILD_DIR)" \
    modules
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/mmap-telemetry
	$(CP) $(PKG_BUILD_DIR)/exports/mmap_telemetry_public.h $(1)/usr/include/mmap-telemetry
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
