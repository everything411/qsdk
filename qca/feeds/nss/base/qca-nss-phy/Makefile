include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-phy
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/nss/base/qca-nss-phy

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-nss-phy
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA-NSS-PHY driver
  FILES:=$(PKG_BUILD_DIR)/nss_ext/qca-nss-phy.ko \
	    $(if $(filter m, $(CONFIG_SFP)),$(LINUX_DIR)/drivers/net/phy/sfp.ko)

  PROVIDES:=qca-nss-phy
  DEPENDS:=+kmod-qca-mdio
  AUTOLOAD:=$(call AutoLoad,08,qca-nss-phy \
	    $(if $(filter m, $(CONFIG_SFP)),sfp))
endef

define KernelPackage/qca-mdio
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA MDIO driver
  KCONFIG:= \
	CONFIG_MDIO_BITBANG \
	CONFIG_MDIO_GPIO \
	CONFIG_MDIO_IPQ4019 \
	CONFIG_MDIO_I2C
  FILES:= \
	$(if $(filter m, $(CONFIG_MDIO_GPIO)),$(LINUX_DIR)/drivers/net/mdio/mdio-gpio.ko) \
	$(if $(filter m, $(CONFIG_MDIO_BITBANG)),$(LINUX_DIR)/drivers/net/mdio/mdio-bitbang.ko) \
	$(if $(filter m, $(CONFIG_MDIO_IPQ4019)),$(LINUX_DIR)/drivers/net/mdio/mdio-ipq4019.ko) \
	$(if $(filter m, $(CONFIG_MDIO_I2C)),$(LINUX_DIR)/drivers/net/mdio/mdio-i2c.ko) \
	$(PKG_BUILD_DIR)/linux_std/mdio_ahb/mdio-ahb.ko
  AUTOLOAD:=$(call AutoLoad,01,$(if $(filter m, $(CONFIG_MDIO_IPQ4019)),mdio-ipq4019) \
	$(if $(filter m, $(CONFIG_MDIO_BITBANG)),mdio-bitbang) \
	$(if $(filter m, $(CONFIG_MDIO_GPIO)),mdio-gpio) \
	$(if $(filter m, $(CONFIG_MDIO_I2C)),mdio-i2c) \
	mdio-ahb)
endef

define KernelPackage/qca-mdio/description
 QCOM MDIO driver and MDIO GPIO driver
endef

define KernelPackage/qca-nss-phy/Description
This package contains drivers for QCA PHYs such as Manhattan PHY and NAPA PHY.
It also contains vendor specific extensions for QCA PHYs which are not supported
by upstream drivers.
endef

define KernelPackage/qcom-lnx-phy
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA81XX PHY driver
  FILES:=$(PKG_BUILD_DIR)/linux_std/qcom-lnx-phy.ko
  DEPENDS:=+kmod-hwmon-core
  PROVIDES:=qcom-lnx-phy
  AUTOLOAD:=$(call AutoLoad,07,qcom-lnx-phy)
endef

define KernelPackage/qcom-lnx-phy/Description
This package contains qca81xx, qce1204 linux phy driver, it will be upstream PHY driver
of QCA8111 PHY, QCE1204 PHY.
endef

define KernelPackage/qca8084
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA8084 PHY driver
  FILES:=$(PKG_BUILD_DIR)/linux_std/qca8084/qca8084-phy.ko
  DEPENDS:=+kmod-qca8k-cc
  PROVIDES:=qca8084
  AUTOLOAD:=$(call AutoLoad,07,qca8084-phy)
endef

define KernelPackage/qca8084/Description
This package contains qca8084 Linux PHY driver
endef

define KernelPackage/qca8xxx-phc
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA8xxx PHC driver
  DEPENDS:=+kmod-qcom-lnx-phy
  FILES:=$(PKG_BUILD_DIR)/linux_std/ptp/qca8xxx-phc.ko
  AUTOLOAD:=$(call AutoLoad, 31, qca8xxx-phc)
endef

define KernelPackage/qca8xxx-phc/Description
Linux PHC driver of QCA8XXX PHY
endef

define KernelPackage/qca8k-cc
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA8K clock controller driver
  FILES:=$(PKG_BUILD_DIR)/linux_std/clock/qca8k-cc.ko
  AUTOLOAD:=$(call AutoLoad, 06, qca8k-cc)
endef

define KernelPackage/qca8k-cc/Description
Clock driver for QCA8386 and QCA8084 devices.
endef

define KernelPackage/qcom-lnx-phy-macsec
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Linux MACsec offloading to PHY driver
  FILES:=$(if $(filter y m,$(CONFIG_MACSEC)),$(PKG_BUILD_DIR)/linux_std/macsec/qcom-lnx-phy-macsec.ko)
  DEPENDS:=+kmod-macsec
  KCONFIG:=CONFIG_MACSEC
  AUTOLOAD:=$(if $(filter y m,$(CONFIG_MACSEC)),$(call AutoLoad,33,qcom-lnx-phy-macsec))
endef

define KernelPackage/qcom-lnx-phy-macsec/Description
This package includes the QCOM Linux MACsec driver, which will
become the upstream driver for MACsec offloading to PHY in Linux kernel.
endef

ifdef CONFIG_TARGET_NAME
QCA_NSS_PHY_CONFIG_OPTS+= \
	TARGET_NAME=$(CONFIG_TARGET_NAME)
endif

QCA_NSS_PHY_CONFIG_OPTS+= \
	LINUX_DIR=$(LINUX_DIR) \
	CROSS_COMPILE=$(TARGET_CROSS) \
	ARCH=$(LINUX_KARCH)

ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
QCA_NSS_PHY_CONFIG_OPTS+= nss-phy-ptp=y
endif
endif

SUPPORTED_PHYS = CONFIG_NSSPHY_QCA808X CONFIG_NSSPHY_QCA81XX CONFIG_NSSPHY_QCA803X \
		 CONFIG_NSSPHY_QCE1204

ifneq (, $(findstring $(CONFIG_TARGET_BOARD), "ipq95xx" "ipq52xx" "ipq96xx"))
    SUPPORTED_PHYS+=CONFIG_NSSPHY_QCA807X
endif

ifneq (, $(findstring $(CONFIG_TARGET_BOARD), "ipq52xx"))
    SUPPORTED_PHYS+=CONFIG_NSSPHY_IPQ52XX
endif

QCA_NSS_PHY_CONFIG_OPTS+= supported_phys="$(SUPPORTED_PHYS)"

define Build/Prepare
	$(CP) $(LOCAL_SRC)/* $(PKG_BUILD_DIR)/  -a
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" \
		$(strip $(QCA_NSS_PHY_CONFIG_OPTS)) \
		modules
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/nss_ext/*h $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/linux_std/clock/*h $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/linux_std/*h $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/linux_std/mdio_ahb/*h $(STAGING_DIR)/usr/include/qca-nss-phy
endef

define KernelPackage/qca-nss-phy/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-nss-phy.init $(1)/etc/init.d/qca-nss-phy
endef

$(eval $(call KernelPackage,qca-mdio))
$(eval $(call KernelPackage,qca8k-cc))
$(eval $(call KernelPackage,qca8084))
$(eval $(call KernelPackage,qcom-lnx-phy))
$(eval $(call KernelPackage,qca8xxx-phc))
$(eval $(call KernelPackage,qca-nss-phy))
$(eval $(call KernelPackage,qcom-lnx-phy-macsec))
