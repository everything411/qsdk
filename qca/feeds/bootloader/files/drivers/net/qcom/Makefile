# SPDX-License-Identifier: GPL-2.0+

ifeq ($(CONFIG_QCOM_NSS_SWITCH_V2),y)
obj-y += nss_switch_v2.o
else ifeq ($(CONFIG_QCOM_NSS_SWITCH),y)
obj-y += nss-switch.o
endif
obj-$(CONFIG_ETH_IPQ9574)	+= ipq9574_port_config.o
obj-$(CONFIG_ETH_IPQ5332) 	+= ipq5332_port_config.o
obj-$(CONFIG_ETH_IPQ5424) 	+= ipq5424_port_config.o
obj-$(CONFIG_ETH_IPQ5210) 	+= ipq5210_port_config.o
obj-$(CONFIG_ETH_IPQ9650) 	+= ipq9650_port_config.o
