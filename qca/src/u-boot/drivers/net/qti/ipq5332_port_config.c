// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "nss-switch.h"

uint32_t nb_vsi_config[CONFIG_ETH_MAX_MAC] = {0x03, 0x05};

/*
 * .id of the PHY TYPE
 * .clk_rate {10, 100, 1000, 10000, 2500, 5000}
 * .mac_mode {10, 100, 1000, 10000, 2500, 5000}
 * .modes {10, 100, 1000, 10000, 2500, 5000}
 * index are based on Mac speed
 */

static struct ipq_eth_port_config ipq5332_port_config[] = {
	{
		QCA8x8x_BYPASS_TYPE,
		{
			CLK_2_5_MHZ,
			CLK_25_MHZ,
			CLK_125_MHZ,
			-1,
			CLK_312_5_MHZ
		},
		{
			GMAC,
			GMAC,
			GMAC,
			-1,
			XGMAC
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,
			PORT_WRAPPER_SGMII0_RGMII4,
			PORT_WRAPPER_SGMII0_RGMII4,
			-1,
			PORT_WRAPPER_SGMII_PLUS
		},
	},{
		QCA8x8x_SWITCH_TYPE,
		{
			CLK_312_5_MHZ,
			CLK_312_5_MHZ,
			CLK_312_5_MHZ,
			-1,
			CLK_312_5_MHZ
		},
		{
			XGMAC,
			XGMAC,
			XGMAC,
			-1,
			XGMAC
		},
		{
			PORT_WRAPPER_SGMII_PLUS,
			PORT_WRAPPER_SGMII_PLUS,
			PORT_WRAPPER_SGMII_PLUS,
			-1,
			PORT_WRAPPER_SGMII_PLUS
		},
	},{

		QCA8081_PHY_TYPE,
		{
			CLK_2_5_MHZ,
			CLK_25_MHZ,
			CLK_125_MHZ,
			-1,
			CLK_312_5_MHZ
		},
		{
			GMAC,
			GMAC,
			GMAC,
			-1,
			XGMAC
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,
			PORT_WRAPPER_SGMII0_RGMII4,
			PORT_WRAPPER_SGMII0_RGMII4,
			-1,
			PORT_WRAPPER_SGMII_PLUS
		},
	},{
		SFP10G_PHY_TYPE,
		{
			CLK_312_5_MHZ,
			CLK_312_5_MHZ,
			CLK_312_5_MHZ,
			CLK_312_5_MHZ,
			CLK_312_5_MHZ,
			CLK_312_5_MHZ
		},
		{
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC
		},
		{
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R
		},
	},{
		AQ_PHY_TYPE,
		{
			CLK_2_5_MHZ,
			CLK_25_MHZ,
			CLK_125_MHZ,
			CLK_312_5_MHZ,
			CLK_78_125_MHZ,
			CLK_156_25_MHZ
		},
		{
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC
		},
		{
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII
		},
	},{
		QCA81xx_PHY_TYPE,
		{
			-1,
			CLK_12_5_MHZ,
			CLK_125_MHZ,
			CLK_312_5_MHZ,
			CLK_78_125_MHZ,
			CLK_156_25_MHZ,
		},
		{
			-1,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC
		},
		{
			-1,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII,
			PORT_WRAPPER_USXGMII
		},
	},{
		UNUSED_PHY_TYPE,
	},
};

struct ipq_eth_port_config *port_config = ipq5332_port_config;

static struct ipq_tdm_config ipq5322_tdm_config [] = {
	{
		{0x22, 0x30, 0x21, 0x31, 0x22, 0x32, 0x20, 0x30, 0x22, 0x31,
			0x21, 0x32, 0x20, 0x30, 0x20, 0x31, 0x22, 0x32, 0x21,
			0x30, 0x22, 0x31, 0x20, 0x32, 0x22, 0x30, 0x21, 0x31,
			0x20, 0x32},
	}
};

struct ipq_tdm_config *tdm_config = ipq5322_tdm_config;

struct ipq_eth_sku *ipq_uniphy = NULL;

struct edma_config ipq_edma_config = {
	.sw_version		= EDMA_SW_VER_1_ID,
	.txdesc_ring_start 	= 23,
	.txdesc_rings 		= 1,
	.txdesc_ring_end 	= 24,
	.txcmpl_ring_start 	= 23,
	.txcmpl_rings 		= 1,
	.txcmpl_ring_end 	= 24,
	.rxfill_ring_start 	= 7,
	.rxfill_rings 		= 1,
	.rxfill_ring_end 	= 8,
	.rxdesc_ring_start 	= 15,
	.rxdesc_rings 		= 1,
	.rxdesc_ring_end 	= 16,
	.tx_map			= 4,
	.rx_map			= 2,
	.max_txcmpl_rings	= 24,
	.max_txdesc_rings	= 24,
	.max_rxdesc_rings	= 16,
	.max_rxfill_rings	= 8,
	.iports			= 3,
	.ports			= 2,
	.start_ports		= 1,
	.vsi			= 7,
	.ipo_action		= 6,
	.tdm_ctrl_val		= 0x80000020,
};
