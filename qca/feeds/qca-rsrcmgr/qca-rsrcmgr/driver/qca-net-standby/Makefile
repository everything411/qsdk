include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk
PKG_NAME:=rsrcmgr-netstandby-drv
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-rsrcmgr/driver/qca-net-standby/
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif
PKG_BUILD_DEPENDS :=
MAKE_OPTS:=
ifeq ($(CONFIG_DEFAULT_kmod-ath12k),y)
OPEN_PROFILE=y
endif
ifeq ($(CONFIG_DEFAULT_kmod-ath11k),y)
OPEN_PROFILE=y
endif

ifeq ($(CONFIG_TARGET_ipq53xx), y)
TARGET_CFLAGS += -DMHT_ETH_INTF
TARGET_CFLAGS += -DTARGET_IPQ53xx
endif

ifeq ($(CONFIG_TARGET_ipq54xx), y)
TARGET_CFLAGS += -DMHT_ETH_INTF
TARGET_CFLAGS += -DTARGET_IPQ54xx
endif

ifeq ($(CONFIG_RM_QCA_256M), y)
EXTRA_CFLAGS += -DRM_QCA_256M_PROFILE=1
endif

define KernelPackage/rsrcmgr-netstandby-drv
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Standby module
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-qca-nss-ppe +kmod-qca-nss-ppe-rule +kmod-qca-nss-dp +kmod-nf-conntrack +PACKAGE_kmod-cfg80211-internal:kmod-qca-wifi
  FILES:=$(PKG_BUILD_DIR)/qca-net-standby.ko
endef

define KernelPackage/rsrcmgr-netstandby-drv/Description
Kernel module for ErP network standby framework to interface
with subsystem drivers and netstandby application library
endef
EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \
	-I$(STAGING_DIR)/usr/include/qca-nss-dp \
	-I$(LINUX_DIR)/user_headers/include/

# Enable wifi reg in netstandby driver for prop
ifneq ($(OPEN_PROFILE),y)
EXTRA_CFLAGS+= -DRM_QCA_PROP
#ERP phase 2 is enabled only for prop & 5.4 kernel
MAKE_OPTS+=erp_phase_2=y
endif

define Build/Compile
	$(KERNEL_MAKE) -C "$(LINUX_DIR)" $(strip $(MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef
# TODO: Can we copy on the data structures and API relevant for user space?
define Build/InstallDev/rsrcmgr-netstandby-drv
	$(INSTALL_DIR) $(1)/usr/include/rsrcmgr-netstandby-drv
	$(CP) $(PKG_BUILD_DIR)/include/* $(1)/usr/include/rsrcmgr-netstandby-drv/
endef
define Build/InstallDev
	$(call Build/InstallDev/rsrcmgr-netstandby-drv,$(1))
endef
$(eval $(call KernelPackage,rsrcmgr-netstandby-drv))
