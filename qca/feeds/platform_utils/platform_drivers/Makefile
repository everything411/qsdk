include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk


PKG_NAME:=platform_drivers
PKG_VERSION:=1

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

OTHER_MENU:=Other modules

define KernelPackage/bootconfig
  SUBMENU:=$(OTHER_MENU)
  TITLE:=Bootconfig partition for failsafe
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq53xx
  KCONFIG:=CONFIG_BOOTCONFIG_PARTITION
  FILES:=$(LINUX_DIR)/drivers/platform/ipq/bootconfig.ko
  AUTOLOAD:=$(call AutoLoad,56,bootconfig,1)
endef

define KernelPackage/bootconfig/description
  Bootconfig partition for failsafe
endef

$(eval $(call KernelPackage,bootconfig))

define KernelPackage/qseecom
  SUBMENU:=$(OTHER_MENU)
  TITLE:=QSEECOM Client Test Driver
  KCONFIG:=CONFIG_QSEECOM
  FILES:=$(LINUX_DIR)/drivers/misc/qseecom.ko
  AUTOLOAD:=$(call AutoLoad,09,qseecom)
endef

define KernelPackage/qseecom/description
 QSEECOM Client Test Driver based on SCM interface
endef

$(eval $(call KernelPackage,qseecom))

define KernelPackage/test-udelay
  CATEGORY:=Kernel modules
  SUBMENU:=$(OTHER_MENU)
  TITLE:=Test module for Udelay
  KCONFIG:=CONFIG_TEST_UDELAY
  FILES:=$(LINUX_DIR)/kernel/time/test_udelay.ko
endef

define KernelPackage/test-udelay/description
  This test module provides sysfs interface to test that udelay() is working
properly.
endef

$(eval $(call KernelPackage,test-udelay))

SOUND_MENU:=Sound Support

define AddPlatformDepends/sound
  SUBMENU:=$(SOUND_MENU)
  DEPENDS+=kmod-sound-core $(1)
endef

define KernelPackage/sound-soc-ipq
  TITLE:=Qualcomm-Atheros IPQ board soundcard support
  DEPENDS:=+kmod-sound-soc-core
  KCONFIG:= \
	CONFIG_SND_QCA_SOC \
	CONFIG_SND_SOC_QCOM=y \
	CONFIG_SND_SOC_IPQ=y \
	CONFIG_SND_SOC_IPQ_CODEC=y
  FILES:= \
	$(LINUX_DIR)/sound/soc/qcom/ipq/snd-soc-ipq-codec.ko
  $(call AddPlatformDepends/sound)
endef

define KernelPackage/sound-soc-ipq/description
 This enables sound card support for Qualcomm-Atheros IPQ board
endef

$(eval $(call KernelPackage,sound-soc-ipq))

CRYPTO_MENU:=Cryptographic API modules

define AddPlatformDepends/crypto
  SUBMENU:=$(CRYPTO_MENU)
  DEPENDS+=+LINUX_4_4:kmod-crypto-core $(1)
endef

define KernelPackage/crypto-aes-internal
  TITLE:=AES cipher CryptoAPI module
  KCONFIG:=CONFIG_CRYPTO_AES CONFIG_CRYPTO_AES_586
  FILES:=$(LINUX_DIR)/crypto/aes_generic.ko
  AUTOLOAD:=$(call AutoLoad,09,aes_generic)
  $(call AddDepends/crypto)
endef

$(eval $(call KernelPackage,crypto-aes-internal))

define KernelPackage/crypto-qce
  TITLE:=QTI Crypto Engine (QCE)
  KCONFIG:= \
	CONFIG_CRYPTO_DEV_QCE=y \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_ALL=y \
	CONFIG_CRYPTO_DEV_QCE_SKCIPHER=y \
	CONFIG_CRYPTO_DEV_QCE_AEAD=y \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_SKCIPHER=n \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_SHA=n \
	CONFIG_CRYPTO_DEV_QCE_SW_MAX_LEN=0 \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_AEAD=n \
	CONFIG_CRYPTO_DEV_QCE_SHA=y
  DEPENDS:=@TARGET_ipq95xx +kmod-crypto-manager +kmod-crypto-hash +kmod-crypto-authenc
endef

$(eval $(call KernelPackage,crypto-qce))

define KernelPackage/crypto-seccrypt
  TITLE:=QTI Crypto Secure Crypt (QSC)
  KCONFIG:= \
	CONFIG_SECCRYPT
  FILES:= \
	$(LINUX_DIR)/drivers/crypto/seccrypt.ko
  AUTOLOAD:=$(call AutoLoad,09,seccrypt)
  DEPENDS:=@TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx
  $(call AddPlatformDepends/crypto)
endef

$(eval $(call KernelPackage,crypto-seccrypt))

USB_MENU:=USB Support

define AddPlatformDepends/usb
  SUBMENU:=$(USB_MENU)
  DEPENDS+=+kmod-usb-core $(1)
endef

define KernelPackage/usb-configfs
 TITLE:= USB functions
  KCONFIG:=CONFIG_USB_CONFIGFS \
	CONFIG_USB_CONFIGFS_SERIAL=n \
	CONFIG_USB_CONFIGFS_ACM=n \
	CONFIG_USB_CONFIGFS_OBEX=n \
	CONFIG_USB_CONFIGFS_NCM=n \
	CONFIG_USB_CONFIGFS_ECM=n \
	CONFIG_USB_CONFIGFS_ECM_SUBSET=n \
	CONFIG_USB_CONFIGFS_RNDIS=n \
	CONFIG_USB_CONFIGFS_EEM=n \
	CONFIG_USB_CONFIGFS_MASS_STORAGE=n \
	CONFIG_USB_CONFIGFS_F_LB_SS=n \
	CONFIG_USB_CONFIGFS_F_FS=n \
	CONFIG_USB_CONFIGFS_F_UAC1=n \
	CONFIG_USB_CONFIGFS_F_UAC2=n \
	CONFIG_USB_CONFIGFS_F_MIDI=n \
	CONFIG_USB_CONFIGFS_F_HID=n \
	CONFIG_USB_CONFIGFS_F_PRINTER=n \
	CONFIG_USB_CONFIGFS_F_QDSS=n
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-configfs/description
 USB functions
endef

$(eval $(call KernelPackage,usb-configfs))

define KernelPackage/usb-uas
  TITLE:=USB Attched SCSI support
  DEPENDS:= +kmod-scsi-core +kmod-usb-storage
  KCONFIG:=CONFIG_USB_UAS
  FILES:=$(LINUX_DIR)/drivers/usb/storage/uas.ko
  AUTOLOAD:=$(call AutoProbe,uas)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-uas/description
 Kernel support for USB Attached SCSI devices (UAS)
endef

$(eval $(call KernelPackage,usb-uas))

define KernelPackage/usb-phy-ipq807x
  TITLE:=DWC3 USB QTI PHY drivers for IPQ807x based targets
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq54xx||TARGET_ipq52xx
  KCONFIG:= \
	CONFIG_PHY_QCOM_QUSB2 \
	CONFIG_PHY_QCOM_QMP=y \
	CONFIG_PHY_QCOM_QMP_USB
  FILES:= \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-qusb2.ko \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-qmp-usb.ko
  AUTOLOAD:=$(call AutoLoad,85,phy-qcom-qusb2 phy-qcom-qmp-usb)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-phy-ipq807x/description
  Support for USB PHY drivers in IPQ807x based SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq807x))

define KernelPackage/usb-phy-ipq96xx
  TITLE:=DWC3 USB QTI PHY drivers for IPQ96xx based targets
  DEPENDS:=@TARGET_ipq96xx
  KCONFIG:= \
	CONFIG_PHY_QCOM_USB_FEMTO_HS \
	CONFIG_PHY_QCOM_QMP=y \
	CONFIG_PHY_QCOM_QMP_USB
  FILES:= \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-usb-femto-hs.ko \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-qmp-usb.ko
  AUTOLOAD:=$(call AutoLoad,85,phy-qcom-usb-femto-hs phy-qcom-qmp-usb)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-phy-ipq96xx/description
  Support for USB PHY drivers in IPQ96xx based SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq96xx))

define KernelPackage/usb-dwc3-internal
  TITLE:=DWC3 USB controller driver
  DEPENDS:=+USB_GADGET_SUPPORT:kmod-usb-gadget +USB_GADGET_SUPPORT:kmod-usb-roles
  KCONFIG:= \
	CONFIG_USB_DWC3 \
	CONFIG_USB_DWC3_HOST=n \
	CONFIG_USB_DWC3_GADGET=n \
	CONFIG_USB_DWC3_DUAL_ROLE=y \
	CONFIG_EXTCON=y \
	CONFIG_USB_DWC3_DEBUG=n \
	CONFIG_USB_DWC3_VERBOSE=n
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3.ko
  AUTOLOAD:=$(call AutoLoad,84,dwc3)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-dwc3-internal/description
 This driver provides support for the Dual Role SuperSpeed
 USB Controller based on the Synopsys DesignWare USB3 IP Core
endef

$(eval $(call KernelPackage,usb-dwc3-internal))

define KernelPackage/usb-dwc3-qcom-internal
  TITLE:=DWC3 QTI USB driver
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq52xx||TARGET_ipq96xx +kmod-usb-dwc3-internal
  KCONFIG:= CONFIG_USB_DWC3_QCOM
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3-qcom.ko
  AUTOLOAD:=$(call AutoLoad,83,dwc3-qcom)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-dwc3-qcom-internal/description
 Some QTI SoCs use DesignWare Core IP for USB2/3 functionality.
 This driver also handles Qscratch wrapper which is needed for
 peripheral mode support.
endef

$(eval $(call KernelPackage,usb-dwc3-qcom-internal))

define KernelPackage/usb-f-qdss
  TITLE:=USB QDSS
  KCONFIG:=CONFIG_USB_F_QDSS \
	CONFIG_USB_CONFIGFS_F_QDSS=y
  DEPENDS:=@TARGET_ipq54xx +kmod-usb-lib-composite +kmod-usb-configfs +kmod-lib-crc-ccitt +kmod-usb-dwc3-qcom-internal
  FILES:=$(LINUX_DIR)/drivers/usb/gadget/function/usb_f_qdss.ko
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-f-qdss/description
 USB QDSS
endef

$(eval $(call KernelPackage,usb-f-qdss))

LEDS_MENU:=LED modules

define KernelPackage/leds-tlc591xx
  SUBMENU:=$(LEDS_MENU)
  TITLE:=TLC591XX LED support
  DEPENDS:=+kmod-i2c-core +kmod-regmap
  KCONFIG:=CONFIG_LEDS_TLC591XX
  FILES:=$(LINUX_DIR)/drivers/leds/leds-tlc591xx.ko
  AUTOLOAD:=$(call AutoLoad,60,leds-tlc591xx)
endef

define KernelPackage/leds-tlc591xx/description
  Driver for the TLC591XX I2C LED controllers.
endef

$(eval $(call KernelPackage,leds-tlc591xx))

define KernelPackage/mhitest_mod
  SUBMENU:=$(OTHER_MENU)
  TITLE:=MHI Test Module for qcn9000.
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq52xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx
  KCONFIG:=CONFIG_MHI_BUS_TEST
  FILES:=$(LINUX_DIR)/drivers/bus/mhi/test/mhitest_mod.ko
endef

define KernelPackage/mhitest_mod/description
  Add MHI Test Module.
endef

$(eval $(call KernelPackage,mhitest_mod))

define KernelPackage/mhi_fuse_blower_mod
  SUBMENU:=$(OTHER_MENU)
  TITLE:=MHI Fuse Blower Module for qcn9224.
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq52xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx
  KCONFIG:=
  FILES:=$(LINUX_DIR)/drivers/bus/mhi/fuse_blower/mhi_fuse_blower_mod.ko
endef

define KernelPackage/mhi_fuse_blower_mod/description
  Add MHI Fuse blower Module.
endef

$(eval $(call KernelPackage,mhi_fuse_blower_mod))

define KernelPackage/qmi_sample_client
  SUBMENU:=$(OTHER_MENU)
  TITLE:= Sample qmi test application.
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq52xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx

  KCONFIG:= \
	  CONFIG_SAMPLE_QMI_CLIENT
  FILES:= \
	  $(LINUX_DIR)/samples/qmi/qmi_sample_client.ko
  AUTOLOAD:=$(call AutoLoad,53,qmi_sample_client)
endef

define KernelPackage/qmi_sample_client/description
Add QMI ping_pong test application
endef

$(eval $(call KernelPackage,qmi_sample_client))

define KernelPackage/usb-phy-ipq5018
  TITLE:=DWC3 USB PHY driver for IPQ5018
  DEPENDS:=@TARGET_ipq53xx
  KCONFIG:= \
	CONFIG_PHY_QCOM_M31_USB \
	CONFIG_PHY_IPQ_UNIPHY_USB
  FILES:= \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-m31.ko \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qca-uniphy.ko
  AUTOLOAD:=$(call AutoLoad,85,phy-qcom-m31 phy-qca-uniphy)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-phy-ipq5018/description
 This driver provides support for the USB PHY drivers
 within the IPQ5018 SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq5018))

define KernelPackage/llcc_perfmon
  SUBMENU:=$(OTHER_MENU)
  TITLE:=LLCC Perfmon Driver
  DEPENDS+=@TARGET_ipq54xx||TARGET_ipq96xx
  KCONFIG:=CONFIG_QCOM_LLCC_PERFMON
  FILES:=$(LINUX_DIR)/drivers/soc/qcom/llcc_perfmon.ko
endef

define KernelPackage/llcc_perfmon/description
  LLCC Perfmonance driver support
endef

$(eval $(call KernelPackage,llcc_perfmon))

define KernelPackage/qcom_eud
  SUBMENU:=$(OTHER_MENU)
  TITLE:=QCOM EUD Driver
  DEPENDS+=@TARGET_ipq52xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq95xx||TARGET_ipq96xx +kmod-usb-gadget +kmod-usb-roles
  KCONFIG:=CONFIG_USB_QCOM_EUD
  FILES:=$(LINUX_DIR)/drivers/usb/misc/qcom_eud.ko
  AUTOLOAD:=$(call AutoLoad,80,qcom_eud)
endef

define KernelPackage/qcom_eud/description
  QCOM EUD driver support
endef

$(eval $(call KernelPackage,qcom_eud))

define KernelPackage/updtest_mod
  SUBMENU:=$(OTHER_MENU)
  TITLE:=User PD Test Module
  DEPENDS+=@TARGET_ipq53xx||TARGET_ipq54xx
  KCONFIG:=CONFIG_QCOM_Q6V5_UPD
  FILES:=$(LINUX_DIR)/drivers/remoteproc/qcom_q6v5_upd.ko
endef

define KernelPackage/updtest_mod/description
  User PD test module
endef

$(eval $(call KernelPackage,updtest_mod))

define Package/rproc-fw-mount
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Remote Processor Firmware Mount Script
  DEPENDS:=@TARGET_ipq96xx
endef

define Package/rproc-fw-mount/description
  Init script to mount CDSP and Prime firmware partitions for IPQ9650
endef

define Package/rproc-fw-mount/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/rproc_fw_mount $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,rproc-fw-mount))
