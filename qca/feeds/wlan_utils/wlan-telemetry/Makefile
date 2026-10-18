include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=telemetry-agent
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_LICENSE:=Qualcomm-Technologies-Inc.-Proprietary

ifeq ($(filter y,$(CONFIG_TARGET_sdx85_cpe) $(CONFIG_TARGET_sdx85_cpe-v1) $(CONFIG_TARGET_echo_cpe)),y)
LOCAL_SRC:=$(TOPDIR)/src/ipq/telemetry_agent
-include $(TOPDIR)/owrt-qti-ipq-open/include/local-development.mk
else
LOCAL_SRC:=$(TOPDIR)/qca/src/qca-wifi/telemetry_agent
-include $(INCLUDE_DIR)/local-development.mk
endif

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  TITLE:=Telemetry Agent
  DEPENDS+= kmod-ath12k
  FILES:=$(PKG_BUILD_DIR)/telemetry_agent.ko
endef

define KernelPackage/$(PKG_NAME)/description
	This package contains telemetry agent support for open source profile.
endef

MAKE_OPTS:= -C "$(PKG_BUILD_DIR)" \
	CROSS_COMPILE="$(KERNEL_CROSS)" \
	ARCH="$(LINUX_KARCH)" \
	KERNEL_PATH="${LINUX_DIR}" \
	BUILD_PROFILE="Open" \
	PWD="$(PKG_BUILD_DIR)"


ifeq ($(filter y,$(CONFIG_TARGET_sdx85_cpe) $(CONFIG_TARGET_sdx85_cpe-v1) $(CONFIG_TARGET_echo_cpe)),y)
MAKE_OPTS += KCFLAGS="-Wall -Wno-unused-variable -I$(STAGING_DIR)/usr/include/mac80211/ath"
else
MAKE_OPTS += EXTRA_CFLAGS="-Wall -Werror -Wno-unused-variable -I$(STAGING_DIR)/usr/include/mac80211/ath"
endif

define Build/InstallDev
        $(CP) $(PKG_BUILD_DIR)/inc/telemetry_agent_app_if.h $(STAGING_DIR)/usr/include
endef

define Build/Compile
	$(KERNEL_MAKE) $(MAKE_OPTS) all
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
