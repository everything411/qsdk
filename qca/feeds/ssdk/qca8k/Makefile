include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca8k
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

define Build/Compile
	#$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCASSDK_CONFIG_OPTS)) $(LNX_CONFIG_OPTS)
endef

define KernelPackage/qca8k/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/qca_dsa $(1)/usr/bin/
endef

$(eval $(call KernelPackage,qca8k))
