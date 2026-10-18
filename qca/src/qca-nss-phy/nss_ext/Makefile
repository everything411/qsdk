###################################################
# Makefile for the QCA NSS PHY EXT driver
###################################################
ccflags-y += -Werror -Wall
TARGET := qca-nss-phy
obj-m += $(TARGET).o

$(TARGET)-objs +=	\
	nss_phy_common.o	\
	nss_phy_linux_wrapper.o	\
	nss_phy_linux.o	\
	nss_phy_c45_common.o

ifneq (, $(filter CONFIG_NSSPHY_QCA807X, $(supported_phys)))
    ccflags-y += -DCONFIG_NSSPHY_QCA807X
    $(TARGET)-objs += qca807x_phy.o
endif

ifneq (, $(filter CONFIG_NSSPHY_QCA808X, $(supported_phys)))
    ccflags-y += -DCONFIG_NSSPHY_QCA808X
    $(TARGET)-objs += qca808x_phy.o qca8084_pinctrl.o
    ifeq ($(nss-phy-ptp), y)
        $(TARGET)-objs += nss_phy_ptp.o nss_phy_ptp_api.o
        ccflags-y += -DNSS_PHY_PTP
    endif
endif

ifneq (, $(filter CONFIG_NSSPHY_QCA81XX, $(supported_phys)))
    ccflags-y += -DCONFIG_NSSPHY_QCA81XX
    $(TARGET)-objs += qca81xx_phy.o
    ifeq ($(nss-phy-ptp), y)
        $(TARGET)-objs += nss_phy_ptp.o nss_phy_ptp_api.o
        ccflags-y += -DNSS_PHY_PTP
    endif
endif

ifneq (, $(filter CONFIG_NSSPHY_QCA803X, $(supported_phys)))
    ccflags-y += -DCONFIG_NSSPHY_QCA803X
    $(TARGET)-objs += qca803x_phy.o
endif

ifneq (, $(filter CONFIG_NSSPHY_QCA833X, $(supported_phys)))
    ccflags-y += -DCONFIG_NSSPHY_QCA833X
    $(TARGET)-objs += qca833x_phy.o
endif
