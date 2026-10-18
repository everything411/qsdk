include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-fls
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-nss-nsm/fls
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-qca-nss-sfe +kmod-qca-nss-ecm
  TITLE:=Flow Statistics
  FILES:=$(PKG_BUILD_DIR)/qca-nss-fls.ko
endef

define KernelPackage/$(PKG_NAME)/Description
Flow Identification
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/qca-nss-fls
	$(CP) $(PKG_BUILD_DIR)/*.h $(STAGING_DIR)/usr/include/qca-nss-fls/
endef

EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-sfe
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ecm

ifneq ($(CONFIG_PACKAGE_kmod-emesh-sp),)
FLS_MAKE_OPTS+=FLS_ECM_CLASSIFIER_EMESH_ENABLE=y
endif

define Build/Compile
	$(KERNEL_MAKE) $(strip $(FLS_MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
