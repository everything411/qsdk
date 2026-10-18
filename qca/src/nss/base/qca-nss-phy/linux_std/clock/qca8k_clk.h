/*
 * Copyright (c) 2021-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _QCA8K_CLK_H_
#define _QCA8K_CLK_H_

#include <linux/mdio.h>

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#define MHT_SWITCH_CORE_CLK			"mht_gcc_switch_core_clk"
#define MHT_APB_BRIDGE_CLK			"mht_gcc_apb_bridge_clk"

#define MHT_MAC0_TX_CLK				"mht_gcc_mac0_tx_clk"
#define MHT_MAC0_TX_UNIPHY1_CLK			"mht_gcc_mac0_tx_srds1_clk"

#define MHT_MAC0_RX_CLK				"mht_gcc_mac0_rx_clk"
#define MHT_MAC0_RX_UNIPHY1_CLK			"mht_gcc_mac0_rx_srds1_clk"

#define MHT_MAC1_TX_CLK				"mht_gcc_mac1_tx_clk"
#define MHT_MAC1_GEPHY0_TX_CLK			"mht_gcc_mac1_gephy0_tx_clk"
#define MHT_MAC1_UNIPHY1_CH0_RX_CLK		"mht_gcc_mac1_srds1_ch0_rx_clk"
#define MHT_MAC1_UNIPHY1_CH0_XGMII_RX_CLK	"mht_gcc_mac1_srds1_ch0_xgmii_rx_clk"

#define MHT_MAC1_RX_CLK				"mht_gcc_mac1_rx_clk"
#define MHT_MAC1_GEPHY0_RX_CLK			"mht_gcc_mac1_gephy0_rx_clk"
#define MHT_MAC1_UNIPHY1_CH0_TX_CLK		"mht_gcc_mac1_srds1_ch0_tx_clk"
#define MHT_MAC1_UNIPHY1_CH0_XGMII_TX_CLK	"mht_gcc_mac1_srds1_ch0_xgmii_tx_clk"

#define MHT_MAC2_TX_CLK				"mht_gcc_mac2_tx_clk"
#define MHT_MAC2_GEPHY1_TX_CLK			"mht_gcc_mac2_gephy1_tx_clk"
#define MHT_MAC2_UNIPHY1_CH1_RX_CLK		"mht_gcc_mac2_srds1_ch1_rx_clk"
#define MHT_MAC2_UNIPHY1_CH1_XGMII_RX_CLK	"mht_gcc_mac2_srds1_ch1_xgmii_rx_clk"

#define MHT_MAC2_RX_CLK				"mht_gcc_mac2_rx_clk"
#define MHT_MAC2_GEPHY1_RX_CLK			"mht_gcc_mac2_gephy1_rx_clk"
#define MHT_MAC2_UNIPHY1_CH1_TX_CLK		"mht_gcc_mac2_srds1_ch1_tx_clk"
#define MHT_MAC2_UNIPHY1_CH1_XGMII_TX_CLK	"mht_gcc_mac2_srds1_ch1_xgmii_tx_clk"

#define MHT_MAC3_TX_CLK				"mht_gcc_mac3_tx_clk"
#define MHT_MAC3_GEPHY2_TX_CLK			"mht_gcc_mac3_gephy2_tx_clk"
#define MHT_MAC3_UNIPHY1_CH2_RX_CLK		"mht_gcc_mac3_srds1_ch2_rx_clk"
#define MHT_MAC3_UNIPHY1_CH2_XGMII_RX_CLK	"mht_gcc_mac3_srds1_ch2_xgmii_rx_clk"

#define MHT_MAC3_RX_CLK				"mht_gcc_mac3_rx_clk"
#define MHT_MAC3_GEPHY2_RX_CLK			"mht_gcc_mac3_gephy2_rx_clk"
#define MHT_MAC3_UNIPHY1_CH2_TX_CLK		"mht_gcc_mac3_srds1_ch2_tx_clk"
#define MHT_MAC3_UNIPHY1_CH2_XGMII_TX_CLK	"mht_gcc_mac3_srds1_ch2_xgmii_tx_clk"

#define MHT_MAC4_TX_CLK				"mht_gcc_mac4_tx_clk"
#define MHT_MAC4_GEPHY3_TX_CLK			"mht_gcc_mac4_gephy3_tx_clk"
#define MHT_MAC4_UNIPHY1_CH3_RX_CLK		"mht_gcc_mac4_srds1_ch3_rx_clk"
#define MHT_MAC4_UNIPHY1_CH3_XGMII_RX_CLK	"mht_gcc_mac4_srds1_ch3_xgmii_rx_clk"

#define MHT_MAC4_RX_CLK				"mht_gcc_mac4_rx_clk"
#define MHT_MAC4_GEPHY3_RX_CLK			"mht_gcc_mac4_gephy3_rx_clk"
#define MHT_MAC4_UNIPHY1_CH3_TX_CLK		"mht_gcc_mac4_srds1_ch3_tx_clk"
#define MHT_MAC4_UNIPHY1_CH3_XGMII_TX_CLK	"mht_gcc_mac4_srds1_ch3_xgmii_tx_clk"

#define MHT_MAC5_TX_CLK				"mht_gcc_mac5_tx_clk"
#define MHT_MAC5_TX_UNIPHY0_CLK			"mht_gcc_mac5_tx_srds0_clk"
#define MHT_MAC5_TX_SRDS0_CLK_SRC		"mht_gcc_mac5_tx_srds0_clk_src"

#define MHT_MAC5_RX_CLK				"mht_gcc_mac5_rx_clk"
#define MHT_MAC5_RX_UNIPHY0_CLK			"mht_gcc_mac5_rx_srds0_clk"
#define MHT_MAC5_RX_SRDS0_CLK_SRC		"mht_gcc_mac5_rx_srds0_clk_src"

#define MHT_SEC_CTRL_CLK			"mht_gcc_sec_ctrl_clk"
#define MHT_SEC_CTRL_SENSE_CLK			"mht_gcc_sec_ctrl_sense_clk"

#define MHT_SRDS0_SYS_CLK			"mht_gcc_srds0_sys_clk"
#define MHT_SRDS1_SYS_CLK			"mht_gcc_srds1_sys_clk"
#define MHT_GEPHY0_SYS_CLK			"mht_gcc_gephy0_sys_clk"
#define MHT_GEPHY1_SYS_CLK			"mht_gcc_gephy1_sys_clk"
#define MHT_GEPHY2_SYS_CLK			"mht_gcc_gephy2_sys_clk"
#define MHT_GEPHY3_SYS_CLK			"mht_gcc_gephy3_sys_clk"

#define MHT_AHB_CLK				"mht_gcc_ahb_clk"
#define MHT_SEC_CTRL_AHB_CLK			"mht_gcc_sec_ctrl_ahb_clk"
#define MHT_TLMM_CLK				"mht_gcc_tlmm_clk"
#define MHT_TLMM_AHB_CLK			"mht_gcc_tlmm_ahb_clk"
#define MHT_CNOC_AHB_CLK			"mht_gcc_cnoc_ahb_clk"
#define MHT_MDIO_AHB_CLK			"mht_gcc_mdio_ahb_clk"
#define MHT_MDIO_MASTER_AHB_CLK			"mht_gcc_mdio_master_ahb_clk"

#define MHT_GLOBAL_RST				"mht_gcc_global_rst"
#define MHT_UNIPHY_XPCS_RST			"mht_uniphy_xpcs_rst"
#define MHT_GEPHY_DSP_HW_RST			"mht_gephy_dsp_hw_rst"
#define MHT_GEPHY_P3_MDC_SW_RST			"mht_gephy_p3_mdc_sw_rst"
#define MHT_GEPHY_P2_MDC_SW_RST			"mht_gephy_p2_mdc_sw_rst"
#define MHT_GEPHY_P1_MDC_SW_RST			"mht_gephy_p1_mdc_sw_rst"
#define MHT_GEPHY_P0_MDC_SW_RST			"mht_gephy_p0_mdc_sw_rst"

typedef enum {
	MHT_P_XO,
	MHT_P_UNIPHY0_RX,
	MHT_P_UNIPHY0_TX,
	MHT_P_UNIPHY1_RX,
	MHT_P_UNIPHY1_TX,
	MHT_P_UNIPHY1_RX312P5M,
	MHT_P_UNIPHY1_TX312P5M,
	MHT_P_MAX,
} mht_clk_parent_t;

struct mht_clk_data {
	unsigned long rate;
	unsigned int rcg_val;
	unsigned int cdiv_val;
	unsigned int cbc_val;
};

struct mht_parent_data {
	unsigned long prate;		/* RCG input clock rate */
	mht_clk_parent_t parent;	/* RCG parent clock id */
	int cfg;			/* RCG clock src value */
};

struct clk_lookup {
	unsigned int rcg;
	unsigned int cdiv;
	unsigned int cbc;
	unsigned int rst_bit;
	const char *clk_name;
	const unsigned long *support_rate;
	unsigned int num_rate;
	const struct mht_parent_data *pdata;
	unsigned int num_parent;
};

#define CLK_LOOKUP(_rcg, _cdiv, _cbc, _rst_bit, _clk_name,		\
		_rate, _num_rate, _pdata, _num_parent)			\
{									\
	.rcg = _rcg,							\
	.cdiv = _cdiv,							\
	.cbc = _cbc,							\
	.rst_bit = _rst_bit,						\
	.clk_name = _clk_name,						\
	.support_rate = _rate,						\
	.num_rate = _num_rate,						\
	.pdata = _pdata,						\
	.num_parent = _num_parent,					\
}

#define MHT_CLK_TYPE_EPHY			BIT(0)
#define MHT_CLK_TYPE_UNIPHY			BIT(1)
#define MHT_CLK_TYPE_MAC			BIT(2)

#define UQXGMII_SPEED_2500M_CLK			312500000
#define UQXGMII_SPEED_1000M_CLK			125000000
#define UQXGMII_SPEED_100M_CLK			25000000
#define UQXGMII_SPEED_10M_CLK			2500000
#define UQXGMII_XPCS_SPEED_2500M_CLK		78125000
#define MHT_AHB_CLK_RATE_104P17M		104160000
#define MHT_SYS_CLK_RATE_25M			25000000
#define MHT_XO_CLK_RATE_50M			50000000

#define MHT_CLK_BASE_REG			0x0c800000
#define MHT_CLK_MUX_SEL				0x300
#define MHT_UNIPHY0_MUX_SEL_MASK		GENMASK(1, 0)
#define MHT_UNIPHY0_SEL_MAC5			0x3
#define MHT_UNIPHY0_SEL_MAC4			0

#define RCGR_CMD_ROOT_OFF			BIT(31)
#define RCGR_CMD_UPDATE				BIT(0)
#define RCGR_SRC_SEL				GENMASK(10, 8)
#define RCGR_SRC_SEL_SHIFT			8
#define RCGR_HDIV				GENMASK(4, 0)
#define RCGR_HDIV_SHIFT				0
#define RCGR_DIV_BYPASS				0
#define RCGR_DIV_MAX				0x1f
#define CDIVR_DIVIDER_10			9	/* CDIVR divided by N + 1 */
#define CDIVR_DIVIDER				GENMASK(3, 0)
#define CDIVR_DIVIDER_SHIFT			0
#define CBCR_CLK_OFF				BIT(31)
#define CBCR_CLK_RESET				BIT(2)
#define CBCR_CLK_ENABLE				BIT(0)

bool qca8k_clk_is_asserted(struct mdio_device *mdiodev, const char *clock_id);
int qca8k_clk_assert(struct mdio_device *mdiodev, const char *clock_id);
int qca8k_clk_deassert(struct mdio_device *mdiodev, const char *clock_id);
int qca8k_clk_reset(struct mdio_device *mdiodev, const char *clock_id);
int qca8k_clk_enable(struct mdio_device *mdiodev, const char *clock_id);
int qca8k_clk_disable(struct mdio_device *mdiodev, const char *clock_id);
int qca8k_clk_parent_set(struct mdio_device *mdiodev,
		const char *clock_id, mht_clk_parent_t parent);
int qca8k_clk_rate_set(struct mdio_device *mdiodev, const char *clock_id, unsigned long rate);
int qca8k_port5_uniphy0_clk_src_set(struct mdio_device *mdiodev, bool bypass_en);
int qca8k_port5_uniphy0_clk_src_get(struct mdio_device *mdiodev, bool *bypass_en);
int qca8k_port_clk_rate_set(struct mdio_device *mdiodev, int port_id, unsigned long rate);
int qca8k_port_clk_reset(struct mdio_device *mdiodev, int port_id, u8 mask);
int qca8k_port_clk_en_set(struct mdio_device *mdiodev,
		int port_id, u8 mask, bool enable);
unsigned long qca8k_uniphy_raw_clock_get(mht_clk_parent_t uniphy_clk);
void qca8k_uniphy_raw_clock_set(mht_clk_parent_t uniphy_clk, unsigned long rate);
void qca8k_gcc_clock_init(struct mdio_device *mdiodev, u32 clk_mode, u32 pbmp);
int qca8k_clk_rate_get(struct mdio_device *mdiodev,
		const char *clock_id, struct mht_clk_data *clk_data);
bool qca8k_clk_is_enabled(struct mdio_device *mdiodev, const char *clock_id);
void qca8k_gcc_port_clk_parent_set(struct mdio_device *mdiodev,
		u32 clk_mode, int mht_port_id);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _QCA8K_CLK_H_ */
