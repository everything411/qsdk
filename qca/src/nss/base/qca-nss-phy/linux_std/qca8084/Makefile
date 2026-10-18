###################################################
# Makefile for the QCA8084 linux PHY driver
###################################################
ccflags-y += -Werror -Wall
TARGET := qca8084-phy
obj-m += $(TARGET).o
$(TARGET)-objs := ../qcom_phy_lib.o qca8084.o
