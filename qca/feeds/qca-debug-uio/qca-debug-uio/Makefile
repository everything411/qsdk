include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-debug-uio
PKG_RELEASE:=1
PKG_VERSION:=1

PKG_LICENSE:=ISC

ifeq (y, $(filter y, $(CONFIG_TARGET_echo_cpe) $(CONFIG_TARGET_sdx85_cpe-v1) $(CONFIG_TARGET_sdx85_cpe)))
LOCAL_SRC:=$(TOPDIR)/src/ipq/qca-debug-uio
-include $(TOPDIR)/owrt-qti-ipq-open/include/local-development.mk
else
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-debug-uio
-include $(INCLUDE_DIR)/local-development.mk
endif

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:=UIO debug driver
  DEPENDS:=
  FILES:=$(PKG_BUILD_DIR)/debug_uio.ko
  AUTOLOAD:=$(call AutoLoad,08,debug_uio)
endef

define KernelPackage/$(PKG_NAME)/Description
  UIO debug driver providing per-subsystem shared-memory debug channels
  to userspace.  Each registered subsystem gets a /dev/uioN device backed
  by an interrupt ring buffer (map 0) and a data ring buffer (map 1).
endef

define KernelPackage/$(PKG_NAME)/install
  $(INSTALL_DIR) $(1)/usr/bin
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		modules
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/qca-debug-uio
	$(CP) $(PKG_BUILD_DIR)/exports/debug_uio_public.h \
		$(1)/usr/include/qca-debug-uio/
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
