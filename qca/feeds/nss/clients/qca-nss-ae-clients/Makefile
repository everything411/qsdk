ifndef __QCA_NSS_AE_CLIENTS_FEED_MAKEFILE__
__QCA_NSS_AE_CLIENTS_FEED_MAKEFILE__:=1

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ae-clients
PKG_LICENSE:=ISC
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

define KernelPackage/qca-nss-ppe-bridge-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE bridge manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr \
		+PACKAGE_kmod-bonding:kmod-bonding \
		+PACKAGE_kmod-qca-ovsmgr:kmod-qca-ovsmgr
  FILES:=$(PKG_BUILD_DIR)/clients/bridge/qca-nss-ppe-bridge-mgr.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-bridge-mgr)
endef

define KernelPackage/qca-nss-ppe-bridge-mgr/Description
Kernel modules for NSS PPE bridge manager
endef

define KernelPackage/qca-nss-ppe-vlan-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE vlan manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vp \
		+PACKAGE_kmod-bonding:kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/vlan/qca-nss-ppe-vlan.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-vlan)
endef

define KernelPackage/qca-nss-ppe-vlan-mgr/Description
Kernel modules for NSS PPE vlan manager
endef

define KernelPackage/qca-nss-ppe-pppoe-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE PPPoE manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx \
		+kmod-qca-nss-ppe \
		+kmod-pppoe @!LINUX_3_18 \
		+PACKAGE_kmod-bonding:kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/pppoe/qca-nss-ppe-pppoe-mgr.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-pppoe-mgr)
endef

define KernelPackage/qca-nss-ppe-pppoe-mgr/Description
Kernel modules for NSS PPE PPPoE manager
endef

define KernelPackage/qca-nss-ppe-lag-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS LAG manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr \
		+kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/lag/qca-nss-ppe-lag.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-lag)
endef

define KernelPackage/qca-nss-ppe-lag-mgr/Description
Kernel modules for NSS PPE LAG manager
endef

define KernelPackage/qca-nss-ppe-dsa-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE DSA manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr
  FILES:=$(PKG_BUILD_DIR)/clients/dsa/qca-nss-ppe-dsa-mgr.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-dsa-mgr)
endef

define KernelPackage/qca-nss-ppe-dsa-mgr/Description
Kernel modules for NSS PPE DSA manager
endef

define KernelPackage/qca-nss-ppe-ath-clients
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE ath clients
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx \
		+kmod-qca-nss-ppe-vp
  FILES:=$(PKG_BUILD_DIR)/clients/ath_client/qca-nss-ppe-ath-clients.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-ath-clients)
endef

define KernelPackage/qca-nss-ppe-ath-clients/Description
Kernel modules for NSS PPE ath clients
endef


EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-bridge-mgr),)
MAKE_OPTS+=bridge-mgr=y
# enable OVS bridge if ovsmgr is enabled
ifneq ($(CONFIG_PACKAGE_kmod-qca-ovsmgr),)
MAKE_OPTS+=NSS_PPE_BRIDGE_MGR_OVS_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-ovsmgr
endif
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-vlan-mgr),)
ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
EXTRA_CFLAGS+= -DNSS_VLAN_MGR_WLANIF_DST_XLATE_SUPPORT
endif

MAKE_OPTS+=vlan-mgr=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-pppoe-mgr),)
MAKE_OPTS+=pppoe-mgr=y
ifneq ($(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq54xx)$(CONFIG_TARGET_ipq96xx)$(CONFIG_TARGET_ipq52xx),)
MAKE_OPTS+=PPPOE_MGR_FE_PPE_ENABLE=y
endif
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-lag-mgr),)
MAKE_OPTS+=lag-mgr=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-dsa-mgr),)
MAKE_OPTS+=dsa-mgr=y
endif

ifeq ($(CONFIG_TARGET_ipq52xx_ipq52xx_32_QSDK_Sfu), y)
MAKE_OPTS+=PPE_SFU_PROFILE=y
endif

MAKE_OPTS+=ath_clients=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ppe

define KernelPackage/qca-nss-ppe-bridge-mgr/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-nss-ppe-bridge-mgr.init $(1)/etc/init.d/qca-nss-ppe-bridge-mgr
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/ppe_bridge_mgr_dump $(1)/usr/bin/
endef

define KernelPackage/qca-nss-ppe-lag-mgr/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/ppe_lag_dump $(1)/usr/bin/
endef

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-bridge-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-vlan-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-pppoe-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-lag-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-dsa-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-ath-clients))

endif # __QCA_NSS_AE_CLIENTS_FEED_MAKEFILE__
