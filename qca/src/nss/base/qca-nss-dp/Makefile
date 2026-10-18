###################################################
# Makefile for the NSS data plane driver
###################################################

obj ?= .

obj-m += qca-nss-dp.o

qca-nss-dp-objs += nss_dp_attach.o \
		   nss_dp_ethtools.o \
		   nss_dp_main.o \
		   hal/soc_ops/$(SoC)/nss_$(SoC).o

ifneq ($(CONFIG_NET_SWITCHDEV),)
qca-nss-dp-objs += nss_dp_switchdev.o
endif

ifeq ($(SoC),$(filter $(SoC),ipq53xx))
ifneq ($(CONFIG_QCA_NSS_DP_EAWTP),)
qca-nss-dp-objs += nss_dp_eawtp.o
endif
endif

ifeq ($(dp-ddrq),y)
qca-nss-dp-objs += nss_dp_ddrq.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_ddrq.o
ccflags-y += -DNSS_DP_DDRQ_SUPPORT
endif

NSS_DP_INCLUDE = -I$(obj)/include -I$(obj)/exports -I$(obj)/hal/include \
		 -I$(obj)/hal/dp_ops/include \
		 -I$(obj)/hal/gmac_ops/syn/xgmac

ifeq ($(SoC),$(filter $(SoC),ipq807x ipq60xx))
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v1/edma_cfg.o \
		   hal/dp_ops/edma_dp/edma_v1/edma_data_plane.o \
		   hal/dp_ops/edma_dp/edma_v1/edma_tx_rx.o \
		   hal/gmac_ops/qcom/qcom_if.o \
		   hal/gmac_ops/syn/xgmac/syn_if.o
NSS_DP_INCLUDE += -I$(obj)/hal/dp_ops/edma_dp/edma_v1/include
ccflags-y += -DNSS_DP_PPE_SUPPORT -DNSS_DP_MAC_POLL_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq807x))
ccflags-y += -DNSS_DP_IPQ807X -DNSS_DP_EDMA_TX_SMALL_PKT_WAR
endif

ifeq ($(SoC),$(filter $(SoC),ipq60xx))
ccflags-y += -DNSS_DP_IPQ60XX
endif

ifeq ($(SoC),$(filter $(SoC),ipq53xx ipq54xx))
ccflags-y += -DNSS_DP_MHT_SW_PORT_MAP
ccflags-y += -DNSS_DP_CONFIG_RST
endif

# Adding flag to enable physical port mirror support
# in PPE - To be enabled when mirror support is needed.
#ifeq ($(SoC),$(filter $(SoC),ipq95xx ipq53xx))
#ccflags-y += -DNSS_DP_PORT_MIRROR_EN
#endif

ifeq ($(SoC),$(filter $(SoC),ipq50xx))
qca-nss-dp-objs += hal/dp_ops/syn_gmac_dp/syn_dp_cfg_rx.o \
		   hal/dp_ops/syn_gmac_dp/syn_dp_cfg_tx.o \
		   hal/dp_ops/syn_gmac_dp/syn_dp_rx.o \
		   hal/dp_ops/syn_gmac_dp/syn_dp_tx.o \
		   hal/dp_ops/syn_gmac_dp/syn_dp.o \
		   hal/gmac_ops/syn/gmac/syn_if.o
NSS_DP_INCLUDE += -I$(obj)/hal/dp_ops/syn_gmac_dp/include
ccflags-y += -DNSS_DP_IPQ50XX -DNSS_DP_ENABLE_NAPI_GRO
endif

ifeq ($(SoC),$(filter $(SoC),ipq95xx ipq53xx ipq54xx))
ccflags-y += -DNSS_DP_MAX_TXCOMP_TIMEOUT
qca-nss-dp-objs += nss_dp_vp_main.o \
		   nss_dp_ethtool_priv.o \
		   hal/dp_ops/edma_dp/edma_v2/edma.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_cfg_rx.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_cfg_tx.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_debugfs.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_dp.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_dp_vp.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_misc.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_procfs.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_rx.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_tx.o \
		   hal/gmac_ops/qcom/qcom_if.o \
		   hal/gmac_ops/syn/xgmac/syn_if.o
ifneq ($(CONFIG_PTP_1588_CLOCK),)
qca-nss-dp-objs += hal/gmac_ops/syn/xgmac/syn_ptp.o
endif
ccflags-y += -DNSS_DP_EDMA_I2C_BUS_ENABLE
ccflags-y += -DNSS_DP_TX_SMALL_PACKET_WAR
ifeq ($(dp-ppe-ds),y)
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v2/edma_ppeds.o
ccflags-y += -DNSS_DP_PPEDS_SUPPORT
endif

ifeq ($(dp-loopback),y)
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v2/edma_cfg_rx_loopback.o \
		   hal/dp_ops/edma_dp/edma_v2/edma_cfg_tx_loopback.o
ccflags-y += -DNSS_DP_EDMA_LOOPBACK_SUPPORT
endif
NSS_DP_INCLUDE += -I$(obj)/hal/dp_ops/edma_dp/edma_v2
NSS_DP_INCLUDE += -I$(obj)/hal/dp_ops/edma_dp/edma_v2/include
ccflags-y += -DNSS_DP_ENABLE_NAPI_GRO -DNSS_DP_VP_SUPPORT -DNSS_DP_EDMA_V2 -DNSS_DP_MAC_POLL_SUPPORT -DNSS_DP_SW_BR_OPS -DNSS_DP_ETHTOOL_MRR_OPS
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx ipq96xx))
ccflags-y += -DNSS_DP_MAX_TXCOMP_TIMEOUT
qca-nss-dp-objs += nss_dp_vp_main.o \
		   nss_dp_ethtool_priv.o \
		   hal/dp_ops/edma_dp/edma_v3/edma.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_cfg_rx.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_cfg_tx.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_debugfs.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_dp.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_dp_vp.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_misc.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_procfs.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_rx.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_tx.o \
		   hal/gmac_ops/qcom/qcom_if.o \
		   hal/gmac_ops/syn/xgmac/syn_if.o
ifneq ($(CONFIG_PTP_1588_CLOCK),)
qca-nss-dp-objs += hal/gmac_ops/syn/xgmac/syn_ptp.o
endif
ccflags-y += -DNSS_DP_EDMA_I2C_BUS_ENABLE
ccflags-y += -DNSS_DP_TX_SMALL_PACKET_WAR
ifeq ($(dp-ppe-ds),y)
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v3/edma_ppeds_wifi8.o
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v3/edma_ppeds_wifi7.o
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v3/edma_ppeds_common.o
ccflags-y += -DNSS_DP_PPEDS_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx ipq52xx))
ccflags-y += -DNSS_DP_HW_GRO
endif

ifeq ($(dp-loopback),y)
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v3/edma_cfg_rx_loopback.o \
		   hal/dp_ops/edma_dp/edma_v3/edma_cfg_tx_loopback.o
ccflags-y += -DNSS_DP_EDMA_LOOPBACK_SUPPORT
endif

ifeq ($(dp-udp-st),y)
qca-nss-dp-objs += hal/dp_ops/edma_dp/edma_v3/edma_cfg_tx_udp_st.o
ccflags-y += -DNSS_DP_UDP_ST_SUPPORT
endif
NSS_DP_INCLUDE += -I$(obj)/hal/dp_ops/edma_dp/edma_v3
NSS_DP_INCLUDE += -I$(obj)/hal/dp_ops/edma_dp/edma_v3/include
ccflags-y += -DNSS_DP_ENABLE_NAPI_GRO -DNSS_DP_VP_SUPPORT -DNSS_DP_EDMA_V3 -DNSS_DP_MAC_POLL_SUPPORT -DNSS_DP_SW_BR_OPS -DNSS_DP_ETHTOOL_MRR_OPS
endif

ifeq ($(SoC),$(filter $(SoC),ipq53xx))
ccflags-y += -DNSS_DP_IPQ53XX
endif

ifeq ($(SoC),$(filter $(SoC),ipq95xx))
ccflags-y += -DNSS_DP_IPQ95XX
endif

ifeq ($(SoC),$(filter $(SoC),ipq96xx))
ccflags-y += -DNSS_DP_IPQ96XX
ccflags-y += -DNSS_DP_HIGHER_RING_MASK_CONFIG
ccflags-y += -DNSS_DP_EDMA_SKIP_PL_OFFSET
ccflags-y += -DNSS_DP_RING_IDX_CONFIG
ccflags-y += -DNSS_DP_EDMA_RING_RESET
ccflags-y += -DNSS_DP_HTT_SW_PORT_MAP
endif

ifeq ($(SoC),$(filter $(SoC),ipq52xx))
ccflags-y += -DNSS_DP_IPQ52XX
ccflags-y += -DNSS_DP_PON_SUPPORT
ccflags-y += -DNSS_DP_HIGHER_RING_MASK_CONFIG
ccflags-y += -DNSS_DP_EDMA_SKIP_PL_OFFSET
ccflags-y += -DNSS_DP_RING_IDX_CONFIG
ccflags-y += -DNSS_DP_EDMA_RING_RESET
ccflags-y += -DNSS_DP_HTT_SW_PORT_MAP
endif

ifeq ($(SoC),$(filter $(SoC),ipq54xx))
ccflags-y += -DNSS_DP_IPQ54XX
ccflags-y += -DNSS_DP_EDMA_SKIP_PL_OFFSET
ccflags-y += -DNSS_DP_HIGHER_RING_MASK_CONFIG
ccflags-y += -DNSS_DP_MDIO_HIGHER_VP_PORTS_SUPP
ccflags-y += -DNSS_DP_HIGHER_FC_CONFIG
ccflags-y += -DNSS_DP_RING_IDX_CONFIG
ccflags-y += -DNSS_DP_EDMA_SKIP_FOUR_PPEDS_NODES
ccflags-y += -DNSS_DP_EDMA_MHT_SW_WITH_VP_RING
ccflags-y += -DNSS_DP_EDMA_RING_RESET
ccflags-y += -DNSS_DP_EDMA_LOOPBACK_BUF_CONFIG
endif

ifeq ($(dp-net-standby),y)
qca-nss-dp-objs += nss_dp_netstandby.o
ccflags-y += -DNSS_DP_NETSTANDBY
endif

ifeq ($(dp-sfu),y)
ccflags-y += -DNSS_DP_SFU_PROFILE
endif

ccflags-y += $(NSS_DP_INCLUDE)
ccflags-y += -Wall -Werror
ccflags-y += -DEDMA_DEBUG_LEVEL=2
