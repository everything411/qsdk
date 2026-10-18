###################################################
# Makefile for the QCA81XX linux PHY driver
###################################################
ccflags-y += -Werror -Wall
TARGET := qca81xx-phy
obj-m += $(TARGET).o
$(TARGET)-objs := qca81xx.o
ifdef CONFIG_HWMON
$(TARGET)-objs += qca81xx_hwmon.o
endif
ifdef CONFIG_MACSEC
$(TARGET)-objs += qca81xx_macsec.o
endif
