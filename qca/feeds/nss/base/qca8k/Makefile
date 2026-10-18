include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca8k
PKG_LICENSE:=GPL-2.0
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
	PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

ifeq ($(LINUX_VERSION), $(filter 5.4%, $(LINUX_VERSION)))
	QCA8K_KO_PATH=$(LINUX_DIR)/drivers/net/dsa/qca8k.ko
else
	QCA8K_KO_PATH=$(LINUX_DIR)/drivers/net/dsa/qca/qca8k.ko
endif

define KernelPackage/qca8k
  SUBMENU:=$(NETWORK_DEVICES_MENU)
  TITLE:=Kernel driver for QCA DSA
  DEPENDS:=@TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq807x||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq60xx||TARGET_ipq_ipq50xx_64||TARGET_ipq_ipq50xx||TARGET_ipq50xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
  KCONFIG:=CONFIG_NET_DSA_QCA8K
  FILES:=$(QCA8K_KO_PATH)
endef

define KernelPackage/qca8k/description
Kernel driver for QCA DSA
endef

define KernelPackage/qce2k
  SUBMENU:=$(NETWORK_DEVICES_MENU)
  TITLE:=Kernel driver for QCE2204 DSA
  DEPENDS:=@(TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx) +kmod-qca-nss-dp
  KCONFIG:=CONFIG_NET_DSA_QCE2204 CONFIG_PCS_QCE2204
  FILES:= \
	$(LINUX_DIR)/drivers/net/pcs/pcs-qce2204.ko \
	$(LINUX_DIR)/drivers/net/dsa/qca/qce2204_dsa.ko
  AUTOLOAD:=$(call AutoLoad,31,pcs-qce2204) \
	$(call AutoLoad,32,qce2204_dsa)
endef
define KernelPackage/qce2k/description
Kernel driver for QCE DSA
endef

define Build/Compile
	#$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCASSDK_CONFIG_OPTS)) $(LNX_CONFIG_OPTS)
endef

define Install/qca_dsa
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/qca_dsa $(1)/usr/bin/
endef

define KernelPackage/qca8k/install
	$(call Install/qca_dsa,$(1))
endef

ifeq ($(filter y m,$(CONFIG_PACKAGE_kmod-qca8k)),)
define KernelPackage/qce2k/install
	$(call Install/qca_dsa,$(1))
endef
endif

$(eval $(call KernelPackage,qca8k))
$(eval $(call KernelPackage,qce2k))
