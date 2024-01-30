
ARCH:=arm
SUBTARGET:=ipq53xx_32
BOARDNAME:=QTI IPQ53xx(32bit) based boards
CPU_TYPE:=cortex-a7

DEFAULT_PACKAGES += \
	uboot-2016-ipq5332 -uboot-2016-ipq5332_tiny -lk-ipq53xx \
	fwupgrade-tools

define Target/Description
	Build firmware image for IPQ53xx SoC devices.
endef
