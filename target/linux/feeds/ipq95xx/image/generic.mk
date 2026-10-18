
define Device/qcom_alxx
        $(call Device/MultiDTBFitImage)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-ALXX
	DEVICE_VARIANT :=
	BOARD_NAME := ap-alxx
	SOC := ipq9574
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,13668k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += qcom_alxx

define Device/qcom_al01-c1
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-AL01-C1
	DEVICE_VARIANT := C1
	BOARD_NAME := ap-al01.1-c1
	BUILD_DTS_ipq9574-al01-c1 := 1
	SOC := ipq9574
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_5_4), qcom_al01-c1)

define Device/qcom_al02-c1
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-AL02-C1
	DEVICE_VARIANT := C1
	BOARD_NAME := ap-al02-c1
	BUILD_DTS_ipq9574-al02-c1 := 1
	SOC := ipq9574
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_5_4), qcom_al02-c1)

define Device/qcom_al02-c2
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-AL02-C2
	DEVICE_VARIANT := C2
	BOARD_NAME := ap-al02-c2
	BUILD_DTS_ipq9574-al02-c2 := 1
	SOC := ipq9574
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_5_4), qcom_al02-c2)

define Device/qcom_al02-c7
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := AP-AL02-C7
	DEVICE_VARIANT := C7
	BOARD_NAME := ap-al02.1-c7
	BUILD_DTS_ipq9574-al02-c7 := 1
	SOC := ipq9574
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_5_4), qcom_al02-c7)

define Device/qcom_rdp433
	$(call Device/FitImageLzma)
	DEVICE_VENDOR := Qualcomm Technologies, Inc.
	DEVICE_MODEL := RDP433
	DEVICE_VARIANT := AP-AL02-C4
	BOARD_NAME := ap-al02.1-c4
	BUILD_DTS_ipq9574-rdp433 := 1
	SOC := ipq9574
	KERNEL_INSTALL := 1
	KERNEL_SIZE := $(if $(CONFIG_DEBUG),8680k,6500k)
	IMAGE_SIZE := 25344k
	IMAGE/sysupgrade.bin := append-kernel | pad-to $$$$(KERNEL_SIZE) | append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += $(if $(CONFIG_LINUX_6_1)$(CONFIG_LINUX_6_6), qcom_rdp433)
