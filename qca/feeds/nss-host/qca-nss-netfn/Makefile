include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-netfn
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

PKG_BUILD_DEPENDS :=
MAKE_OPTS:=
MAKE_OPTS+=NETFN_FLOWMGR_AE_PPE_ENABLE=y
MAKE_OPTS+=NETFN_FLOWMGR_AE_SFE_ENABLE=y

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-nss-netfn-tcpst
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=+kmod-qca-nss-ppe \
	  +kmod-qca-nss-ppe-vp \
	  +kmod-qca-nss-flowmgr
  TITLE:=NSS network function tcp speedtest offload
  FILES:=$(PKG_BUILD_DIR)/offload/tcpst/qca-nss-netfn-tcpst.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-tcpst),)
MAKE_OPTS+=tcpst=y
endif

define KernelPackage/qca-nss-netfn-tcpst/Description
NSS network function tcpst offload module.
endef

define KernelPackage/qca-nss-netfn-tcpst/install
	$(INSTALL_DIR) $(1)/usr/bin
endef

define KernelPackage/qca-nss-netfn-pkt-steer
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx
  TITLE:=NSS network function for pkt steer
  FILES:=$(PKG_BUILD_DIR)/offload/pkt_steer/qca-nss-netfn-pkt-steer.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-netfn-pkt-steer)
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-pkt-steer),)
MAKE_OPTS+=pkt_steer=y
endif

define KernelPackage/qca-nss-netfn-pkt-steer/Description
NSS network function for pkt steer.
endef

define KernelPackage/qca-nss-netfn-pkt-steer/install
	$(INSTALL_DIR) $(1)/usr/bin
endef

define KernelPackage/qca-nss-netfn-capwap
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq53xx \
	  +kmod-qca-nss-ppe \
	  +kmod-qca-nss-ppe-vp \
	  +kmod-qca-nss-netfn-dtls \
	  +kmod-qca-nss-netfn-pkt-steer \
	  +kmod-qca-nss-netfn-flow-cookie
  TITLE:=NSS network function capwap offload
  FILES:=$(PKG_BUILD_DIR)/offload/capwap/qca-nss-netfn-capwap.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-capwap),)
MAKE_OPTS+=capwap=y
endif

define KernelPackage/qca-nss-netfn-capwap/Description
NSS network function capwap offload module.
endef

define KernelPackage/qca-nss-netfn-capwap/install
	$(INSTALL_DIR) $(1)/usr/bin
endef

define KernelPackage/qca-nss-flowmgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for Network function manager
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq95xx \
	  +kmod-qca-nss-ppe \
	  +kmod-qca-nss-sfe
  FILES:=$(PKG_BUILD_DIR)/mgr/flowmgr/qca-nss-flowmgr.ko
  AUTOLOAD:=$(call AutoLoad,50,qca-nss-flowmgr)
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-flowmgr),)
 MAKE_OPTS+=flowmgr=y
endif

define KernelPackage/qca-nss-flowmgr/Description
	Networking module for network function flow manager.
endef

define Build/InstallDev/qca-nss-flowmgr
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-netfn
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-netfn/
endef

define KernelPackage/qca-nss-netfn-flow-cookie
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for network function flow Cookie manager
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx
  FILES:=$(PKG_BUILD_DIR)/offload/flow_cookie/qca-nss-netfn-flow-cookie.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-flow-cookie),)
MAKE_OPTS+=flowcookie=y
endif

define KernelPackage/qca-nss-netfn-flow-cookie/Description
Kernel modules for network function flow Cookie manager
endef


define Build/InstallDev/qca-nss-netfn-flow-cookie
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-netfn
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-netfn/
endef

define KernelPackage/qca-nss-netfn-flow-cookie/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) files/netfn_flow_dump $(1)/usr/bin/
endef

MAKE_OPTS+=netlink=y

define KernelPackage/qca-nss-netfn-auto-flowmgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Network function test manager netfn auto flowmgr
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx \
		+kmod-qca-nss-flowmgr
  FILES:=$(PKG_BUILD_DIR)/netfn_auto/flowmgr/qca-nss-netfn-auto-flowmgr.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-auto-flowmgr),)
MAKE_OPTS+=netfn_auto_flowmgr=y
endif

define KernelPackage/qca-nss-netfn-auto-flowmgr/Description
Network function test manager netfn auto flowmgr
endef

define Build/InstallDev/qca-nss-netfn-auto-flowmgr
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-netfn
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-netfn/
	$(CP) $(PKG_BUILD_DIR)/netfn_auto/include/* $(1)/usr/include/qca-nss-netfn/

endef

define KernelPackage/qca-nss-netfn-auto-flowcookie
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Network function test manager netfn auto flowcookie
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx \
  		+kmod-qca-nss-netfn-flow-cookie
  FILES:=$(PKG_BUILD_DIR)/netfn_auto/flowcookie/qca-nss-netfn-auto-flowcookie.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-auto-flowcookie),)
MAKE_OPTS+=netfn_auto_flowcookie=y
endif

define KernelPackage/qca-nss-netfn-auto-flowcookie/Description
Network function test manager netfn auto flowcookie
endef

define Build/InstallDev/qca-nss-netfn-auto-flowcookie
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-netfn
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-netfn/
	$(CP) $(PKG_BUILD_DIR)/netfn_auto/include/* $(1)/usr/include/qca-nss-netfn/

endef

define KernelPackage/qca-nss-netfn-auto-capwapmgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Network function test manager netfn auto capwapmgr
  DEPENDS:=@TARGET_ipq53xx \
	+kmod-qca-nss-netfn-capwapmgr
  FILES:=$(PKG_BUILD_DIR)/netfn_auto/capwapmgr/qca-nss-netfn-auto-capwapmgr.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-auto-capwapmgr),)
MAKE_OPTS+=netfn_auto_capwapmgr=y
endif

define KernelPackage/qca-nss-netfn-auto-capwapmgr/Description
Network function test manager netfn auto capwapmgr
endef

define Build/InstallDev/qca-nss-netfn-auto-capwapmgr
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-netfn
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-netfn/
	$(CP) $(PKG_BUILD_DIR)/netfn_auto/include/* $(1)/usr/include/qca-nss-netfn/

endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-flowmgr,$(1))
	$(call Build/InstallDev/qca-nss-netfn-flow-cookie,$(1))
	$(call Build/InstallDev/qca-nss-netfn-auto-flowmgr,$(1))
	$(call Build/InstallDev/qca-nss-netfn-auto-flowcookie,$(1))
	$(call Build/InstallDev/qca-nss-netfn-auto-capwapmgr,$(1))
endef

define KernelPackage/qca-nss-netfn-dtls
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq53xx \
	  +kmod-qca-nss-ppe-vp \
	  +kmod-qca-nss-eip
  TITLE:=NSS network function DTLS offload
  FILES:=$(PKG_BUILD_DIR)/offload/dtls/qca-nss-netfn-dtls.ko
endef

define KernelPackage/qca-nss-netfn-dtls/Description
  NSS network function DTLS offload module.
endef

define KernelPackage/qca-nss-netfn-dtls/install
  $(INSTALL_DIR) $(1)/usr/bin
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-dtls),)
  EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-eip
  MAKE_OPTS+=dtls=m
endif

define KernelPackage/qca-nss-netfn-capwapmgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq53xx \
	  +kmod-qca-nss-netfn-capwap \
	  +kmod-qca-nss-netfn-dtls \
	  +kmod-qca-nss-flowmgr
  TITLE:=NSS network function CAPWAP manager
  FILES:=$(PKG_BUILD_DIR)/mgr/capwapmgr/qca-nss-netfn-capwapmgr.ko
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-netfn-capwapmgr),)
 EXTRA_CFLAGS += -I$(STAGING_DIR)/usr/include/qca-nss-ppe
 EXTRA_CFLAGS += -DNETFN_CAPWAPMGR_ONE_NETDEV
 MAKE_OPTS+=capwapmgr=y
endif

define KernelPackage/qca-nss-netfn-capwapmgr/Description
NSS network function capwapmanager.
endef

define Build/InstallDev/qca-nss-netfn-capwapmgr
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-netfn
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-netfn/
endef

define Build/InstallDev
        $(call Build/InstallDev/qca-nss-netfn-capwapmgr,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(PKG_BUILD_DIR)/exports \
	-I$(PKG_BUILD_DIR)/offload/exports \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-nss-sfe

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-netfn-tcpst))
$(eval $(call KernelPackage,qca-nss-netfn-capwap))
$(eval $(call KernelPackage,qca-nss-netfn-pkt-steer))
$(eval $(call KernelPackage,qca-nss-flowmgr))
$(eval $(call KernelPackage,qca-nss-netfn-dtls))
$(eval $(call KernelPackage,qca-nss-netfn-capwapmgr))
$(eval $(call KernelPackage,qca-nss-netfn-flow-cookie))
$(eval $(call KernelPackage,qca-nss-netfn-auto-flowmgr))
$(eval $(call KernelPackage,qca-nss-netfn-auto-flowcookie))
$(eval $(call KernelPackage,qca-nss-netfn-auto-capwapmgr))
