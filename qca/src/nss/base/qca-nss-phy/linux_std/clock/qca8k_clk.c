/*
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <asm/div64.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/mdio.h>
#include <linux/phy.h>

#include "qca8k_clk.h"

#define MHT_PORT_CLK_CBC_MAX			8
/* 2 uniphy with rx and tx */
#define MHT_UNIPHY_INSTANCE			2

#define IPQ_HIGH_ADDR_PREFIX			0x18
#define IPQ_LOW_ADDR_PREFIX			0x10

#define WORK_MODE_PHY0_SEL_BOFFSET		0
#define WORK_MODE_PHY1_SEL_BOFFSET		1
#define WORK_MODE_PHY2_SEL_BOFFSET		2
#define WORK_MODE_PHY3_SEL0_BOFFSET		3
#define WORK_MODE_PHY3_SEL1_BOFFSET		4
#define WORK_MODE_PORT5_SEL_BOFFSET		5

#define MHT_CLK_SWITCH_MODE			BIT(WORK_MODE_PHY3_SEL1_BOFFSET)
#define MHT_CLK_SWITCH_BYPASS_PORT5_MODE	BIT(WORK_MODE_PORT5_SEL_BOFFSET)
#define MHT_CLK_PHY_UQXGMII_MODE		(BIT(WORK_MODE_PORT5_SEL_BOFFSET) | \
						BIT(WORK_MODE_PHY3_SEL0_BOFFSET) | \
						BIT(WORK_MODE_PHY2_SEL_BOFFSET) | \
						BIT(WORK_MODE_PHY1_SEL_BOFFSET) | \
						BIT(WORK_MODE_PHY0_SEL_BOFFSET))
#define MHT_CLK_PHY_SGMII_UQXGMII_MODE		(BIT(WORK_MODE_PORT5_SEL_BOFFSET) | \
						BIT(WORK_MODE_PHY2_SEL_BOFFSET) | \
						BIT(WORK_MODE_PHY1_SEL_BOFFSET) | \
						BIT(WORK_MODE_PHY0_SEL_BOFFSET))

struct qca8k_cc_priv {
	struct dentry *debugfs_dir;
};

static unsigned long mht_uniphy_raw_clock[MHT_UNIPHY_INSTANCE * 2] = {0};

static const unsigned long mht_switch_core_support_rates[] = {
	UQXGMII_SPEED_2500M_CLK,
};

static const unsigned long mht_cpuport_clk_support_rates[] = {
	UQXGMII_SPEED_10M_CLK,
	UQXGMII_SPEED_100M_CLK,
	UQXGMII_SPEED_1000M_CLK,
	UQXGMII_SPEED_2500M_CLK,
};

static const unsigned long mht_phyport_clk_support_rates[] = {
	UQXGMII_SPEED_10M_CLK,
	UQXGMII_SPEED_100M_CLK,
	UQXGMII_SPEED_1000M_CLK,
	UQXGMII_SPEED_2500M_CLK,
	UQXGMII_XPCS_SPEED_2500M_CLK,
};

static const unsigned long mht_ahb_clk_support_rates[] = {
	MHT_XO_CLK_RATE_50M,
	MHT_AHB_CLK_RATE_104P17M,
};

static const unsigned long mht_sys_clk_support_rates[] = {
	MHT_SYS_CLK_RATE_25M,
};

static const struct mht_parent_data mht_switch_core_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX312P5M, 1 },
};

static const struct mht_parent_data mht_mac0_tx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 } ,
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY1_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX, 2 },
};

static const struct mht_parent_data mht_mac0_rx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 } ,
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY1_RX, 1 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY1_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX, 2 },
};

/* port 1, 2, 3 rx/tx clock have the same parents */
static const struct mht_parent_data mht_mac1_tx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 } ,
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX312P5M, 6 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_RX312P5M, 7 },
};

static const struct mht_parent_data mht_mac1_rx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX312P5M, 6 },
};

static const struct mht_parent_data mht_mac4_tx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX312P5M, 3 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_RX312P5M, 7 },
};

static const struct mht_parent_data mht_mac4_rx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX312P5M, 3 },
};

static const struct mht_parent_data mht_mac5_tx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY1_TX, 7 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX, 7 },
};

static const struct mht_parent_data mht_mac5_rx_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_1000M_CLK, MHT_P_UNIPHY1_TX, 7 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX, 7 },
};

static const struct mht_parent_data mht_ahb_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
	{ UQXGMII_SPEED_2500M_CLK, MHT_P_UNIPHY1_TX312P5M, 2 },
};

static const struct mht_parent_data mht_sys_clk_pdata[] = {
	{ MHT_XO_CLK_RATE_50M, MHT_P_XO, 0 },
};

static struct clk_lookup mht_clk_lookup_table[] = {
	/* switch core clock */
	CLK_LOOKUP(4, 0, 8, CBCR_CLK_RESET,
			MHT_SWITCH_CORE_CLK,
			mht_switch_core_support_rates, ARRAY_SIZE(mht_switch_core_support_rates),
			mht_switch_core_pdata, ARRAY_SIZE(mht_switch_core_pdata)),
	CLK_LOOKUP(4, 0, 0x10, CBCR_CLK_RESET,
			MHT_APB_BRIDGE_CLK,
			mht_switch_core_support_rates, ARRAY_SIZE(mht_switch_core_support_rates),
			mht_switch_core_pdata, ARRAY_SIZE(mht_switch_core_pdata)),
	/* port 0 tx clock */
	CLK_LOOKUP(0x18, 0x1c, 0x20, CBCR_CLK_RESET,
			MHT_MAC0_TX_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac0_tx_clk_pdata, ARRAY_SIZE(mht_mac0_tx_clk_pdata)),
	CLK_LOOKUP(0x18, 0x1c, 0x24, CBCR_CLK_RESET,
			MHT_MAC0_TX_UNIPHY1_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac0_tx_clk_pdata, ARRAY_SIZE(mht_mac0_tx_clk_pdata)),
	/* port 0 rx clock */
	CLK_LOOKUP(0x2c, 0x30, 0x34, CBCR_CLK_RESET,
			MHT_MAC0_RX_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac0_rx_clk_pdata, ARRAY_SIZE(mht_mac0_rx_clk_pdata)),
	CLK_LOOKUP(0x2c, 0x30, 0x3c, CBCR_CLK_RESET,
			MHT_MAC0_RX_UNIPHY1_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac0_rx_clk_pdata, ARRAY_SIZE(mht_mac0_rx_clk_pdata)),
	/* port 1 tx clock */
	CLK_LOOKUP(0x44, 0x48, 0x50, CBCR_CLK_RESET,
			MHT_MAC1_UNIPHY1_CH0_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x44, 0x48, 0x54, CBCR_CLK_RESET,
			MHT_MAC1_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x44, 0x48, 0x58, CBCR_CLK_RESET,
			MHT_MAC1_GEPHY0_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x44, 0x4c, 0x5c, CBCR_CLK_RESET,
			MHT_MAC1_UNIPHY1_CH0_XGMII_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	/* port 1 rx clock */
	CLK_LOOKUP(0x64, 0x68, 0x70, CBCR_CLK_RESET,
			MHT_MAC1_UNIPHY1_CH0_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0x64, 0x68, 0x74, CBCR_CLK_RESET,
			MHT_MAC1_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0x64, 0x68, 0x78, CBCR_CLK_RESET,
			MHT_MAC1_GEPHY0_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0x64, 0x6c, 0x7c, CBCR_CLK_RESET,
			MHT_MAC1_UNIPHY1_CH0_XGMII_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	/* port 2 tx clock */
	CLK_LOOKUP(0x84, 0x88, 0x90, CBCR_CLK_RESET,
			MHT_MAC2_UNIPHY1_CH1_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x84, 0x88, 0x94, CBCR_CLK_RESET,
			MHT_MAC2_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x84, 0x88, 0x98, CBCR_CLK_RESET,
			MHT_MAC2_GEPHY1_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x84, 0x8c, 0x9c, CBCR_CLK_RESET,
			MHT_MAC2_UNIPHY1_CH1_XGMII_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	/* port 2 rx clock */
	CLK_LOOKUP(0xa4, 0xa8, 0xb0, CBCR_CLK_RESET,
			MHT_MAC2_UNIPHY1_CH1_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xa4, 0xa8, 0xb4, CBCR_CLK_RESET,
			MHT_MAC2_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xa4, 0xa8, 0xb8, CBCR_CLK_RESET,
			MHT_MAC2_GEPHY1_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xa4, 0xac, 0xbc, CBCR_CLK_RESET,
			MHT_MAC2_UNIPHY1_CH1_XGMII_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	/* port 3 tx clock */
	CLK_LOOKUP(0xc4, 0xc8, 0xd0, CBCR_CLK_RESET,
			MHT_MAC3_UNIPHY1_CH2_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0xc4, 0xc8, 0xd4, CBCR_CLK_RESET,
			MHT_MAC3_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0xc4, 0xc8, 0xd8, CBCR_CLK_RESET,
			MHT_MAC3_GEPHY2_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0xc4, 0xcc, 0xdc, CBCR_CLK_RESET,
			MHT_MAC3_UNIPHY1_CH2_XGMII_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_tx_clk_pdata, ARRAY_SIZE(mht_mac1_tx_clk_pdata)),
	/* port 3 rx clock */
	CLK_LOOKUP(0xe4, 0xe8, 0xf0, CBCR_CLK_RESET,
			MHT_MAC3_UNIPHY1_CH2_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xe4, 0xe8, 0xf4, CBCR_CLK_RESET,
			MHT_MAC3_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xe4, 0xe8, 0xf8, CBCR_CLK_RESET,
			MHT_MAC3_GEPHY2_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xe4, 0xec, 0xfc, CBCR_CLK_RESET,
			MHT_MAC3_UNIPHY1_CH2_XGMII_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac1_rx_clk_pdata, ARRAY_SIZE(mht_mac1_rx_clk_pdata)),
	/* port 4 tx clock */
	CLK_LOOKUP(0x104, 0x108, 0x110, CBCR_CLK_RESET,
			MHT_MAC4_UNIPHY1_CH3_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_tx_clk_pdata, ARRAY_SIZE(mht_mac4_tx_clk_pdata)),
	CLK_LOOKUP(0x104, 0x108, 0x114, CBCR_CLK_RESET,
			MHT_MAC4_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_tx_clk_pdata, ARRAY_SIZE(mht_mac4_tx_clk_pdata)),
	CLK_LOOKUP(0x104, 0x108, 0x118, CBCR_CLK_RESET,
			MHT_MAC4_GEPHY3_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_tx_clk_pdata, ARRAY_SIZE(mht_mac4_tx_clk_pdata)),
	CLK_LOOKUP(0x104, 0x10c, 0x11c, CBCR_CLK_RESET,
			MHT_MAC4_UNIPHY1_CH3_XGMII_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_tx_clk_pdata, ARRAY_SIZE(mht_mac4_tx_clk_pdata)),
	/* port 4 rx clock */
	CLK_LOOKUP(0x124, 0x128, 0x130, CBCR_CLK_RESET,
			MHT_MAC4_UNIPHY1_CH3_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_rx_clk_pdata, ARRAY_SIZE(mht_mac4_rx_clk_pdata)),
	CLK_LOOKUP(0x124, 0x128, 0x134, CBCR_CLK_RESET,
			MHT_MAC4_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_rx_clk_pdata, ARRAY_SIZE(mht_mac4_rx_clk_pdata)),
	CLK_LOOKUP(0x124, 0x128, 0x138, CBCR_CLK_RESET,
			MHT_MAC4_GEPHY3_RX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_rx_clk_pdata, ARRAY_SIZE(mht_mac4_rx_clk_pdata)),
	CLK_LOOKUP(0x124, 0x12c, 0x13c, CBCR_CLK_RESET,
			MHT_MAC4_UNIPHY1_CH3_XGMII_TX_CLK,
			mht_phyport_clk_support_rates, ARRAY_SIZE(mht_phyport_clk_support_rates),
			mht_mac4_rx_clk_pdata, ARRAY_SIZE(mht_mac4_rx_clk_pdata)),
	/* port 5 tx clock */
	CLK_LOOKUP(0x144, 0x148, 0x14c, CBCR_CLK_RESET,
			MHT_MAC5_TX_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac5_tx_clk_pdata, ARRAY_SIZE(mht_mac5_tx_clk_pdata)),
	CLK_LOOKUP(0x144, 0x148, 0x150, CBCR_CLK_RESET,
			MHT_MAC5_TX_UNIPHY0_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac5_tx_clk_pdata, ARRAY_SIZE(mht_mac5_tx_clk_pdata)),
	/* port 5 rx clock */
	CLK_LOOKUP(0x158, 0x15c, 0x160, CBCR_CLK_RESET,
			MHT_MAC5_RX_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac5_rx_clk_pdata, ARRAY_SIZE(mht_mac5_rx_clk_pdata)),
	CLK_LOOKUP(0x158, 0x15c, 0x164, CBCR_CLK_RESET,
			MHT_MAC5_RX_UNIPHY0_CLK,
			mht_cpuport_clk_support_rates, ARRAY_SIZE(mht_cpuport_clk_support_rates),
			mht_mac5_rx_clk_pdata, ARRAY_SIZE(mht_mac5_rx_clk_pdata)),
	/* AHB bridge clock */
	CLK_LOOKUP(0x16c, 0, 0x170, CBCR_CLK_RESET,
			MHT_AHB_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x174, CBCR_CLK_RESET,
			MHT_SEC_CTRL_AHB_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x178, CBCR_CLK_RESET,
			MHT_TLMM_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x190, CBCR_CLK_RESET,
			MHT_TLMM_AHB_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x194, CBCR_CLK_RESET,
			MHT_CNOC_AHB_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x198, CBCR_CLK_RESET,
			MHT_MDIO_AHB_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x19c, CBCR_CLK_RESET,
			MHT_MDIO_MASTER_AHB_CLK,
			mht_ahb_clk_support_rates, ARRAY_SIZE(mht_ahb_clk_support_rates),
			mht_ahb_clk_pdata, ARRAY_SIZE(mht_ahb_clk_pdata)),
	/* SYS clock */
	CLK_LOOKUP(0x1a4, 0, 0x1a8, CBCR_CLK_RESET,
			MHT_SRDS0_SYS_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1ac, CBCR_CLK_RESET,
			MHT_SRDS1_SYS_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1b0, CBCR_CLK_RESET,
			MHT_GEPHY0_SYS_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1b4, CBCR_CLK_RESET,
			MHT_GEPHY1_SYS_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1b8, CBCR_CLK_RESET,
			MHT_GEPHY2_SYS_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1bc, CBCR_CLK_RESET,
			MHT_GEPHY3_SYS_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),

	/* SEC control clock */
	CLK_LOOKUP(0x1c4, 0, 0x1c8, CBCR_CLK_RESET,
			MHT_SEC_CTRL_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),
	CLK_LOOKUP(0x1c4, 0, 0x1d0, CBCR_CLK_RESET,
			MHT_SEC_CTRL_SENSE_CLK,
			mht_sys_clk_support_rates, ARRAY_SIZE(mht_sys_clk_support_rates),
			mht_sys_clk_pdata, ARRAY_SIZE(mht_sys_clk_pdata)),

	/* GEPHY reset */
	CLK_LOOKUP(0, 0, 0x304, BIT(0), MHT_GEPHY_P0_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(1), MHT_GEPHY_P1_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(2), MHT_GEPHY_P2_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(3), MHT_GEPHY_P3_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(4), MHT_GEPHY_DSP_HW_RST, NULL, 0, NULL, 0),

	/* Global reset */
	CLK_LOOKUP(0, 0, 0x308, BIT(0), MHT_GLOBAL_RST, NULL, 0, NULL, 0),

	/* XPCS reset */
	CLK_LOOKUP(0, 0, 0x30c, BIT(0), MHT_UNIPHY_XPCS_RST, NULL, 0, NULL, 0),
};

static inline struct clk_lookup *qca8k_clk_find(const char *clock_id)
{
	int i;
	struct clk_lookup *clk;

	for (i = 0; i < ARRAY_SIZE(mht_clk_lookup_table); i++) {
		clk = &mht_clk_lookup_table[i];
		if (!strncmp(clock_id, clk->clk_name, strlen(clock_id)))
			return clk;
	}

	return NULL;
}

static inline void split_addr(u32 regaddr, u16 *r1, u16 *r2, u16 *page, u16 *sw_addr)
{
	*r1 = regaddr & 0x1c;

	regaddr >>= 5;
	*r2 = regaddr & 0x7;

	regaddr >>= 3;
	*page = regaddr & 0xffff;

	regaddr >>= 16;
	*sw_addr = regaddr & 0xff;
}

static u32 qca8386_read(struct mii_bus *bus, unsigned int reg)
{
	u16 r1, r2, page, sw_addr;
	u16 lo, hi;

	split_addr(reg, &r1, &r2, &page, &sw_addr);

	/* There is no competition, so the lock is not needed.
	 * since this function is only called before mii_bus registered.
	 */
	__mdiobus_write(bus, IPQ_HIGH_ADDR_PREFIX | (sw_addr >> 5), sw_addr & 0x1f, page);

	lo = __mdiobus_read(bus, IPQ_LOW_ADDR_PREFIX | r2, r1);
	hi = __mdiobus_read(bus, IPQ_LOW_ADDR_PREFIX | r2, r1 | BIT(1));

	return hi << 16 | lo;
};

static int qca8386_write(struct mii_bus *bus, unsigned int reg, unsigned int val)
{
	u16 r1, r2, page, sw_addr;
	u16 lo, hi;

	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	split_addr(reg, &r1, &r2, &page, &sw_addr);

	/* There is no competition, so the lock is not needed.
	 * since this function is only called before mii_bus registered.
	 */
	__mdiobus_write(bus, IPQ_HIGH_ADDR_PREFIX | (sw_addr >> 5), sw_addr & 0x1f, page);

	__mdiobus_write(bus, IPQ_LOW_ADDR_PREFIX | r2, r1, lo);
	__mdiobus_write(bus, IPQ_LOW_ADDR_PREFIX | r2, r1 | BIT(1), hi);

	return 0;
};

static int qca_mht_mii_write(struct mdio_device *mdiodev, int reg, int val)
{
	struct mii_bus *bus = mdiodev->bus;
	int ret;

	mutex_lock(&bus->mdio_lock);
	ret = qca8386_write(bus, reg, val);
	mutex_unlock(&bus->mdio_lock);

	return ret;
}

static u32 qca_mht_mii_read(struct mdio_device *mdiodev, int reg)
{
	struct mii_bus *bus = mdiodev->bus;
	u32 val;

	mutex_lock(&bus->mdio_lock);
	val = qca8386_read(bus, reg);
	mutex_unlock(&bus->mdio_lock);

	return val;
}

static int qca_mht_mii_update(struct mdio_device *mdiodev, int reg, int clear, int set)
{
	struct mii_bus *bus = mdiodev->bus;
	u32 val;

	mutex_lock(&bus->mdio_lock);
	val = qca8386_read(bus, reg);
	val &= ~clear;
	val |= set;
	qca8386_write(bus, reg, val);
	mutex_unlock(&bus->mdio_lock);

	return 0;
}

static inline int qca8k_clk_update(struct mdio_device *mdiodev, int cmd_reg)
{
	u32 reg_val;
	int i;

	/* update RCG to the new programmed configuration */
	reg_val = qca_mht_mii_read(mdiodev, cmd_reg);
	reg_val |= RCGR_CMD_UPDATE;
	qca_mht_mii_write(mdiodev, cmd_reg, reg_val);

	for (i = 1000; i > 0; i--) {
		reg_val = qca_mht_mii_read(mdiodev, cmd_reg);
		if (!(reg_val & RCGR_CMD_UPDATE))
			return 0;

		udelay(1);
	}

	pr_err("CLK cmd reg 0x%x fails updating to new configurations\n", cmd_reg);
	return -EINVAL;
}

bool qca8k_clk_is_asserted(struct mdio_device *mdiodev, const char *clock_id)
{
	struct clk_lookup *clk;
	u32 reg_val = 0;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return false;
	}

	reg_val = qca_mht_mii_read(mdiodev, MHT_CLK_BASE_REG + clk->cbc);
	return !!(reg_val & clk->rst_bit);
}
EXPORT_SYMBOL(qca8k_clk_is_asserted);

int qca8k_clk_assert(struct mdio_device *mdiodev, const char *clock_id)
{
	struct clk_lookup *clk;
	u32 cbc_reg = 0;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return -ENXIO;
	}

	cbc_reg = MHT_CLK_BASE_REG + clk->cbc;

	qca_mht_mii_update(mdiodev, cbc_reg, clk->rst_bit, clk->rst_bit);
	return 0;
}
EXPORT_SYMBOL(qca8k_clk_assert);

int qca8k_clk_deassert(struct mdio_device *mdiodev, const char *clock_id)
{
	struct clk_lookup *clk;
	u32 cbc_reg = 0;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return -ENXIO;
	}

	cbc_reg = MHT_CLK_BASE_REG + clk->cbc;

	qca_mht_mii_update(mdiodev, cbc_reg, clk->rst_bit, 0);
	return 0;
}
EXPORT_SYMBOL(qca8k_clk_deassert);

int qca8k_clk_reset(struct mdio_device *mdiodev, const char *clock_id)
{
	int rv = 0;

	rv = qca8k_clk_assert(mdiodev, clock_id);
	if (rv)
		return rv;

	/* Time required by HW to complete assert */
	udelay(10);

	rv = qca8k_clk_deassert(mdiodev, clock_id);

	return rv;
}
EXPORT_SYMBOL(qca8k_clk_reset);

bool qca8k_clk_is_enabled(struct mdio_device *mdiodev, const char *clock_id)
{
	struct clk_lookup *clk;
	u32 reg_val = 0;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return false;
	}

	reg_val = qca_mht_mii_read(mdiodev, MHT_CLK_BASE_REG + clk->rcg - 4);
	return (reg_val & RCGR_CMD_ROOT_OFF) == 0;
}
EXPORT_SYMBOL(qca8k_clk_is_enabled);

int qca8k_clk_enable(struct mdio_device *mdiodev, const char *clock_id)
{
	struct clk_lookup *clk;
	u32 cbc_reg = 0;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return -ENXIO;
	}

	cbc_reg = MHT_CLK_BASE_REG + clk->cbc;
	qca_mht_mii_update(mdiodev, cbc_reg, CBCR_CLK_ENABLE, CBCR_CLK_ENABLE);
	udelay(1);

	return 0;
}
EXPORT_SYMBOL(qca8k_clk_enable);

int qca8k_clk_disable(struct mdio_device *mdiodev, const char *clock_id)
{
	struct clk_lookup *clk;
	u32 cbc_reg = 0;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return -ENXIO;
	}

	cbc_reg = MHT_CLK_BASE_REG + clk->cbc;

	qca_mht_mii_update(mdiodev, cbc_reg, CBCR_CLK_ENABLE, 0);
	return 0;
}
EXPORT_SYMBOL(qca8k_clk_disable);

int qca8k_clk_parent_set(struct mdio_device *mdiodev,
		const char *clock_id, mht_clk_parent_t parent)
{
	u32 rcg_reg = 0, cmd_reg = 0, cfg = 0, cur_cfg = 0;
	const struct mht_parent_data *pdata = NULL;
	struct clk_lookup *clk;
	u32 i, reg_val;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return -ENXIO;
	}

	for (i = 0; i < clk->num_parent; i++) {
		pdata = &(clk->pdata[i]);
		if (pdata->parent == parent)
			break;
	}

	if (i == clk->num_parent) {
		pr_err("CLK %s is configured as incorrect parent %d\n", clock_id, parent);
		return -EINVAL;
	}

	rcg_reg = MHT_CLK_BASE_REG + clk->rcg;
	cmd_reg = MHT_CLK_BASE_REG + clk->rcg - 4;

	reg_val = qca_mht_mii_read(mdiodev, rcg_reg);
	cur_cfg = (reg_val & RCGR_SRC_SEL) >> RCGR_SRC_SEL_SHIFT;
	cfg = pdata->cfg;

	if (cfg == cur_cfg) {
		pr_debug("CLK %s parent %d is already configured correctly\n", clock_id, parent);
		return 0;
	}

	/* update clock parent */
	reg_val &= ~RCGR_SRC_SEL;
	reg_val |= cfg << RCGR_SRC_SEL_SHIFT;
	qca_mht_mii_write(mdiodev, rcg_reg, reg_val);

	/* update RCG to the new programmed configuration */
	return qca8k_clk_update(mdiodev, cmd_reg);
}
EXPORT_SYMBOL(qca8k_clk_parent_set);

void qca8k_uniphy_raw_clock_set(mht_clk_parent_t uniphy_clk, unsigned long rate)
{
	switch (uniphy_clk) {
		case MHT_P_UNIPHY0_RX:
		case MHT_P_UNIPHY0_TX:
		case MHT_P_UNIPHY1_RX:
		case MHT_P_UNIPHY1_TX:
			break;
		default:
			pr_err("Invalid uniphy_clk %d\n", uniphy_clk);
			return;
	}

	mht_uniphy_raw_clock[uniphy_clk - MHT_P_UNIPHY0_RX] = rate;
	return;
}
EXPORT_SYMBOL(qca8k_uniphy_raw_clock_set);

unsigned long qca8k_uniphy_raw_clock_get(mht_clk_parent_t uniphy_clk)
{
	switch (uniphy_clk) {
		case MHT_P_UNIPHY0_RX:
		case MHT_P_UNIPHY0_TX:
		case MHT_P_UNIPHY1_RX:
		case MHT_P_UNIPHY1_TX:
			break;
		default:
			pr_err("Invalid uniphy_clk %d\n", uniphy_clk);
			return MHT_XO_CLK_RATE_50M;
	}

	return mht_uniphy_raw_clock[uniphy_clk - MHT_P_UNIPHY0_RX];
}
EXPORT_SYMBOL(qca8k_uniphy_raw_clock_get);

int qca8k_clk_rate_set(struct mdio_device *mdiodev,
		const char *clock_id, unsigned long rate)
{
	u32 rcg_reg = 0, cmd_reg = 0, cdiv_reg = 0, cdiv_val = 0;
	const struct mht_parent_data *pdata = NULL;
	u32 i, reg_val, parent_index = 0;
	u64 div, prate = 0;
	struct clk_lookup *clk;

	clk = qca8k_clk_find(clock_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clock_id);
		return -ENXIO;
	}

	for (i = 0; i < clk->num_rate; i++)
		if (rate == clk->support_rate[i])
			break;

	if (i == clk->num_rate) {
		pr_err("CLK %s does not support to configure rate %ld\n", clock_id, rate);
		return -EINVAL;
	}

	rcg_reg = MHT_CLK_BASE_REG + clk->rcg;
	cmd_reg = MHT_CLK_BASE_REG + clk->rcg - 4;
	if (clk->cdiv != 0)
		cdiv_reg = MHT_CLK_BASE_REG + clk->cdiv;

	reg_val = qca_mht_mii_read(mdiodev, rcg_reg);

	/* get the parent rate of clock */
	parent_index = (reg_val & RCGR_SRC_SEL) >> RCGR_SRC_SEL_SHIFT;
	for (i = 0; i < clk->num_parent; i++) {
		pdata = &(clk->pdata[i]);
		if (pdata->cfg == parent_index) {
			/* uniphy0 rx, tx and unphy1 rx, tx clock can be 125M or 312.5M, which
			 * depends on the current link speed, the clock rate needs to be acquired
			 * dynamically.
			 */
			switch (pdata->parent) {
				case MHT_P_UNIPHY0_RX:
				case MHT_P_UNIPHY0_TX:
				case MHT_P_UNIPHY1_RX:
				case MHT_P_UNIPHY1_TX:
					prate = qca8k_uniphy_raw_clock_get(pdata->parent);
					break;
				default:
					/* XO 50M or 315P5M fix clock rate */
					prate = pdata->prate;
					break;
			}
			/* find the parent clock rate */
			break;
		}
	}

	if (i == clk->num_parent || prate == 0) {
		pr_err("CLK %s is configured as unsupported parent value %d\n",
				clock_id, parent_index);
		return -EINVAL;
	}

	/* when configuring XPSC clock to UQXGMII_XPCS_SPEED_2500M_CLK, the RCGR divider
	 * need to be bypassed, since there are two dividers from the same RCGR, one is
	 * for XPCS clock, the other is for EPHY port clock.
	 */
	if (rate == UQXGMII_XPCS_SPEED_2500M_CLK) {
		if (prate != UQXGMII_SPEED_2500M_CLK) {
			pr_err("CLK %s parent(%lld) needs to be updated to %d\n",
					clock_id, prate, UQXGMII_SPEED_2500M_CLK);
			return -EINVAL;
		}
		div = RCGR_DIV_BYPASS;
		cdiv_val = (UQXGMII_SPEED_2500M_CLK / UQXGMII_XPCS_SPEED_2500M_CLK) - 1;
	} else {

		/* calculate the RCGR divider prate/rate = (rcg_divider + 1)/2 */
		div = prate * 2;
		do_div(div, rate);
		div--;

		/* if the RCG divider can't meet the requirement, the CDIV reg can be simply
		 * divided by 10 to satisfy the required clock rate.
		 */
		if (div > RCGR_DIV_MAX) {
			/* update CDIV Reg to be divided by 10(N+1) */
			cdiv_val = CDIVR_DIVIDER_10;

			/* caculate the new RCG divider */
			do_div(prate, CDIVR_DIVIDER_10 + 1);
			div = prate * 2;
			do_div(div, rate);
			div--;
		}
	}

	/* update CDIV Reg to be divided by N(N-1 for reg value) */
	if (cdiv_reg != 0)
		qca_mht_mii_update(mdiodev, cdiv_reg,
				CDIVR_DIVIDER, cdiv_val << CDIVR_DIVIDER_SHIFT);

	if (cdiv_reg == 0 && cdiv_val > 0) {
		pr_err("CLK %s needs CDIVR to generate rate %ld from prate %lld\n",
				clock_id, rate, prate);
		return -EINVAL;
	}

	/* update RCGR */
	reg_val &= ~RCGR_HDIV;
	reg_val |= div << RCGR_HDIV_SHIFT;
	qca_mht_mii_write(mdiodev, rcg_reg, reg_val);

	/* update RCG to the new programmed configuration */
	return qca8k_clk_update(mdiodev, cmd_reg);
}
EXPORT_SYMBOL(qca8k_clk_rate_set);

int qca8k_clk_rate_get(struct mdio_device *mdiodev,
		const char *clock_id, struct mht_clk_data *clk_data)
{
	struct clk_lookup *clk;
	u64 div, prate = 0;
	int i, reg_val, parent_index = 0;
	const struct mht_parent_data *pdata = NULL;
	char clk_id[64] = {0};
	bool bypass_en = false;

	strlcpy(clk_id, clock_id, sizeof(clk_id));

	qca8k_port5_uniphy0_clk_src_get(mdiodev, &bypass_en);
	if (bypass_en == true) {
		if (strncasecmp(clock_id, MHT_MAC5_TX_UNIPHY0_CLK,
					strlen(MHT_MAC5_TX_UNIPHY0_CLK)) == 0)
			strlcpy(clk_id, MHT_MAC4_RX_CLK, sizeof(clk_id));
		else if (strncasecmp(clock_id, MHT_MAC5_RX_UNIPHY0_CLK,
					strlen(MHT_MAC5_RX_UNIPHY0_CLK)) == 0)
			strlcpy(clk_id, MHT_MAC4_TX_CLK, sizeof(clk_id));
	}

	clk = qca8k_clk_find(clk_id);
	if (!clk) {
		pr_err("CLK %s is not found!\n", clk_id);
		return -ENXIO;
	}

	reg_val = qca_mht_mii_read(mdiodev, MHT_CLK_BASE_REG + clk->rcg);

	/* get the parent rate of clock */
	parent_index = (reg_val & RCGR_SRC_SEL) >> RCGR_SRC_SEL_SHIFT;
	for (i = 0; i < clk->num_parent; i++) {
		pdata = &(clk->pdata[i]);
		if (pdata->cfg == parent_index) {
			/* uniphy0 rx, tx and unphy1 rx, tx clock can be 125M or 312.5M, which
			 * depends on the current link speed, the clock rate needs to be acquired
			 * dynamically.
			 */
			switch (pdata->parent) {
				case MHT_P_UNIPHY0_RX:
				case MHT_P_UNIPHY0_TX:
				case MHT_P_UNIPHY1_RX:
				case MHT_P_UNIPHY1_TX:
					prate = qca8k_uniphy_raw_clock_get(pdata->parent);
					break;
				default:
					/* XO 50M or 315P5M fix clock rate */
					prate = pdata->prate;
					break;
			}
			/* find the parent clock rate */
			break;
		}
	}

	if (i == clk->num_parent || prate == 0) {
		pr_err("CLK %s is configured as unsupported parent value %d\n",
				clk_id, parent_index);
		return -EINVAL;
	}

	/* calculate the current clock rate */
	div = (reg_val >> RCGR_HDIV_SHIFT) & RCGR_HDIV;
	if (div != 0) {
		/* RCG divider is bypassed if the div value is 0 */
		prate *= 2;
		do_div(prate, div + 1);
	}

	clk_data->rcg_val = reg_val;

	reg_val = qca_mht_mii_read(mdiodev, MHT_CLK_BASE_REG + clk->cbc);
	clk_data->cbc_val = reg_val;

	if (clk->cdiv != 0) {
		reg_val = qca_mht_mii_read(mdiodev, MHT_CLK_BASE_REG + clk->cdiv);
		clk_data->cdiv_val = reg_val;
		do_div(prate, ((reg_val >> CDIVR_DIVIDER_SHIFT) & CDIVR_DIVIDER) + 1);
	}

	clk_data->rate = prate;

	return 0;
}
EXPORT_SYMBOL(qca8k_clk_rate_get);

int qca8k_port5_uniphy0_clk_src_set(struct mdio_device *mdiodev, bool bypass_en)
{
	int mux_sel = 0;

	/* In switch mode, uniphy0 rx clock is from mac5 rx, uniphy0 tx clock is from mac5 tx;
	 * In bypass mode, uniphy0 rx clock is from mac4 tx, uniphy0 tx clock is from mac4 rx;
	 */

	if (bypass_en)
		mux_sel = MHT_UNIPHY0_SEL_MAC4;
	else
		mux_sel = MHT_UNIPHY0_SEL_MAC5;

	qca_mht_mii_update(mdiodev, MHT_CLK_BASE_REG + MHT_CLK_MUX_SEL,
			MHT_UNIPHY0_MUX_SEL_MASK, mux_sel);
	return 0;
}
EXPORT_SYMBOL(qca8k_port5_uniphy0_clk_src_set);

int qca8k_port5_uniphy0_clk_src_get(struct mdio_device *mdiodev, bool *bypass_en)
{
	u32 reg_val = 0;

	/* In switch mode, uniphy0 rx clock is from mac5 rx, uniphy0 tx clock is from mac5 tx;
	 * In bypass mode, uniphy0 rx clock is from mac4 tx, uniphy0 tx clock is from mac4 rx;
	 */
	reg_val = qca_mht_mii_read(mdiodev, MHT_CLK_BASE_REG + MHT_CLK_MUX_SEL);
	*bypass_en = (reg_val & MHT_UNIPHY0_SEL_MAC5) ? false : true;

	return 0;
}
EXPORT_SYMBOL(qca8k_port5_uniphy0_clk_src_get);

int qca8k_port_clk_rate_set(struct mdio_device *mdiodev,
		int mht_port_id, unsigned long rate)
{
	int rv = 0;
	char *mac_rx_clk = NULL, *mac_tx_clk = NULL;
	char *xgmii_tx_clk = NULL, *xgmii_rx_clk = NULL;

	switch (mht_port_id) {
		case 0:
			mac_rx_clk = MHT_MAC0_RX_CLK;
			mac_tx_clk = MHT_MAC0_TX_CLK;
			break;
		case 1:
			mac_rx_clk = MHT_MAC1_RX_CLK;
			mac_tx_clk = MHT_MAC1_TX_CLK;
			xgmii_rx_clk = MHT_MAC1_UNIPHY1_CH0_XGMII_RX_CLK;
			xgmii_tx_clk = MHT_MAC1_UNIPHY1_CH0_XGMII_TX_CLK;
			break;
		case 2:
			mac_rx_clk = MHT_MAC2_RX_CLK;
			mac_tx_clk = MHT_MAC2_TX_CLK;
			xgmii_rx_clk = MHT_MAC2_UNIPHY1_CH1_XGMII_RX_CLK;
			xgmii_tx_clk = MHT_MAC2_UNIPHY1_CH1_XGMII_TX_CLK;
			break;
		case 3:
			mac_rx_clk = MHT_MAC3_RX_CLK;
			mac_tx_clk = MHT_MAC3_TX_CLK;
			xgmii_rx_clk = MHT_MAC3_UNIPHY1_CH2_XGMII_RX_CLK;
			xgmii_tx_clk = MHT_MAC3_UNIPHY1_CH2_XGMII_TX_CLK;
			break;
		case 4:
			mac_rx_clk = MHT_MAC4_RX_CLK;
			mac_tx_clk = MHT_MAC4_TX_CLK;
			xgmii_rx_clk = MHT_MAC4_UNIPHY1_CH3_XGMII_RX_CLK;
			xgmii_tx_clk = MHT_MAC4_UNIPHY1_CH3_XGMII_TX_CLK;
			break;
		case 5:
			mac_rx_clk = MHT_MAC5_RX_CLK;
			mac_tx_clk = MHT_MAC5_TX_CLK;
			break;
		default:
			pr_err("Unsupported mht_port_id %d\n", mht_port_id);
			return -EINVAL;
	}

	rv = qca8k_clk_rate_set(mdiodev, mac_rx_clk, rate);
	if (rv)
		return rv;

	rv = qca8k_clk_rate_set(mdiodev, mac_tx_clk, rate);
	if (rv)
		return rv;

	if (xgmii_rx_clk != NULL && xgmii_tx_clk != NULL) {
		/* XGMII take the different clock rate from MAC clock when the link
		 * speed is 2.5G.
		 */
		if (rate == UQXGMII_SPEED_2500M_CLK)
			rate = UQXGMII_XPCS_SPEED_2500M_CLK;
		rv = qca8k_clk_rate_set(mdiodev, xgmii_rx_clk, rate);
		if (rv)
			return rv;

		rv = qca8k_clk_rate_set(mdiodev, xgmii_tx_clk, rate);
		if (rv)
			return rv;
	}

	return 0;
}
EXPORT_SYMBOL(qca8k_port_clk_rate_set);

static inline int qca8k_clk_ids_get(int mht_port_id,
		u8 mask, char **clk_ids)
{
	switch (mht_port_id) {
		case 0:
			if (mask & MHT_CLK_TYPE_MAC) {
				*clk_ids++ = MHT_MAC0_TX_CLK;
				*clk_ids++ = MHT_MAC0_RX_CLK;
			}

			if (mask & MHT_CLK_TYPE_UNIPHY) {
				*clk_ids++ = MHT_MAC0_TX_UNIPHY1_CLK;
				*clk_ids++ = MHT_MAC0_RX_UNIPHY1_CLK;
			}
			break;
		case 1:
			if (mask & MHT_CLK_TYPE_MAC) {
				*clk_ids++ = MHT_MAC1_TX_CLK;
				*clk_ids++ = MHT_MAC1_RX_CLK;
			}

			if (mask & MHT_CLK_TYPE_UNIPHY) {
				*clk_ids++ = MHT_MAC1_UNIPHY1_CH0_RX_CLK;
				*clk_ids++ = MHT_MAC1_UNIPHY1_CH0_TX_CLK;
				*clk_ids++ = MHT_MAC1_UNIPHY1_CH0_XGMII_RX_CLK;
				*clk_ids++ = MHT_MAC1_UNIPHY1_CH0_XGMII_TX_CLK;
			}

			if (mask & MHT_CLK_TYPE_EPHY) {
				*clk_ids++ = MHT_MAC1_GEPHY0_TX_CLK;
				*clk_ids++ = MHT_MAC1_GEPHY0_RX_CLK;
			}
			break;
		case 2:
			if (mask & MHT_CLK_TYPE_MAC) {
				*clk_ids++ = MHT_MAC2_TX_CLK;
				*clk_ids++ = MHT_MAC2_RX_CLK;
			}

			if (mask & MHT_CLK_TYPE_UNIPHY) {
				*clk_ids++ = MHT_MAC2_UNIPHY1_CH1_RX_CLK;
				*clk_ids++ = MHT_MAC2_UNIPHY1_CH1_TX_CLK;
				*clk_ids++ = MHT_MAC2_UNIPHY1_CH1_XGMII_RX_CLK;
				*clk_ids++ = MHT_MAC2_UNIPHY1_CH1_XGMII_TX_CLK;
			}

			if (mask & MHT_CLK_TYPE_EPHY) {
				*clk_ids++ = MHT_MAC2_GEPHY1_TX_CLK;
				*clk_ids++ = MHT_MAC2_GEPHY1_RX_CLK;
			}
			break;
		case 3:
			if (mask & MHT_CLK_TYPE_MAC) {
				*clk_ids++ = MHT_MAC3_TX_CLK;
				*clk_ids++ = MHT_MAC3_RX_CLK;
			}

			if (mask & MHT_CLK_TYPE_UNIPHY) {
				*clk_ids++ = MHT_MAC3_UNIPHY1_CH2_RX_CLK;
				*clk_ids++ = MHT_MAC3_UNIPHY1_CH2_TX_CLK;
				*clk_ids++ = MHT_MAC3_UNIPHY1_CH2_XGMII_RX_CLK;
				*clk_ids++ = MHT_MAC3_UNIPHY1_CH2_XGMII_TX_CLK;
			}

			if (mask & MHT_CLK_TYPE_EPHY) {
				*clk_ids++ = MHT_MAC3_GEPHY2_TX_CLK;
				*clk_ids++ = MHT_MAC3_GEPHY2_RX_CLK;
			}
			break;
		case 4:
			if (mask & MHT_CLK_TYPE_MAC) {
				*clk_ids++ = MHT_MAC4_TX_CLK;
				*clk_ids++ = MHT_MAC4_RX_CLK;
			}

			if (mask & MHT_CLK_TYPE_UNIPHY) {
				*clk_ids++ = MHT_MAC4_UNIPHY1_CH3_RX_CLK;
				*clk_ids++ = MHT_MAC4_UNIPHY1_CH3_TX_CLK;
				*clk_ids++ = MHT_MAC4_UNIPHY1_CH3_XGMII_RX_CLK;
				*clk_ids++ = MHT_MAC4_UNIPHY1_CH3_XGMII_TX_CLK;
			}

			if (mask & MHT_CLK_TYPE_EPHY) {
				*clk_ids++ = MHT_MAC4_GEPHY3_TX_CLK;
				*clk_ids++ = MHT_MAC4_GEPHY3_RX_CLK;
			}
			break;
		case 5:
			if (mask & MHT_CLK_TYPE_MAC) {
				*clk_ids++ = MHT_MAC5_TX_CLK;
				*clk_ids++ = MHT_MAC5_RX_CLK;
			}

			if (mask & MHT_CLK_TYPE_UNIPHY) {
				*clk_ids++ = MHT_MAC5_TX_UNIPHY0_CLK;
				*clk_ids++ = MHT_MAC5_RX_UNIPHY0_CLK;
			}
			break;
		default:
			pr_err("Unsupported mht_port_id %d\n", mht_port_id);
			return -EINVAL;
	}

	return 0;
}

int qca8k_port_clk_reset(struct mdio_device *mdiodev, int mht_port_id, u8 mask)
{
	int rv = 0;
	char *clk_ids[MHT_PORT_CLK_CBC_MAX + 1] = {NULL};
	int i = 0;

	rv = qca8k_clk_ids_get(mht_port_id, mask, clk_ids);
	if (rv)
		return rv;

	while(clk_ids[i] != NULL) {
		rv = qca8k_clk_reset(mdiodev, clk_ids[i]);
		if (rv)
			return rv;
		i++;
	}

	return 0;
}
EXPORT_SYMBOL(qca8k_port_clk_reset);

int qca8k_port_clk_en_set(struct mdio_device *mdiodev,
		int mht_port_id, u8 mask, bool enable)
{
	int rv = 0;
	char *clk_ids[MHT_PORT_CLK_CBC_MAX + 1] = {NULL};
	int i = 0;

	rv = qca8k_clk_ids_get(mht_port_id, mask, clk_ids);
	if (rv)
		return rv;

	while(clk_ids[i] != NULL) {
		if (enable)
			rv = qca8k_clk_enable(mdiodev, clk_ids[i]);
		else
			rv = qca8k_clk_disable(mdiodev, clk_ids[i]);

		if (rv)
			return rv;
		i++;
	}

	return 0;
}
EXPORT_SYMBOL(qca8k_port_clk_en_set);

static void qca8k_gcc_common_clk_parent_enable(struct mdio_device *mdiodev, u32 clk_mode)
{
	/* Switch core */
	qca8k_clk_parent_set(mdiodev, MHT_SWITCH_CORE_CLK, MHT_P_UNIPHY1_TX312P5M);
	qca8k_clk_rate_set(mdiodev, MHT_SWITCH_CORE_CLK, UQXGMII_SPEED_2500M_CLK);
	/* Disable switch core clock to save power in phy mode */
	if (MHT_CLK_PHY_UQXGMII_MODE == clk_mode || MHT_CLK_PHY_SGMII_UQXGMII_MODE == clk_mode)
		qca8k_clk_disable(mdiodev, MHT_SWITCH_CORE_CLK);
	else
		qca8k_clk_enable(mdiodev, MHT_SWITCH_CORE_CLK);
	qca8k_clk_enable(mdiodev, MHT_APB_BRIDGE_CLK);

	/* AHB bridge */
	qca8k_clk_parent_set(mdiodev, MHT_AHB_CLK, MHT_P_UNIPHY1_TX312P5M);
	qca8k_clk_rate_set(mdiodev, MHT_AHB_CLK, MHT_AHB_CLK_RATE_104P17M);
	qca8k_clk_enable(mdiodev, MHT_AHB_CLK);
	qca8k_clk_enable(mdiodev, MHT_SEC_CTRL_AHB_CLK);
	qca8k_clk_enable(mdiodev, MHT_TLMM_CLK);
	qca8k_clk_enable(mdiodev, MHT_TLMM_AHB_CLK);
	qca8k_clk_enable(mdiodev, MHT_CNOC_AHB_CLK);
	qca8k_clk_enable(mdiodev, MHT_MDIO_AHB_CLK);
	qca8k_clk_enable(mdiodev, MHT_MDIO_MASTER_AHB_CLK);

	/* System */
	qca8k_clk_parent_set(mdiodev, MHT_SRDS0_SYS_CLK, MHT_P_XO);
	qca8k_clk_rate_set(mdiodev, MHT_SRDS0_SYS_CLK, MHT_SYS_CLK_RATE_25M);
	/* assert serdes0 clock to save power in phy mode */
	if (MHT_CLK_PHY_UQXGMII_MODE == clk_mode)
		qca8k_clk_assert(mdiodev, MHT_SRDS0_SYS_CLK);
	else if (clk_mode != MHT_CLK_SWITCH_MODE)
		qca8k_clk_enable(mdiodev, MHT_SRDS0_SYS_CLK);
	qca8k_clk_enable(mdiodev, MHT_SRDS1_SYS_CLK);
	qca8k_clk_enable(mdiodev, MHT_GEPHY0_SYS_CLK);
	qca8k_clk_enable(mdiodev, MHT_GEPHY1_SYS_CLK);
	qca8k_clk_enable(mdiodev, MHT_GEPHY2_SYS_CLK);
	qca8k_clk_enable(mdiodev, MHT_GEPHY3_SYS_CLK);

	/* Sec control */
	qca8k_clk_parent_set(mdiodev, MHT_SEC_CTRL_CLK, MHT_P_XO);
	qca8k_clk_rate_set(mdiodev, MHT_SEC_CTRL_CLK, MHT_SYS_CLK_RATE_25M);
	qca8k_clk_enable(mdiodev, MHT_SEC_CTRL_CLK);
	qca8k_clk_enable(mdiodev, MHT_SEC_CTRL_SENSE_CLK);
}

void qca8k_gcc_port_clk_parent_set(struct mdio_device *mdiodev,
		u32 clk_mode, int mht_port_id)
{
	mht_clk_parent_t port_tx_parent, port_rx_parent;
	char *tx_clk_id, *rx_clk_id;

	/* Initialize the clock parent with port 1, 2, 3, clock parent is same for these ports;
	 * the clock parent will be updated for port 0, 4, 5.
	 */
	switch(clk_mode) {
		case MHT_CLK_SWITCH_MODE:
		case MHT_CLK_SWITCH_BYPASS_PORT5_MODE:
			port_tx_parent = MHT_P_UNIPHY1_TX312P5M;
			break;
		case MHT_CLK_PHY_UQXGMII_MODE:
		case MHT_CLK_PHY_SGMII_UQXGMII_MODE:
			port_tx_parent = MHT_P_UNIPHY1_RX312P5M;
			break;
		default:
			pr_err("Unsupported clock mode %d\n", clk_mode);
			return;
	}
	port_rx_parent = MHT_P_UNIPHY1_TX312P5M;

	switch (mht_port_id) {
		case 0:
			port_tx_parent = MHT_P_UNIPHY1_TX;
			port_rx_parent = MHT_P_UNIPHY1_RX;
			tx_clk_id = MHT_MAC0_TX_CLK;
			rx_clk_id = MHT_MAC0_RX_CLK;
			break;
		case 1:
			tx_clk_id = MHT_MAC1_TX_CLK;
			rx_clk_id = MHT_MAC1_RX_CLK;
			break;
		case 2:
			tx_clk_id = MHT_MAC2_TX_CLK;
			rx_clk_id = MHT_MAC2_RX_CLK;
			break;
		case 3:
			tx_clk_id = MHT_MAC3_TX_CLK;
			rx_clk_id = MHT_MAC3_RX_CLK;
			break;
		case 4:
			switch(clk_mode) {
				case MHT_CLK_SWITCH_BYPASS_PORT5_MODE:
				case MHT_CLK_PHY_SGMII_UQXGMII_MODE:
					port_tx_parent = MHT_P_UNIPHY0_RX;
					port_rx_parent = MHT_P_UNIPHY0_TX;
					break;
				case MHT_CLK_SWITCH_MODE:
					port_tx_parent = MHT_P_UNIPHY1_TX312P5M;
					port_rx_parent = MHT_P_UNIPHY1_TX312P5M;
					break;
				case MHT_CLK_PHY_UQXGMII_MODE:
					port_tx_parent = MHT_P_UNIPHY1_RX312P5M;
					port_rx_parent = MHT_P_UNIPHY1_TX312P5M;
					break;
				default:
					pr_err("Unsupported clock mode %d\n", clk_mode);
					return;
			}
			tx_clk_id = MHT_MAC4_TX_CLK;
			rx_clk_id = MHT_MAC4_RX_CLK;
			break;
		case 5:
			port_tx_parent = MHT_P_UNIPHY0_TX;
			port_rx_parent = MHT_P_UNIPHY0_RX;
			tx_clk_id = MHT_MAC5_TX_CLK;
			rx_clk_id = MHT_MAC5_RX_CLK;
			switch (clk_mode) {
				case MHT_CLK_SWITCH_BYPASS_PORT5_MODE:
				case MHT_CLK_PHY_SGMII_UQXGMII_MODE:
					qca8k_port5_uniphy0_clk_src_set(mdiodev, true);
					break;
				case MHT_CLK_SWITCH_MODE:
				case MHT_CLK_PHY_UQXGMII_MODE:
					if(clk_mode == MHT_CLK_SWITCH_MODE) {
						port_tx_parent = MHT_P_UNIPHY0_TX;
						port_rx_parent = MHT_P_UNIPHY0_RX;
					}
					qca8k_port5_uniphy0_clk_src_set(mdiodev, false);
					break;
				default:
					pr_err("Unsupported clock mode %d\n", clk_mode);
					return;
			}
			break;
		default:
			pr_err("Unsupported mht_port_id %d\n", mht_port_id);
			return;
	}

	qca8k_clk_parent_set(mdiodev, tx_clk_id, port_tx_parent);
	qca8k_clk_parent_set(mdiodev, rx_clk_id, port_rx_parent);
}
EXPORT_SYMBOL(qca8k_gcc_port_clk_parent_set);

/* The input parameter pbmp will be 0 when the clock mode from device 0 is the following mode:
 * MHT_CLK_SWITCH_BYPASS_PORT5_MODE(for phy port 4),
 * MHT_CLK_PHY_UQXGMII_MODE and MHT_CLK_PHY_SGMII_UQXGMII_MODE.
 *
 * The clock mode MHT_CLK_SWITCH_MODE adn MHT_CLK_SWITCH_BYPASS_PORT5_MODE(for switch device 1) where
 * the pbmp will be acquired from dts.
 */
void qca8k_gcc_clock_init(struct mdio_device *mdiodev, u32 clk_mode, u32 pbmp)
{
	int mht_port_id = 0;
	/* clock type mask value for 6 manhattan ports */
	u8 clk_mask[5 + 1] = {0};
	static int gcc_common_clk_init_bmp = 0;
	bool switch_flag = false;
	mht_clk_parent_t uniphy_index = MHT_P_UNIPHY0_RX;
	int work_mode_bit_index = 0;

	switch (clk_mode) {
		case MHT_CLK_SWITCH_MODE:
		case MHT_CLK_SWITCH_BYPASS_PORT5_MODE:
			work_mode_bit_index = 0;
			while (pbmp) {
				if (pbmp & 1) {
					if (mht_port_id == 0 ||
							mht_port_id == 5) {
						clk_mask[mht_port_id] = MHT_CLK_TYPE_MAC |
							MHT_CLK_TYPE_UNIPHY;
					} else {
						clk_mask[mht_port_id] = MHT_CLK_TYPE_MAC |
							MHT_CLK_TYPE_EPHY;
					}
				}
				pbmp >>= 1;
				mht_port_id++;
			}

			if (clk_mode == MHT_CLK_SWITCH_BYPASS_PORT5_MODE) {
				/* For phy port 4 in switch bypass mode */
				clk_mask[4] = MHT_CLK_TYPE_EPHY;
				clk_mask[5] = MHT_CLK_TYPE_UNIPHY;
				work_mode_bit_index = 1;
			}

			switch_flag = true;
			break;
		case MHT_CLK_PHY_UQXGMII_MODE:
		case MHT_CLK_PHY_SGMII_UQXGMII_MODE:
			work_mode_bit_index = 2;
			clk_mask[1] = MHT_CLK_TYPE_UNIPHY | MHT_CLK_TYPE_EPHY;
			clk_mask[2] = MHT_CLK_TYPE_UNIPHY | MHT_CLK_TYPE_EPHY;
			clk_mask[3] = MHT_CLK_TYPE_UNIPHY | MHT_CLK_TYPE_EPHY;
			clk_mask[4] = MHT_CLK_TYPE_UNIPHY | MHT_CLK_TYPE_EPHY;
			if (clk_mode == MHT_CLK_PHY_SGMII_UQXGMII_MODE) {
				/* For phy port4 in PHY bypass mode */
				clk_mask[4] = MHT_CLK_TYPE_EPHY;
				clk_mask[5] = MHT_CLK_TYPE_UNIPHY;
				work_mode_bit_index = 3;
			}
			break;
		default:
			pr_err("Unsupported clock mode %d\n", clk_mode);
			return;
	}

	if (!(gcc_common_clk_init_bmp & BIT(work_mode_bit_index))) {
		qca8k_gcc_common_clk_parent_enable(mdiodev, clk_mode);
		gcc_common_clk_init_bmp |= BIT(work_mode_bit_index);

		/* Initialize the uniphy raw clock, if the port4 is in bypass mode, the uniphy0
		 * raw clock need to be dynamically updated between UQXGMII_SPEED_2500M_CLK and
		 * UQXGMII_SPEED_1000M_CLK according to the realtime link speed.
		 */
		uniphy_index = MHT_P_UNIPHY0_RX;
		while (uniphy_index <= MHT_P_UNIPHY1_TX) {
			/* the uniphy raw clock may be already initialized. */
			if (0 == qca8k_uniphy_raw_clock_get(uniphy_index))
				qca8k_uniphy_raw_clock_set(uniphy_index,
						UQXGMII_SPEED_2500M_CLK);
			uniphy_index++;
		}
	}

	mht_port_id = 0;
	pbmp = 0;
	while (mht_port_id < ARRAY_SIZE(clk_mask)) {
		if (clk_mask[mht_port_id] != 0) {
			qca8k_gcc_port_clk_parent_set(mdiodev, clk_mode, mht_port_id);
			if (clk_mask[mht_port_id] & MHT_CLK_TYPE_MAC)
				qca8k_port_clk_en_set(mdiodev,
						mht_port_id, MHT_CLK_TYPE_MAC, true);
			if (clk_mask[mht_port_id] & MHT_CLK_TYPE_UNIPHY && switch_flag == true)
				qca8k_port_clk_en_set(mdiodev,
						mht_port_id, MHT_CLK_TYPE_UNIPHY, true);
			pbmp |= BIT(mht_port_id);
		}
		mht_port_id++;
	}

	pr_info("MHT GCC CLK initialization with clock mode %d on port bmp 0x%x\n",
			clk_mode, pbmp);
}
EXPORT_SYMBOL(qca8k_gcc_clock_init);

static ssize_t qca8k_clk_cfg_set(struct file *file,
				 const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct mdio_device *mdiodev = file_inode(file)->i_private;
	char *clk_str, *clk_id, *op_str, *val_str = NULL;
	u32 op_val;

	/* Copy the user space buf */
	clk_str = memdup_user_nul(buf, count);
	if (IS_ERR(clk_str))
		return PTR_ERR(clk_str);

	if (clk_str[count - 1] == '\n')
		clk_str[count - 1] = '\0';

	clk_id = strsep(&clk_str, " ");
	if (!clk_id)
		goto parse_fail;

	op_str = strsep(&clk_str, " ");
	if (!op_str)
		goto parse_fail;

	/* the op_val is optinal */
	val_str = strsep(&clk_str, " ");
	if (val_str) {
		if (kstrtou32(val_str, 0, &op_val) < 0)
			goto parse_fail;
	}

	pr_debug("clk_id: %s, option: %s %s\n", clk_id, op_str, val_str ? val_str : "");

	if (strncasecmp(op_str, "parent", 6) == 0) {
		if (val_str != NULL)
			qca8k_clk_parent_set(mdiodev, clk_id, op_val);
		else {
			pr_err("parent value needed\n");
			goto parse_fail;
		}
	}

	if (strncasecmp(op_str, "rate", 4) == 0) {
		if (val_str != NULL)
			qca8k_clk_rate_set(mdiodev, clk_id, op_val);
		else {
			pr_err("rate value needed\n");
			goto parse_fail;
		}
	}

	if (strncasecmp(op_str, "reset", 5) == 0) {
		qca8k_clk_reset(mdiodev, clk_id);
	}

	if (strncasecmp(op_str, "deassert", 8) == 0) {
		qca8k_clk_deassert(mdiodev, clk_id);
	}

	if (strncasecmp(op_str, "assert", 6) == 0) {
		qca8k_clk_assert(mdiodev, clk_id);
	}

	if (strncasecmp(op_str, "enable", 6) == 0) {
		qca8k_clk_enable(mdiodev, clk_id);
	}

	if (strncasecmp(op_str, "disable", 7) == 0) {
		qca8k_clk_disable(mdiodev, clk_id);
	}

	kfree(clk_str);
	return count;

parse_fail:
	if (clk_str)
		kfree(clk_str);

	pr_info("clk_cfg supported options:\n"
			"clock_id parent parent_value[0-6]\n"
			"Example: echo mht_gcc_mac1_tx_clk parent 6 > clk_cfg\n"
			"clock_id rate rate_value\n"
			"Example: echo mht_gcc_mac1_tx_clk rate 312500000 > clk_cfg\n"
			"clock_id reset\n"
			"Example: echo mht_gcc_mac1_tx_clk reset > clk_cfg\n"
			"clock_id deassert\n"
			"Example: echo mht_gcc_mac1_tx_clk deassert > clk_cfg\n"
			"clock_id assert\n"
			"Example: echo mht_gcc_mac1_tx_clk assert > clk_cfg\n"
			"clock_id enable\n"
			"Example: echo mht_gcc_mac1_tx_clk enable > clk_cfg\n"
			"clock_id disable\n"
			"Example: echo mht_gcc_mac1_tx_clk disable > clk_cfg\n");

	return -EINVAL;
}

static int qca8k_clk_cfg_show(struct seq_file *s, void *data)
{
	struct mdio_device *mdiodev = s->private;
	struct mht_clk_data clk_data;
	struct clk_lookup *clk;
	int i, rv = 0;

	seq_printf(s, "%-31s Frequency RCG_VAL CDIV_VAL CBC_VAL\n",
		   "Clock Name");

	for (i = 0; i < ARRAY_SIZE(mht_clk_lookup_table); i++) {
		clk = &mht_clk_lookup_table[i];
		if (clk->rcg != 0) {
			memset(&clk_data, 0, sizeof(clk_data));

			rv = qca8k_clk_rate_get(mdiodev, clk->clk_name, &clk_data);
			if (rv != 0)
				continue;

			seq_printf(s, "%-31s %-9ld 0x%-5x 0x%-6x 0x%-5x\n",
				   clk->clk_name + 4, clk_data.rate,
				   clk_data.rcg_val, clk_data.cdiv_val,
				   clk_data.cbc_val);
		}
	}

	return 0;
}

static int qca8k_clk_cfg_open(struct inode *inode, struct file *file)
{
	return single_open(file, qca8k_clk_cfg_show, inode->i_private);
}

static const struct file_operations qca8k_clks_fops = {
	.owner   = THIS_MODULE,
	.open    = qca8k_clk_cfg_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
	.write   = qca8k_clk_cfg_set,
};

static int qca8k_cc_probe(struct mdio_device *mdiodev)
{
	struct device *dev = &mdiodev->dev;
	struct qca8k_cc_priv *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->debugfs_dir = debugfs_create_dir(dev_name(dev), NULL);
	debugfs_create_file("clk_cfg", 0600, priv->debugfs_dir, mdiodev,
			    &qca8k_clks_fops);

	mdiodev_set_drvdata(mdiodev, priv);
	dev_info(&mdiodev->dev, "QCA8K CC driver registered.\n");

	return 0;
}

static void qca8k_cc_remove(struct mdio_device *mdiodev)
{
	struct qca8k_cc_priv *priv = mdiodev_get_drvdata(mdiodev);

	debugfs_remove_recursive(priv->debugfs_dir);
}

static const struct of_device_id qca8k_cc_match_table[] = {
	{ .compatible = "qcom,qca8k-cc" },
	{ }
};
MODULE_DEVICE_TABLE(of, qca8k_cc_match_table);

static struct mdio_driver qca8k_cc_driver = {
	.mdiodrv.driver = {
		.name = "qcom,qca8k-cc",
		.of_match_table	= qca8k_cc_match_table,
	},
	.probe = qca8k_cc_probe,
	.remove = qca8k_cc_remove,
};

mdio_module_driver(qca8k_cc_driver);

MODULE_DESCRIPTION("QCOM QCA8K Clock Controller Driver");
MODULE_LICENSE("Dual BSD/GPL");
