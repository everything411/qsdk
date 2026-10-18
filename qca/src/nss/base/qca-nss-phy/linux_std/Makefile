obj-y := qca8084/ ptp/ clock/ mdio_ahb/
obj-$(CONFIG_MACSEC) += macsec/
#
# PHY objects
#
ccflags-y += -Werror -Wall -Wmissing-prototypes
obj-m += qcom-lnx-phy.o

qcom-lnx-phy-objs := qca81xx/qca81xx.o qce1204/qce1204.o

ifdef CONFIG_HWMON
qcom-lnx-phy-objs += qca81xx/qca81xx_hwmon.o qce1204/qce1204_hwmon.o
endif
