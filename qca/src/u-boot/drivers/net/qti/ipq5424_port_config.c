// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "nss-switch.h"

uint32_t nb_vsi_config[CONFIG_ETH_MAX_MAC] = {0x03, 0x05, 0x09};


/*
 * .id of the PHY TYPE
 * .clk_rate {10, 100, 1000, 10000, 2500, 5000}
 * .mac_mode {10, 100, 1000, 10000, 2500, 5000}
 * .modes {10, 100, 1000, 10000, 2500, 5000}
 * index are based on Mac speed
 */

static struct ipq_eth_port_config ipq5424_port_config[] = {
	{
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
			CLK_1_25_MHZ,
			CLK_12_5_MHZ,
			CLK_125_MHZ,
			CLK_312_5_MHZ,
			CLK_78_125_MHZ,
			CLK_156_25_MHZ,
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

struct ipq_eth_port_config *port_config = ipq5424_port_config;

static struct ipq_tdm_config ipq5424_tdm_config [] = {
	{
		{0x21, 0x30, 0x22, 0x31, 0x20, 0x32, 0x23, 0x30, 0x21,
		0x33, 0x22, 0x31, 0x20, 0x32, 0x23, 0x30, 0x21, 0x33,
		0x22, 0x31, 0x20, 0x32, 0x23, 0x30, 0x21, 0x33, 0x22,
		0x31, 0x20, 0x32, 0x23, 0x30, 0x21, 0x33, 0x20, 0x31,
		0x22, 0x30, 0x23, 0x32, 0x20, 0x33, 0x21, 0x30, 0x22,
		0x31, 0x20, 0x32, 0x23, 0x30, 0x21, 0x33, 0x22, 0x31,
		0x20, 0x32, 0x23, 0x30, 0x21, 0x33, 0x22, 0x31, 0x20,
		0x32, 0x23, 0x30, 0x21, 0x33, 0x22, 0x31, 0x20, 0x32,
		0x23, 0x30, 0x21, 0x33, 0x20, 0x31, 0x22, 0x30, 0x23,
		0x32, 0x20, 0x33, 0x21, 0x30, 0x22, 0x31, 0x20, 0x32,
		0x23, 0x30, 0x21, 0x33, 0x22, 0x31, 0x20, 0x32, 0x23,
		0x30, 0x21, 0x33, 0x22, 0x31, 0x20, 0x32, 0x23, 0x30,
		0x21, 0x33, 0x22, 0x31, 0x20, 0x32, 0x23, 0x30, 0x21,
		0x33, 0x20, 0x31, 0x22, 0x30, 0x23, 0x32, 0x20, 0x33
		},

	},
};

struct ipq_tdm_config *tdm_config = ipq5424_tdm_config;

static struct ipq_eth_sku ipq5424_uniphy[CONFIG_ETH_MAX_UNIPHY] = {
	{
		.reg	= 0xA6264,
		.bit	= 0,
	},
	{
		.reg	= 0xA626C,
		.bit	= 0,
	},
	{
		.reg	= 0xA6274,
		.bit	= 0,
	},
};

struct ipq_eth_sku *ipq_uniphy = ipq5424_uniphy;

struct edma_config ipq_edma_config = {
	.sw_version		= EDMA_SW_VER_2_ID,
	.txdesc_ring_start 	= 0,
	.txdesc_rings 		= 1,
	.txdesc_ring_end 	= 1,
	.txcmpl_ring_start 	= 0,
	.txcmpl_rings 		= 1,
	.txcmpl_ring_end 	= 1,
	.rxfill_ring_start 	= 0,
	.rxfill_rings 		= 1,
	.rxfill_ring_end 	= 1,
	.rxdesc_ring_start 	= 0,
	.rxdesc_rings 		= 1,
	.rxdesc_ring_end 	= 1,
	.tx_map			= 6,
	.rx_map			= 3,
	.max_txcmpl_rings	= 32,
	.max_txdesc_rings	= 32,
	.max_rxdesc_rings	= 24,
	.max_rxfill_rings	= 8,
	.iports			= 4,
	.ports			= 3,
	.start_ports		= 1,
	.vsi			= 0xf,
	.ipo_action		= 6,
	.tdm_ctrl_val		= 0x8000007E,
};
