include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-fls-lite
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
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-qca-nss-ecm
  TITLE:=Flow Statistics lite version
  FILES:=$(PKG_BUILD_DIR)/qca-nss-fls-lite.ko
endef

define KernelPackage/$(PKG_NAME)/Description
Flow Identification lite version
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/qca-nss-fls-lite
	$(CP) $(PKG_BUILD_DIR)/*.h $(STAGING_DIR)/usr/include/qca-nss-fls-lite/
endef

EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ecm

FLS_LITE_MAKE_OPTS+=FLS_LITE_ENABLE=y

ifneq ($(CONFIG_PACKAGE_kmod-emesh-sp),)
FLS_LITE_MAKE_OPTS+=FLS_ECM_CLASSIFIER_EMESH_ENABLE=y
endif

define Build/Compile
	$(KERNEL_MAKE) $(strip $(FLS_LITE_MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,$(PKG_NAME)))