// SPDX-License-Identifier: GPL-2.0+
/*
 * QCE1204 PHY Driver for U-Boot
 * Based on NSS driver reference and QCA81xx skeleton
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <command.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <phy.h>
#include <miiphy.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <clk.h>
#include <reset.h>
#include <dm/device.h>
#include <dm/of_access.h>
#include <malloc.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <log.h>
#include "qcom_qce2204_ppe.h"

#define QCE1204_PHY_ID                          0x004dd190
#define IPQ52XX_PHY_ID                          0x004dd120

/* Address Offsets */
enum qce1204_addr_offset {
	PCS0_ADDR_OFFSET = 4,
	PCS1_ADDR_OFFSET = 5,
	SOC_ADDR_OFFSET = 6,
};

/* PHY Registers */
#define QCE1204_PHY_SPEC_STATUS                 0x11
#define QCE1204_PHY_SS_LINK_STATUS              0x400
#define QCE1204_PHY_SS_SPEED_MASK               0x380
#define QCE1204_PHY_SS_SPEED_2500               0x200
#define QCE1204_PHY_SS_SPEED_1000               0x100
#define QCE1204_PHY_SS_SPEED_100                0x80
#define QCE1204_PHY_SS_SPEED_10                 0
#define QCE1204_PHY_SS_DUPLEX_FULL              0x2000
#define QCE1204_PHY_CONTROL                     0x19
#define QCE1204_PHY_FIFO_RESET                  0x3
#define QCE1204_PHY_MMD7_IPG_OP                 0x901d
#define QCE1204_PHY_IPG_10_TO_11_EN             0x1

/* PCS MMD1 Registers */
#define QCE1204_PCS_MMD1_CALIBRATION4           0x78
#define QCE1204_PCS_MMD1_MODE_CTRL              0x11b
#define QCE1204_PCS_MMD1_BYPASS_TUNING_IPG      0x189
#define QCE1204_PCS_MMD1_GMII_DATAPASS_SEL      0x180
#define QCE1204_PCS_MMD1_QUSGMII_RESET          0x18c
#define QCE1204_PCS_MMD1_CDA_CONTROL1           0x20
#define QCE1204_PCS_MMD1_PLL_POWER_ON_AND_RESET 0x1e0
#define QCE1204_PCS_MMD1_SSC_CLK                0x166
#define QCE1204_PCS_MMD1_MS_LDO0                0x16d
#define QCE1204_PCS_MMD1_MS_LDO1                0x16e

/* PCS MMD1 Register Fields */
#define QCE1204_PCS_MMD1_CALIBRATION_DONE       0x80
#define QCE1204_PCS_MMD1_XPCS_MODE              0x1000
#define QCE1204_PCS_MMD1_DATAPASS_MASK          0x1
#define QCE1204_PCS_MMD1_DATAPASS_QUSGMII       0x1
#define QCE1204_PCS_MMD1_SSCG_ENABLE            0x8
#define QCE1204_PCS_MMD1_SSC_CLK_EN             0x80
#define QCE1204_PCS_MMD1_ANA_SOFT_RESET_MASK    0x40
#define QCE1204_PCS_MMD1_ANA_SOFT_RESET         0
#define QCE1204_PCS_MMD1_ANA_SOFT_RELEASE       0x40
#define QCE1204_PCS_MMD1_QUSGMII_FUNC_RESET     0x10

/* PCS MMD3 Registers */
#define QCE1204_PCS_MMD3_PCS_CTRL2              0x7
#define QCE1204_PCS_MMD3_10GBASE_PCS_STATUS1    0x20
#define QCE1204_PCS_MMD3_DIG_CTRL1              0x8000
#define QCE1204_PCS_MMD3_VR_RPCS_TPC            0x8007
#define QCE1204_PCS_MMD3_MII_AM_INTERVAL        0x800a

/* PCS MMD3 Register Fields */
#define QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R     0
#define QCE1204_PCS_MMD3_10GBASE_UP             0x1000
#define QCE1204_PCS_MMD3_QUSGMII_EN             0x200
#define QCE1204_PCS_MMD3_QUSGMII_MODE           0x1400
#define QCE1204_PCS_MMD3_MII_AM_INTERVAL_VAL    0x6018
#define QCE1204_PCS_MMD3_XPCS_SOFT_RESET        0x8000
#define QCE1204_PCS_MMD3_QUSGMII_FIFO_RESET     0x400

/* PCS MMD31 (VEND2) Registers */
#define QCE1204_PCS_MMD_MII_CTRL                0
#define QCE1204_PCS_SPEED_MASK			0x2060
#define QCE1204_PCS_SPEED_10M			0
#define QCE1204_PCS_MMD_MII_DIG_CTRL            0x8000
#define QCE1204_PCS_MMD_MII_AN_INT_MSK          0x8001
#define QCE1204_PCS_MMD_MII_XAUI_MODE_CTRL      0x8004

/* PCS MMD31 Register Fields */
#define QCE1204_PCS_MMD_MII_AN_ENABLE           0x1000
#define QCE1204_PCS_MMD_PHY_MODE_CTRL_EN        0x1
#define QCE1204_PCS_MMD_AN_COMPLETE_INT         0x1
#define QCE1204_PCS_MMD_MII_4BITS_CTRL          0x0
#define QCE1204_PCS_MMD_TX_CONFIG_CTRL          0x8
#define QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE    0x1

/* CDT Threshold Registers */
#define QCE1204_MMD3_CDT_THRESH_CTRL2           0x8073
#define QCE1204_MMD3_CDT_THRESH_CTRL3           0x8074
#define QCE1204_MMD3_CDT_THRESH_CTRL4           0x8075
#define QCE1204_MMD3_CDT_THRESH_CTRL5           0x8076
#define QCE1204_MMD3_CDT_THRESH_CTRL6           0x8077
#define QCE1204_MMD3_CDT_THRESH_CTRL7           0x8078
#define QCE1204_MMD3_CDT_THRESH_CTRL9           0x807a
#define QCE1204_MMD3_CDT_THRESH_CTRL13          0x807e
#define QCE1204_MMD3_CDT_THRESH_CTRL14          0x807f

/* CDT Threshold Values */
#define QCE1204_MMD3_CDT_THRESH_CTRL2_VAL       0xb03f
#define QCE1204_MMD3_CDT_THRESH_CTRL3_VAL       0xc040
#define QCE1204_MMD3_CDT_THRESH_CTRL4_VAL       0xa060
#define QCE1204_MMD3_CDT_THRESH_CTRL5_VAL       0xc040
#define QCE1204_MMD3_CDT_THRESH_CTRL6_VAL       0xa060
#define QCE1204_MMD3_CDT_THRESH_CTRL7_VAL       0xae50
#define QCE1204_MMD3_CDT_THRESH_CTRL9_VAL       0xc060
#define QCE1204_MMD3_CDT_THRESH_CTRL13_VAL      0xb060
#define QCE1204_MMD3_CDT_THRESH_CTRL14_VAL      0xb6b0

/* Channel MMD mapping */
#define QCE1204_PCS_MMD_CH2                     26
#define QCE1204_PCS_MMD_CH3                     27
#define QCE1204_PCS_MMD_CH4                     28

/* EEE Registers */
#define QCE1204_PCS_MMD3_AN_LP_BASE_ABL2        0x14
#define QCE1204_PCS_MMD3_EEE_MODE_CTRL          0x8006
#define QCE1204_PCS_MMD3_EEE_TX_TIMER           0x8008
#define QCE1204_PCS_MMD3_EEE_RX_TIMER           0x8009
#define QCE1204_PCS_MMD3_EEE_MODE_CTRL1         0x800b

/* EEE Register Fields */
#define QCE1204_PCS_MMD3_XPCS_EEE_CAP           0x40
#define QCE1204_PCS_MMD3_EEE_RES_REGS           0x100
#define QCE1204_PCS_MMD3_EEE_SIGN_BIT_REGS      0x40
#define QCE1204_PCS_MMD3_EEE_EN                 0x3
#define QCE1204_PCS_MMD3_EEE_TSL_REGS           0xa
#define QCE1204_PCS_MMD3_EEE_TLU_REGS           0xc0
#define QCE1204_PCS_MMD3_EEE_TWL_REGS           0x1600
#define QCE1204_PCS_MMD3_EEE_100US_REG_REGS     0xc8
#define QCE1204_PCS_MMD3_EEE_RWR_REG_REGS       0x1c00
#define QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE     0x1
#define QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE  0x100

/* 2.5G EEE TX LPI Control */
#define QCE1024_PHY_2P5G_EEE_TX_LPI_CTRL        0xa10c
#define QCE1024_PHY_TX_LPI_DELAY_SEL_MASK       0xf00
#define QCE1024_PHY_TX_LPI_DELAY_SEL_1          0x100
#define QCE1024_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL0 0xa02c
#define QCE1024_PHY_2P5G_EEE_TX_QUIET_TIME0     0x3db
#define QCE1024_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL1 0xa031
#define QCE1024_PHY_2P5G_EEE_TX_QUIET_TIME1     0x14
#define QCE1024_PHY_2P5G_EEE_TX_LPI_WAKE_CTRL	0xa10c
#define QCE1024_PHY_2P5G_EEE_TX_WAKE_TIME       0x2fc

/* Debug/DAC Registers */
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL0         0x2880
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL0_VAL     0x7777
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL1         0x3880
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL1_VAL     0xa4a4
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL2         0x3980
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL2_VAL     0xa4a4
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL3		0xc980
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL3_VAL	0xc0
#define QCE1204_DEBUG_PLL_CTRL			0x1f
#define QCE1204_DEBUG_PLL0_FORCE_ON		BIT(2)
#define QCE1204_DEBUG_ANA_2P5G_TX_GAIN_CTRL	0xbb80
#define QCE1204_DEBUG_ANA_2P5G_TX_GAIN_VAL	0xf33

#define QCE1204_MMD7_LED0_CTRL			0x8078
#define QCE1204_SPEED_10M_ON			BIT(4)

/* Debug register access constants */
#define QCE1204_DEBUG_ADDR                      0x1d
#define QCE1204_DEBUG_DATA                      0x1e

/* SOC/GCC Registers */
#define QCE1204_EPHY_CFG                        0x90F018
#define QCE1204_EPHY_LDO_CTRL                   GENMASK(9, 8)
#define QCE1204_WORK_MODE_SEL                   0x90f030
#define QCE1204_PHY_MODE_MASK                   GENMASK(3, 0)
#define QCE1204_PHY_MODE                        0xf
#define QCE1204_SWITCH_MODE_MASK                GENMASK(5, 0)
#define QCE1204_SWITCH_MODE                     0x10

/* TLMM/GPIO Definitions */
#define QCE1204_TLMM_BASE                       0x400000
#define QCE1204_TLMM_GPIO_OFFSET                0x1000
#define TO_TLMM_CFG_REG(pin) \
	(QCE1204_TLMM_BASE + QCE1204_TLMM_GPIO_OFFSET * (pin))
#define QCE1204_TLMM_GPIO_PULL                  GENMASK(1, 0)
#define QCE1204_TLMM_FUNC_MASK                  GENMASK(5, 2)
#define QCE1204_TLMM_DRV                        GENMASK(8, 6)
#define QCE1204_TLMM_DRV_16_MA                  0x1c0
#define QCE1204_TLMM_LED_MODE                   BIT(11)

#define QCA81XX_MMD3_10G_FRAME_CHECK_CTRL	0xa110
#define QCA81XX_MMD3_10G_FRAME_CHECK_EN		0x80
#define QCA81XX_MMD7_COUNTER_CTRL		0x8029
#define QCA81XX_MMD7_FRAME_CHECK_EN		1
#define QCA81XX_MMD7_CNT_SELFCLR		2

enum {
	QCE1204_GPIO0_PHY_INT = 0,
	QCE1204_GPIO1_P0_LED_0,
	QCE1204_GPIO2_P1_LED_0,
	QCE1204_GPIO3_P2_LED_0,
	QCE1204_GPIO4_P3_LED_0,
	QCE1204_GPIO9_P0_WOL_INT = 9,
	QCE1204_GPIO15_P1_WOL_INT = 15,
	QCE1204_GPIO16_P2_WOL_INT,
	QCE1204_GPIO17_P3_WOL_INT,
	QCE1204_GPIO_MAX
};

/* Speed definitions for U-Boot compatibility */
#ifndef SPEED_UNKNOWN
#define SPEED_UNKNOWN   -1
#endif

/* Valid clock rates for QCE1204 */
#define QCE1204_CLK_RATE_2P5M			2500000
#define QCE1204_CLK_RATE_12P5M			12500000
#define QCE1204_CLK_RATE_25M			25000000
#define QCE1204_CLK_RATE_78P125M		78125000
#define QCE1204_CLK_RATE_104M			104170000
#define QCE1204_CLK_RATE_125M			125000000
#define QCE1204_CLK_RATE_156P25M		156250000
#define QCE1204_CLK_RATE_312P5M			312500000

#define NSS_CC_EPHY_RX_MUX_SEL			0x39B00610
#define NSS_CC_EPHY_TX_MUX_SEL			0x39B00614
/* IPQ52xx TCSR GPHY LDO registers */
#define TCSR_GPHY_LDO_BIAS_EN			0x1961000
#define GPHY_LDO_BIAS_EN			BIT(0)
/* IPQ52xx CMN PLL source select register */
#define CMN_PLL_SRC_SEL_REG			0x9B42c
#define CMN_PLL_312P5M_SEL			BIT(10)

/* Clock type index for each channel */
enum qce1204_clk_type {
	QCE1204_CLK_GMII_TX = 0,
	QCE1204_CLK_GMII_RX,
	QCE1204_CLK_XGMII_TX,
	QCE1204_CLK_XGMII_RX,
	QCE1204_CLK_TYPE_MAX
};

#ifdef CONFIG_PHY_QCE_2204
#define QCE1204_MAX_SWITCH_PORTS		5

struct qce1204_switch_port {
	int		 port_id;
	int		 phy_addr;
	struct clk	 tx_clk;
	struct clk	 rx_clk;
	struct reset_ctl tx_reset;
	struct reset_ctl rx_reset;
	struct phy_device *ext_phydev;
	/* Cached link state — updated after each successful configuration */
	int		 last_link;
	int		 last_speed;
	int		 last_duplex;
	bool		 configured;
	bool		 valid;
	bool		 is_external;
};

static bool g_switch_ppe_initialized;
#endif

/* Per-channel clocks and resets */
struct qce1204_channel_clk {
	struct clk clks[QCE1204_CLK_TYPE_MAX];
	struct reset_ctl resets[QCE1204_CLK_TYPE_MAX];
};

struct qce1204_shared_clk_data {
	struct qce1204_channel_clk channels[4];
	struct clk pcs_sys_clk;
	struct clk ahb_clk;
	struct reset_ctl pcs_sys_reset;
	struct reset_ctl xpcs_reset;
	struct clk switch_core_clk;
	struct clk switch_ipe_clk;
	struct clk switch_btq_clk;
	struct clk switch_cfg_clk;
	struct clk switch_apb_clk;
	struct clk mac0_tx_clk;
	struct clk mac0_rx_clk;
	struct clk mac1_tx_clk;
	struct clk mac1_rx_clk;
	struct clk mac2_tx_clk;
	struct clk mac2_rx_clk;
	struct clk mac3_tx_clk;
	struct clk mac3_rx_clk;
	struct clk mac4_tx_clk;
	struct clk mac4_rx_clk;
	/* MAC5 gate clocks for port 5 USXGMII (SRDS0) */
	struct clk mac5_tx_clk;          /* QCE2204_NSSCC_MAC5_TX_CLK       0x220 */
	struct clk mac5_rx_clk;          /* QCE2204_NSSCC_MAC5_RX_CLK       0x23c */
	struct clk mac5_tx_xgmii_clk;    /* QCE2204_NSSCC_MAC5_TX_SRDS0_CH0_XGMII_CLK 0x228 */
	struct clk mac5_rx_xgmii_clk;    /* QCE2204_NSSCC_MAC5_RX_SRDS0_CH0_XGMII_CLK 0x248 */
	/* MAC5 SRDS0 channel resets for port 5 USXGMII (PCS_FUNC_TX/RX in Linux) */
	struct reset_ctl mac5_tx_srds0_reset; /* QCE2204_NSSCC_MAC5_TX_SRDS0_ARES 0x224 bit2 */
	struct reset_ctl mac5_rx_srds0_reset; /* QCE2204_NSSCC_MAC5_RX_SRDS0_ARES 0x244 bit2 */
	/* MAC5 XGMII resets for port 5 USXGMII (XPCS_FUNC_XGMII_TX/RX in Linux) */
	struct reset_ctl mac5_tx_xgmii_reset; /* QCE2204_NSSCC_MAC5_TX_SRDS0_CH0_XGMII_ARES 0x228 bit2 */
	struct reset_ctl mac5_rx_xgmii_reset; /* QCE2204_NSSCC_MAC5_RX_SRDS0_CH0_XGMII_ARES 0x248 bit2 */
	struct reset_ctl switch_btq_reset;
	struct reset_ctl switch_cfg_reset;
	struct reset_ctl switch_core_reset;
	struct reset_ctl switch_ipe_reset;
	struct reset_ctl switch_mac0_reset;
	struct reset_ctl switch_mac1_reset;
	struct reset_ctl switch_mac2_reset;
	struct reset_ctl switch_mac3_reset;
	struct reset_ctl switch_mac4_reset;
	struct reset_ctl switch_mac5_reset;
	struct reset_ctl xgmac0_ptp_ref_reset;
	struct reset_ctl xgmac1_ptp_ref_reset;
	struct reset_ctl mac0_tx_reset;
	struct reset_ctl mac0_rx_reset;
	struct reset_ctl mac1_tx_reset;
	struct reset_ctl mac1_rx_reset;
	struct reset_ctl mac2_tx_reset;
	struct reset_ctl mac2_rx_reset;
	struct reset_ctl mac3_tx_reset;
	struct reset_ctl mac3_rx_reset;
	struct reset_ctl mac4_tx_reset;
	struct reset_ctl mac4_rx_reset;
	struct reset_ctl mac5_tx_reset;
	struct reset_ctl mac5_rx_reset;
	/* SRDS0 (PCS0) clocks and resets for port 5 USXGMII */
	struct clk srds0_sys_clk;
	struct reset_ctl srds0_sys_reset;
	struct reset_ctl srds0_xpcs_reset;
};

struct qce1204_clk_data {
	struct clk tx_clk;
	struct clk rx_clk;
	struct clk sys_clk;
	struct reset_ctl tx_reset;
	struct reset_ctl rx_reset;
	struct reset_ctl sys_reset;
};

struct qce1204_priv {
	struct qce1204_clk_data clk_data;
	struct qce1204_shared_clk_data *shared_clk_data;
	phy_interface_t package_mode;
	bool clocks_initialized;
	u8 base_phy_addr;
#ifdef CONFIG_PHY_QCE_2204
	struct qce1204_switch_port sw_ports[QCE1204_MAX_SWITCH_PORTS];
	int num_sw_ports;
#endif
};

struct ipq52xx_phy_priv {
	struct clk	 ephy_rx_clk;
	struct clk	 ephy_tx_clk;
	struct clk	 sys_clk;
	struct reset_ctl ephy_rx_reset;
	struct reset_ctl ephy_tx_reset;
	struct reset_ctl sys_clk_reset;
	u32 *ldo_bias_reg;
	u32 *pll_src_sel_reg;
	u32 *ephy_rx_mux_reg;
	u32  ephy_rx_mux_val;
	u32 *ephy_tx_mux_reg;
	u32  ephy_tx_mux_val;
	u32 *gmii_rx_reg;
	u32 *gmii_tx_reg;
	u32 *rx_clk_cmd_reg;
	u32 *tx_clk_cmd_reg;
};
static struct qce1204_shared_clk_data *g_shared_clk_data;

static int qce1204_phy_fifo_reset(struct phy_device *phydev, bool enable);
static int qce1204_phy_debug_write(struct phy_device *phydev, unsigned int reg, u16 val);

static int qce1204_soc_addr_get(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;

	return  priv->base_phy_addr + SOC_ADDR_OFFSET;
}

static void qce1204_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid, u16 *reg_high)
{
	*reg_low = (regaddr & 0xc) << 1;
	*reg_mid = regaddr >> 4 & 0xffff;
	*reg_high = ((regaddr >> 20) & 0xf) << 1 | BIT(0);
}

u32 qce1204_soc_read(struct phy_device *phydev, u32 reg)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;
	struct phy_device local_phydev;

	addr = qce1204_soc_addr_get(phydev);
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = addr;

	qce1204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	/* Write AHB address bit4~bit23 */
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(100);

	/* Write AHB address bit0~bit3 and read low 16bit data */
	lo = phy_read(&local_phydev, MDIO_DEVAD_NONE, reg_low);

	/* Write AHB address bit0~bit3 and read high 16 bit data */
	hi = phy_read(&local_phydev, MDIO_DEVAD_NONE, reg_low + 4);

	debug("##%s | SOC[0x%08x]: 0x%08x\n", __func__, reg, (hi << 16) | lo);
	return (hi << 16) | lo;
}

int qce1204_soc_write(struct phy_device *phydev, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;
	struct phy_device local_phydev;

	addr = qce1204_soc_addr_get(phydev);
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = addr;

	qce1204_split_addr(reg, &reg_low, &reg_mid, &reg_high);
	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(100);

	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_low, lo);

	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_low + 4, hi);

	debug("## %s | %d | reg : 0x%x | val: 0x%x [%s]\n", __func__, __LINE__,
	      reg, val, qce1204_soc_read(phydev, reg) == val ? "MATCH" : "NO_MATCH");

	return 0;
}

static int qce1204_soc_modify(struct phy_device *phydev, u32 reg, u32 mask, u32 set)
{
	u32 val;

	val = qce1204_soc_read(phydev, reg);
	val = (val & ~mask) | set;
	return qce1204_soc_write(phydev, reg, val);
}

static int qce1204_pcs_read_mmd(struct phy_device *phydev, int devad,
				int regnum)
{
	struct phy_device local_phydev;
	int ret;
	struct qce1204_priv *priv = phydev->priv;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = priv->base_phy_addr + PCS1_ADDR_OFFSET;

	ret = phy_read(&local_phydev, devad, regnum);
	return ret;
}

static int qce1204_pcs_write_mmd(struct phy_device *phydev, int devad,
				 int regnum, u16 val)
{
	struct phy_device local_phydev;
	int ret;
	struct qce1204_priv *priv = phydev->priv;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = priv->base_phy_addr + PCS1_ADDR_OFFSET;

	ret = phy_write(&local_phydev, devad, regnum, val);
	return ret;
}

static int qce1204_pcs_modify_mmd(struct phy_device *phydev, int devad,
				  int regnum, u16 mask, u16 set)
{
	int new, ret;
	struct phy_device local_phydev;
	struct qce1204_priv *priv = phydev->priv;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = priv->base_phy_addr + PCS1_ADDR_OFFSET;

	ret = phy_read(&local_phydev, devad, regnum);
	if (ret < 0)
		return ret;

	new = (ret & ~mask) | set;
	ret = phy_write(&local_phydev, devad, regnum, new);

	return ret;
}

#ifdef CONFIG_PHY_QCE_2204
/* PCS0 (SRDS0) MMD accessors — mirror of PCS1 wrappers, using PCS0_ADDR_OFFSET */
static int qce1204_pcs0_read_mmd(struct phy_device *phydev, int devad, int regnum)
{
	struct qce1204_priv *priv = phydev->priv;
	u32 saved_addr = phydev->addr;
	int ret;

	phydev->addr = priv->base_phy_addr + PCS0_ADDR_OFFSET;
	ret = phy_read(phydev, devad, regnum);
	phydev->addr = saved_addr;
	return ret;
}

static int qce1204_pcs0_write_mmd(struct phy_device *phydev, int devad, int regnum, u16 val)
{
	struct qce1204_priv *priv = phydev->priv;
	u32 saved_addr = phydev->addr;
	int ret;

	phydev->addr = priv->base_phy_addr + PCS0_ADDR_OFFSET;
	ret = phy_write(phydev, devad, regnum, val);
	phydev->addr = saved_addr;
	return ret;
}

static int qce1204_pcs0_modify_mmd(struct phy_device *phydev, int devad,
				   int regnum, u16 mask, u16 set)
{
	struct qce1204_priv *priv = phydev->priv;
	u32 saved_addr = phydev->addr;
	int new, ret;

	phydev->addr = priv->base_phy_addr + PCS0_ADDR_OFFSET;
	ret = phy_read(phydev, devad, regnum);
	if (ret >= 0) {
		new = (ret & ~mask) | set;
		ret = phy_write(phydev, devad, regnum, new);
	}
	phydev->addr = saved_addr;
	return ret;

}
#endif

static int qce1204_pcs_mmd_get(struct phy_device *phydev, int channel)
{
	switch (channel) {
	case 1:
		return MDIO_MMD_VEND2;
	case 2:
		return QCE1204_PCS_MMD_CH2;
	case 3:
		return QCE1204_PCS_MMD_CH3;
	case 4:
		return QCE1204_PCS_MMD_CH4;
	default:
		return -EOPNOTSUPP;
	}
}

static int qce1204_pcs_modify_channel_mmd(struct phy_device *phydev,
					  int channel, int regnum,
					   u16 mask, u16 set)
{
	int mmd_id = qce1204_pcs_mmd_get(phydev, channel);

	if (mmd_id < 0)
		return -EOPNOTSUPP;

	return qce1204_pcs_modify_mmd(phydev, mmd_id, regnum, mask, set);
}

static int qce1204_ensure_clock_controller_ready(struct phy_device *phydev)
{
	struct udevice *clk_dev = NULL;
	ofnode mdio_node, clk_node;
	u32 reg;
	int ret;
	static bool clock_controller_probed;

	/* Only do this once per system */
	if (clock_controller_probed)
		return 0;

	/* Get the MDIO bus device node */
	if (!phydev->bus || !phydev->bus->priv) {
		debug("QCE1204: No MDIO bus device available\n");
		return -ENODEV;
	}

	mdio_node = dev_ofnode(phydev->bus->priv);
	if (!ofnode_valid(mdio_node)) {
		debug("QCE1204: Invalid MDIO device node\n");
		return -ENODEV;
	}

	/* Find the clock controller node (should be at reg = 6) */
	ofnode_for_each_subnode(clk_node, mdio_node) {
		ret = ofnode_read_u32(clk_node, "reg", &reg);
		if (ret == 0 && reg == qce1204_soc_addr_get(phydev)) {
			/* Check if this is the clock controller */
			if (ofnode_device_is_compatible(clk_node, "qcom,qce2204-nsscc")) {
				debug("QCE1204: Found clock controller at MDIO address %d\n", reg);
				goto found_clk_node;
			}
		}
	}

	return -ENODEV;

found_clk_node:
	ret = uclass_get_device_by_ofnode(UCLASS_CLK, clk_node, &clk_dev);
	if (ret == 0 && clk_dev) {
		debug("QCE1204: Clock controller (UCLASS_CLK) already bound and probed\n");
		clock_controller_probed = true;
		return 0;
	}

	return 0;
}

static int qce1204_clk_get_from_node(struct phy_device *phydev, ofnode node,
				     struct clk *clk, const char *name)
{
	int ret;

	if (!ofnode_valid(node)) {
		debug("QCE1204: Invalid node for clock %s\n", name);
		return -EINVAL;
	}

	ret = clk_get_by_name_nodev(node, name, clk);
	if (ret < 0) {
		debug("QCE1204: Failed to get clock %s from node: %d\n", name, ret);
		return ret;
	}

	debug("QCE1204: Successfully got clock %s from PHY node\n", name);
	return 0;
}

static int qce1204_reset_get_from_node(struct phy_device *phydev, ofnode node,
				       struct reset_ctl *reset, const char *name)
{
	int ret, index;
	const char *reset_names;

	if (!ofnode_valid(node)) {
		debug("QCE1204: Invalid node for reset %s\n", name);
		return -EINVAL;
	}

	ret = ofnode_read_string_index(node, "reset-names", 0, &reset_names);
	if (ret < 0) {
		debug("QCE1204: No reset-names property found for reset %s\n", name);
		return ret;
	}

	index = 0;
	while (ret == 0) {
		if (strcmp(reset_names, name) == 0) {
			ret = reset_get_by_index_nodev(node, index, reset);
			if (ret < 0) {
				debug("QCE1204: Failed to get reset %s (index %d) from node: %d\n",
				      name, index, ret);
				return ret;
			}
			return 0;
		}
		index++;
		ret = ofnode_read_string_index(node, "reset-names", index, &reset_names);
	}

	return -ENODATA;
}

static int qce1204_clk_enable(struct phy_device *phydev, struct clk *clk, bool enable)
{
	int ret;

	if (!clk->dev)
		return 0;

	if (enable)
		ret = clk_enable(clk);
	else
		ret = clk_disable(clk);

	if (ret < 0) {
		debug("Failed to %s clock: %d\n",
		      enable ? "enable" : "disable", ret);
		return ret;
	}
	return 0;
}

static int qce1204_reset_assert(struct phy_device *phydev, struct reset_ctl *reset, bool assert)
{
	int ret;

	if (!reset->dev)
		return 0;

	if (assert)
		ret = reset_assert(reset);
	else
		ret = reset_deassert(reset);

	if (ret < 0) {
		dev_err(phydev->dev, "Failed to %s reset: %d\n",
			assert ? "assert" : "deassert", ret);
		return ret;
	}
	return 0;
}

static struct qce1204_clk_data *qce1204_get_clk_data(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;

	if (!priv)
		return NULL;

	return &priv->clk_data;
}

static struct qce1204_shared_clk_data *qce1204_get_shared_clk_data(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;

	if (!priv)
		return NULL;

	return priv->shared_clk_data;
}

static int qce1204_pcs_clk_set_rate(struct phy_device *phydev, u32 channel,
				    unsigned long gmii_clk_rate, unsigned long xgmii_clk_rate)
{
	struct qce1204_shared_clk_data *clk_data;
	struct qce1204_channel_clk *ch_clk;
	int ret;

	if (channel < 1 || channel > 4)
		return -EINVAL;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	ch_clk = &clk_data->channels[channel - 1];

	if (ch_clk->clks[QCE1204_CLK_GMII_TX].dev) {
		ret = clk_set_rate(&ch_clk->clks[QCE1204_CLK_GMII_TX], gmii_clk_rate);
		if (ret < 0) {
			dev_err(phydev->dev, "Failed to set GMII TX clock rate: %d\n", ret);
			return ret;
		}
	}

	if (ch_clk->clks[QCE1204_CLK_GMII_RX].dev) {
		ret = clk_set_rate(&ch_clk->clks[QCE1204_CLK_GMII_RX], gmii_clk_rate);
		if (ret < 0) {
			dev_err(phydev->dev, "Failed to set GMII RX clock rate: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int qce1204_pcs_clk_set(struct phy_device *phydev, u32 channel, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;
	struct qce1204_channel_clk *ch_clk;
	int ret;

	if (channel < 1 || channel > 4)
		return -EINVAL;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	ch_clk = &clk_data->channels[channel - 1];

	ret = qce1204_clk_enable(phydev, &ch_clk->clks[QCE1204_CLK_GMII_TX], enable);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &ch_clk->clks[QCE1204_CLK_GMII_RX], enable);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &ch_clk->clks[QCE1204_CLK_XGMII_TX], enable);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &ch_clk->clks[QCE1204_CLK_XGMII_RX], enable);
	if (ret < 0)
		return ret;

	return 0;
}

static int qce1204_pcs_clk_reset_assert(struct phy_device *phydev, u32 channel, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	struct qce1204_channel_clk *ch_clk;
	int ret;

	if (channel < 1 || channel > 4)
		return -EINVAL;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	ch_clk = &clk_data->channels[channel - 1];

	ret = qce1204_reset_assert(phydev, &ch_clk->resets[QCE1204_CLK_GMII_TX], assert);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &ch_clk->resets[QCE1204_CLK_GMII_RX], assert);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &ch_clk->resets[QCE1204_CLK_XGMII_TX], assert);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &ch_clk->resets[QCE1204_CLK_XGMII_RX], assert);
	if (ret < 0)
		return ret;

	return 0;
}

static int qce1204_pcs_clk_reset(struct phy_device *phydev, u32 channel)
{
	int ret;

	ret = qce1204_pcs_clk_reset_assert(phydev, channel, true);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qce1204_pcs_clk_reset_assert(phydev, channel, false);

	return ret;
}

static int __maybe_unused qce1204_phy_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return 0;

	ret = qce1204_clk_enable(phydev, &clk_data->tx_clk, enable);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &clk_data->rx_clk, enable);
	if (ret < 0)
		return ret;

	return 0;
}

static int qce1204_phy_clk_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return 0;

	ret = qce1204_reset_assert(phydev, &clk_data->tx_reset, assert);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &clk_data->rx_reset, assert);
	if (ret < 0)
		return ret;

	return 0;
}

static int qce1204_phy_clk_reset(struct phy_device *phydev)
{
	int ret;

	ret = qce1204_phy_clk_reset_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qce1204_phy_clk_reset_assert(phydev, false);

	return ret;
}

static int qce1204_set_srds_mux(struct phy_device *phydev)
{
	struct udevice *dev = phydev->dev;
	static const char * const srds_mux[] = {
				"srds0_rx_mux",
				"srds0_tx_mux",
				"srds0_xgmii_rx_mux",
				"srds0_xgmii_tx_mux",
				"srds1_rx_mux",
				"srds1_tx_mux",
				"srds1_xgmii_rx_mux",
				"srds1_xgmii_tx_mux",
				};
	struct clk clk;
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(srds_mux); i++) {
		const char *clk_name = srds_mux[i];

		ret = qce1204_clk_get_from_node(phydev, phydev->node, &clk, clk_name);
		if (ret < 0) {
			/*
			 * Clock not present in this DTS (e.g. srds0_* absent in
			 * switch-mode DTS which only has srds1_* mux clocks).
			 * Skip silently – the mux clock is optional per board.
			 */
			dev_dbg(dev, "Clock '%s' not found (err=%d), skipping\n",
				clk_name, ret);
			continue;
		}

		ret = clk_enable(&clk);
		if (ret) {
			dev_err(dev, "Failed to enable clk '%s' (err=%d)\n", clk_name, ret);
			return ret;
		}

		dev_dbg(dev, "Clock '%s' enabled\n", clk_name);
	}

	return 0;
}

static int qce1204_phy_clk_init(struct phy_device *phydev, struct qce1204_clk_data *clk_data)
{
	ofnode phy_node;
	int ret;

	phy_node = phydev->node;
	if (!ofnode_valid(phy_node))
		return 0;

	/* Use the correct PHY node for clock/reset lookup */
	debug("QCE1204: Initializing PHY clocks from correct device tree node\n");

	ret = qce1204_clk_get_from_node(phydev, phy_node, &clk_data->tx_clk, "tx_clk");
	if (ret < 0 && ret != -ENODATA)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node, &clk_data->rx_clk, "rx_clk");
	if (ret < 0 && ret != -ENODATA)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node, &clk_data->sys_clk, "sys_clk");
	if (ret < 0 && ret != -ENODATA)
		return ret;

	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->tx_reset, "tx_reset");
	if (ret < 0 && ret != -ENODATA)
		return ret;

	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->rx_reset, "rx_reset");
	if (ret < 0 && ret != -ENODATA)
		return ret;

	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->sys_reset, "sys_reset");
	if (ret < 0 && ret != -ENODATA)
		return ret;

	return 0;
}

static int qce1204_phy_shared_clk_init(struct phy_device *phydev,
				       struct qce1204_shared_clk_data *clk_data)
{
	ofnode phy_node;
	char name[32];
	int i, ret;
	struct qce1204_priv *priv = phydev->priv;

	if (phydev->addr != priv->base_phy_addr) {
		debug("QCE1204: Skipping shared clock init for non-master PHY (addr=%d)\n",
		      phydev->addr);
		return 0;
	}

	ret = qce1204_ensure_clock_controller_ready(phydev);
	if (ret < 0) {
		debug("QCE1204: Clock controller not ready, continuing without shared clocks: %d\n",
		      ret);
		return 0;
	}

	phy_node = phydev->node;
	if (!ofnode_valid(phy_node))
		return 0;

	/* Initialize Channel Clocks from PHY node */
	for (i = 0; i < 4; i++) {
		snprintf(name, sizeof(name), "ch%d_gmii_tx_clk", i);
		ret = qce1204_clk_get_from_node(phydev, phy_node,
						&clk_data->channels[i].clks[QCE1204_CLK_GMII_TX],
						name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;

		snprintf(name, sizeof(name), "ch%d_gmii_rx_clk", i);
		ret = qce1204_clk_get_from_node(phydev, phy_node,
						&clk_data->channels[i].clks[QCE1204_CLK_GMII_RX],
						name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;

		snprintf(name, sizeof(name), "ch%d_xgmii_tx_clk", i);
		ret = qce1204_clk_get_from_node(phydev, phy_node,
						&clk_data->channels[i].clks[QCE1204_CLK_XGMII_TX],
						name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;

		snprintf(name, sizeof(name), "ch%d_xgmii_rx_clk", i);
		ret = qce1204_clk_get_from_node(phydev, phy_node,
						&clk_data->channels[i].clks[QCE1204_CLK_XGMII_RX],
						name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;
	}

	/* Initialize PCS SYS and AHB Clocks from PHY node */
	ret = qce1204_clk_get_from_node(phydev, phy_node, &clk_data->pcs_sys_clk, "pcs_sys_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node, &clk_data->ahb_clk, "ahb_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/* Initialize Channel Resets from PHY node */
	for (i = 0; i < 4; i++) {
		snprintf(name, sizeof(name), "ch%d_gmii_tx_reset", i);
		ret = qce1204_reset_get_from_node(phydev, phy_node,
						  &clk_data->channels[i].resets
						  [QCE1204_CLK_GMII_TX],
						  name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;

		snprintf(name, sizeof(name), "ch%d_gmii_rx_reset", i);
		ret = qce1204_reset_get_from_node(phydev, phy_node,
						  &clk_data->channels[i].resets
						  [QCE1204_CLK_GMII_RX],
						  name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;

		snprintf(name, sizeof(name), "ch%d_xgmii_tx_reset", i);
		ret = qce1204_reset_get_from_node(phydev, phy_node,
						  &clk_data->channels[i].resets
						  [QCE1204_CLK_XGMII_TX],
						  name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;

		snprintf(name, sizeof(name), "ch%d_xgmii_rx_reset", i);
		ret = qce1204_reset_get_from_node(phydev, phy_node,
						  &clk_data->channels[i].resets
						  [QCE1204_CLK_XGMII_RX],
						  name);
		if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
			return ret;
	}

	/* Initialize PCS/XPCS Resets from PHY node */
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->pcs_sys_reset,
					  "pcs_sys_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->xpcs_reset, "xpcs");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/* Initialize Switch Resets from PHY node */
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_core_reset,
					  "switch_core_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_btq_reset,
					  "switch_btq_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_cfg_reset,
					  "switch_cfg_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_ipe_reset,
					  "switch_ipe_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_mac0_reset,
					  "switch_mac0_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_mac1_reset,
					  "switch_mac1_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_mac2_reset,
					  "switch_mac2_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_mac3_reset,
					  "switch_mac3_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_mac4_reset,
					  "switch_mac4_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->switch_mac5_reset,
					  "switch_mac5_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->xgmac0_ptp_ref_reset,
					  "xgmac0_ptp_ref_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->xgmac1_ptp_ref_reset,
					  "xgmac1_ptp_ref_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac0_tx_reset,
					  "mac0_tx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac0_rx_reset,
					  "mac0_rx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac1_tx_reset,
					  "mac1_tx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac1_rx_reset,
					  "mac1_rx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac2_tx_reset,
					  "mac2_tx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac2_rx_reset,
					  "mac2_rx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac3_tx_reset,
					  "mac3_tx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac3_rx_reset,
					  "mac3_rx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac4_tx_reset,
					  "mac4_tx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac4_rx_reset,
					  "mac4_rx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac5_tx_reset,
					  "mac5_tx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node, &clk_data->mac5_rx_reset,
					  "mac5_rx_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/* SRDS0 (PCS0) clocks and resets for port 5 USXGMII — optional */
	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->srds0_sys_clk, "srds0_sys_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node,
					  &clk_data->srds0_sys_reset, "srds0_sys_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;
	ret = qce1204_reset_get_from_node(phydev, phy_node,
					  &clk_data->srds0_xpcs_reset, "srds0_xpcs_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/*
	 * Switch-level gate clocks (switch mode only).
	 * These are optional - absent in PHY mode DTS, present in switch mode DTS.
	 */
	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->switch_core_clk, "switch_core_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->switch_ipe_clk, "switch_ipe_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->switch_btq_clk, "switch_btq_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->switch_cfg_clk, "switch_cfg_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->switch_apb_clk, "switch_apb_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/*
	 * MAC TX/RX gate clocks (switch mode only).
	 * MAC0 = CPU port; MAC1-4 = user ports.
	 * These are the final per-MAC clock enables, separate from the
	 * SRDS1 channel clocks (ch0-ch3) which configure the RCG.
	 */
	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac0_tx_clk, "mac0_tx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac0_rx_clk, "mac0_rx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac1_tx_clk, "mac1_tx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac1_rx_clk, "mac1_rx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac2_tx_clk, "mac2_tx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac2_rx_clk, "mac2_rx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac3_tx_clk, "mac3_tx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac3_rx_clk, "mac3_rx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac4_tx_clk, "mac4_tx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac4_rx_clk, "mac4_rx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/* MAC5 gate clocks for port 5 USXGMII (SRDS0) */
	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac5_tx_clk, "mac5_tx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac5_rx_clk, "mac5_rx_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac5_tx_xgmii_clk, "mac5_tx_xgmii_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_clk_get_from_node(phydev, phy_node,
					&clk_data->mac5_rx_xgmii_clk, "mac5_rx_xgmii_clk");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/* MAC5 SRDS0 channel resets (PCS_FUNC_TX/RX in Linux pcs-qce2204) */
	ret = qce1204_reset_get_from_node(phydev, phy_node,
					  &clk_data->mac5_tx_srds0_reset,
					  "mac5_tx_srds0_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_reset_get_from_node(phydev, phy_node,
					  &clk_data->mac5_rx_srds0_reset,
					  "mac5_rx_srds0_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	/* MAC5 XGMII resets for port 5 USXGMII (XPCS_FUNC_XGMII_TX/RX in Linux) */
	ret = qce1204_reset_get_from_node(phydev, phy_node,
					  &clk_data->mac5_tx_xgmii_reset,
					  "mac5_tx_xgmii_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	ret = qce1204_reset_get_from_node(phydev, phy_node,
					  &clk_data->mac5_rx_xgmii_reset,
					  "mac5_rx_xgmii_reset");
	if (ret < 0 && ret != -ENODATA && ret != -ENODEV)
		return ret;

	return 0;
}

static int qce1204_pcs_speed_clock_set(struct phy_device *phydev, u32 channel, u32 speed);

#ifdef CONFIG_PHY_QCE_2204
static int qce1204_switch_clks_enable(struct phy_device *phydev)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	ret = qce1204_clk_enable(phydev, &clk_data->switch_core_clk, true);
	if (ret < 0)
		return ret;

	ret = clk_set_rate(&clk_data->switch_core_clk, 250000000);
	if (ret < 0) {
		dev_warn(phydev->dev,
			 "QCE1204: Failed to set switch core clock to 250MHz: %d\n", ret);
	}

	ret = qce1204_clk_enable(phydev, &clk_data->switch_ipe_clk, true);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &clk_data->switch_btq_clk, true);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &clk_data->switch_cfg_clk, true);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &clk_data->switch_apb_clk, true);
	if (ret < 0)
		return ret;

	ret = clk_set_rate(&clk_data->mac0_tx_clk, 250000000);

	ret = qce1204_clk_enable(phydev, &clk_data->mac0_tx_clk, true);
	if (ret < 0)
		return ret;

	ret = clk_set_rate(&clk_data->mac0_rx_clk, 250000000);

	ret = qce1204_clk_enable(phydev, &clk_data->mac0_rx_clk, true);
	if (ret < 0)
		return ret;

	/* Enable MAC1-4 TX/RX gate clocks */
	ret = qce1204_clk_enable(phydev, &clk_data->mac1_tx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac1_rx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac2_tx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac2_rx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac3_tx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac3_rx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac4_tx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac4_rx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac5_tx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac5_rx_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac5_tx_xgmii_clk, true);
	if (ret < 0)
		return ret;
	ret = qce1204_clk_enable(phydev, &clk_data->mac5_rx_xgmii_clk, true);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &clk_data->switch_core_reset, true);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qce1204_reset_assert(phydev, &clk_data->switch_core_reset, false);
	if (ret < 0)
		return ret;
	mdelay(10);

	dev_dbg(phydev->dev, "QCE1204: Switch mode clocks enabled for all 4 ports\n");
	return 0;
}
#endif

static int qce1204_xpcs_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	return qce1204_reset_assert(phydev, &clk_data->xpcs_reset, assert);
}

static int qce1204_ahb_clk_set_rate(struct phy_device *phydev, unsigned long rate)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data || !clk_data->ahb_clk.dev)
		return 0;

	ret = clk_set_rate(&clk_data->ahb_clk, rate);
	if (ret < 0) {
		dev_err(phydev->dev, "Failed to set AHB clock rate: %d\n", ret);
		return ret;
	}
	return 0;
}

static int qce1204_pcs_sys_clk_set_rate(struct phy_device *phydev, unsigned long rate)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data || !clk_data->pcs_sys_clk.dev)
		return 0;

	ret = clk_set_rate(&clk_data->pcs_sys_clk, rate);
	if (ret < 0) {
		debug("Failed to set PCS clock rate: %d\n", ret);
		return ret;
	}
	return 0;
}

static int qce1204_pcs_sys_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	return qce1204_clk_enable(phydev, &clk_data->pcs_sys_clk, enable);
}

static int qce1204_pcs_sys_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	return qce1204_reset_assert(phydev, &clk_data->pcs_sys_reset, assert);
}

static int qce1204_pcs_sys_reset(struct phy_device *phydev)
{
	int ret;

	ret = qce1204_pcs_sys_reset_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(10);
	ret = qce1204_pcs_sys_reset_assert(phydev, false);

	return ret;
}

static int qce1204_phy_sys_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_clk_data *clk_data;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return 0;

	return qce1204_clk_enable(phydev, &clk_data->sys_clk, enable);
}

static int qce1204_phy_sys_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_clk_data *clk_data;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return 0;

	return qce1204_reset_assert(phydev, &clk_data->sys_reset, assert);
}

static int qce1204_phy_sys_reset(struct phy_device *phydev)
{
	int ret;

	ret = qce1204_phy_sys_reset_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(10);
	ret = qce1204_phy_sys_reset_assert(phydev, false);

	return ret;
}

#ifdef CONFIG_PHY_QCE_2204
static int qce1204_switch_clks_reset_deassert(struct phy_device *phydev)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	/* Deassert all switch resets */
	ret = qce1204_reset_assert(phydev, &clk_data->switch_btq_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_cfg_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_core_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_ipe_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac0_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac1_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac2_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac3_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac4_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac5_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->xgmac0_ptp_ref_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->xgmac1_ptp_ref_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac0_tx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac0_rx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac1_tx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac1_rx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac2_tx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac2_rx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac3_tx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac3_rx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac4_tx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac4_rx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac5_tx_reset, false);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac5_rx_reset, false);
	if (ret)
		return ret;

	return 0;
}
#endif

static int qce1204_switch_clks_reset_assert(struct phy_device *phydev)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return 0;

	/* Assert all switch resets */
	ret = qce1204_reset_assert(phydev, &clk_data->switch_btq_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_cfg_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_core_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_ipe_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac0_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac1_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac2_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac3_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac4_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->switch_mac5_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->xgmac0_ptp_ref_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->xgmac1_ptp_ref_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac0_tx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac0_rx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac1_tx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac1_rx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac2_tx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac2_rx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac3_tx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac3_rx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac4_tx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac4_rx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac5_tx_reset, true);
	if (ret)
		return ret;
	ret = qce1204_reset_assert(phydev, &clk_data->mac5_rx_reset, true);
	if (ret)
		return ret;

	return 0;
}

#ifdef CONFIG_PHY_QCE_2204
static int qce1204_switch_ports_parse(struct phy_device *phydev,
				      struct qce1204_priv *priv)
{
	ofnode switch_node, ports_node, port_node;
	u32 phy_addr, port_id;
	int ret;

	memset(priv->sw_ports, 0, sizeof(priv->sw_ports));
	priv->num_sw_ports = 0;

	switch_node = phydev->node;
	if (!ofnode_valid(switch_node)) {
		debug("QCE1204: Switch node invalid, skipping port parse\n");
		return 0;
	}

	/* Find the 'ports' sub-node of the switch master node */
	ports_node = ofnode_find_subnode(switch_node, "ports");
	if (!ofnode_valid(ports_node)) {
		debug("QCE1204: No 'ports' sub-node in switch node\n");
		return 0;
	}

	ofnode_for_each_subnode(port_node, ports_node) {
		ret = ofnode_read_u32(port_node, "port-id", &port_id);
		if (ret) {
			debug("QCE1204: Port node missing 'port-id', skipping\n");
			continue;
		}

		/* Only handle user ports 1-4 */
		if (port_id < 1 || port_id > QCE1204_MAX_SWITCH_PORTS) {
			debug("QCE1204: Skipping port-id=%d (out of range 1-%d)\n",
			      port_id, QCE1204_MAX_SWITCH_PORTS);
			continue;
		}

		/*
		 * 'phy_addr' is the GEPHY MDIO address (0-based).
		 * phy_addr=0 is valid — do NOT skip it.
		 */
		ret = ofnode_read_u32(port_node, "phy_addr", &phy_addr);
		if (ret) {
			debug("QCE1204: Port %d missing 'phy_addr', skipping\n",
			      port_id);
			continue;
		}

		priv->sw_ports[port_id - 1].port_id     = (int)port_id;
		priv->sw_ports[port_id - 1].phy_addr    = (int)phy_addr;
		priv->sw_ports[port_id - 1].valid       = true;
		priv->sw_ports[port_id - 1].is_external =
			ofnode_read_bool(port_node, "is_external");

		ret = qce1204_clk_get_from_node(phydev, port_node,
						&priv->sw_ports[port_id - 1].tx_clk,
						"tx_clk");
		if (ret < 0 && ret != -ENODATA && ret != -EINVAL)
			debug("QCE1204: Port %d tx_clk not found (%d)\n",
			      port_id, ret);

		ret = qce1204_clk_get_from_node(phydev, port_node,
						&priv->sw_ports[port_id - 1].rx_clk,
						"rx_clk");
		if (ret < 0 && ret != -ENODATA && ret != -EINVAL)
			debug("QCE1204: Port %d rx_clk not found (%d)\n",
			      port_id, ret);

		ret = qce1204_reset_get_from_node(phydev, port_node,
						  &priv->sw_ports[port_id - 1].tx_reset,
						  "tx_reset");
		if (ret < 0 && ret != -ENODATA && ret != -EINVAL)
			debug("QCE1204: Port %d tx_reset not found (%d)\n",
			      port_id, ret);

		ret = qce1204_reset_get_from_node(phydev, port_node,
						  &priv->sw_ports[port_id - 1].rx_reset,
						  "rx_reset");
		if (ret < 0 && ret != -ENODATA && ret != -EINVAL)
			debug("QCE1204: Port %d rx_reset not found (%d)\n",
			      port_id, ret);

		priv->num_sw_ports++;
		debug("QCE1204: Switch port %d: GEPHY MDIO addr=%d\n",
		      port_id, phy_addr);
	}

	debug("QCE1204: Parsed %d switch user ports\n", priv->num_sw_ports);
	return 0;
}

static int qce1204_switch_port_read_link(struct phy_device *phydev,
					 int phy_addr,
					 int *link, int *speed, int *duplex)
{
	struct phy_device local_phydev;
	u16 phy_data, speed_bits;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;

	phy_data = phy_read(&local_phydev, MDIO_MMD_VEND2,
			    QCE1204_PHY_SPEC_STATUS);

	*link = (phy_data & QCE1204_PHY_SS_LINK_STATUS) ? 1 : 0;
	speed_bits = phy_data & QCE1204_PHY_SS_SPEED_MASK;

	switch (speed_bits) {
	case QCE1204_PHY_SS_SPEED_2500:
		*speed = SPEED_2500;
		break;
	case QCE1204_PHY_SS_SPEED_1000:
		*speed = SPEED_1000;
		break;
	case QCE1204_PHY_SS_SPEED_100:
		*speed = SPEED_100;
		break;
	case QCE1204_PHY_SS_SPEED_10:
		*speed = SPEED_10;
		break;
	default:
		*speed = SPEED_UNKNOWN;
		break;
	}

	*duplex = (phy_data & QCE1204_PHY_SS_DUPLEX_FULL) ?
		  DUPLEX_FULL : DUPLEX_HALF;

	debug("QCE1204: Port PHY@%d: link=%d speed=%d duplex=%d\n",
	      phy_addr, *link, *speed, *duplex);
	return 0;
}

static int qce1204_switch_port_clk_set(struct phy_device *phydev,
				       struct qce1204_switch_port *port,
				       bool enable)
{
	int ret;

	ret = qce1204_clk_enable(phydev, &port->tx_clk, enable);
	if (ret < 0)
		return ret;

	ret = qce1204_clk_enable(phydev, &port->rx_clk, enable);
	if (ret < 0)
		return ret;

	if (phydev->drv->uid == IPQ52XX_PHY_ID) {
		ret = qce1204_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL3,
			QCE1204_DEBUG_ANA_10M_DAC_CTRL3_VAL);
	}

	return ret;
}

static int qce1204_switch_port_clk_reset(struct phy_device *phydev,
					 struct qce1204_switch_port *port)
{
	int ret;

	ret = qce1204_reset_assert(phydev, &port->tx_reset, true);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &port->rx_reset, true);
	if (ret < 0)
		return ret;

	mdelay(1);

	ret = qce1204_reset_assert(phydev, &port->tx_reset, false);
	if (ret < 0)
		return ret;

	ret = qce1204_reset_assert(phydev, &port->rx_reset, false);
	if (ret < 0)
		return ret;

	return 0;
}

static int qce1204_switch_port_speed_fixup(struct phy_device *phydev,
					   struct qce1204_switch_port *port,
					   int speed, int link)
{
	struct phy_device local_phydev;
	u32 channel = (u32)port->port_id;
	bool clk_en = (link != 0);
	int ret;

	/*
	 * Port 5 (external QCA81xx PHY via USXGMII/PCS0):
	 *   DTS clocks: tx_clk = MAC5_TX_SRDS0_CLK (0x224)
	 *               rx_clk = MAC5_RX_SRDS0_CLK (0x244)
	 *               sys_clk = SRDS0_SYS_CLK (0x280, already enabled at probe)
	 *
	 * The QUSGMII PCS1 channel clocks (channels 1-4) are NOT used.
	 * Instead: set rate on MAC5 SRDS0 TX/RX clocks, enable them, then
	 * reset them.  The SRDS0 SerDes was already initialised by
	 * qce1204_pcs0_usxgmii_mode_set() during probe.
	 */
	if (port->is_external) {
		unsigned long clk_rate, uxsgmii_speed_cfg = 0;

		/* Determine MAC5 SRDS0 clock rate from link speed */
		switch (speed) {
		case SPEED_10000:
			uxsgmii_speed_cfg = (BIT(13) | BIT(6));
			clk_rate = QCE1204_CLK_RATE_312P5M;
			break;
		case SPEED_5000:
			uxsgmii_speed_cfg = (BIT(13) | BIT(5));
			clk_rate = QCE1204_CLK_RATE_156P25M;
			break;
		case SPEED_2500:
			uxsgmii_speed_cfg = BIT(5);
			clk_rate = QCE1204_CLK_RATE_78P125M;
			break;
		case SPEED_1000:
			uxsgmii_speed_cfg = BIT(6);
			clk_rate = QCE1204_CLK_RATE_125M;
			break;
		case SPEED_100:
			uxsgmii_speed_cfg = BIT(13);
			clk_rate = QCE1204_CLK_RATE_12P5M;
			break;
		case SPEED_10:
			uxsgmii_speed_cfg = 0;
			clk_rate = QCE1204_CLK_RATE_2P5M;
			break;
		default:
			clk_rate = QCE1204_CLK_RATE_125M;
			break;
		}

		/* Set MAC5 TX/RX SRDS0 clock rates when link is up */
		if (link) {
			if (port->tx_clk.dev) {
				ret = clk_set_rate(&port->tx_clk, clk_rate);
				if (ret < 0)
					debug("QCE1204: Port %d: MAC5 TX SRDS0 set_rate failed: %d\n",
					      port->port_id, ret);
			}
			if (port->rx_clk.dev) {
				ret = clk_set_rate(&port->rx_clk, clk_rate);
				if (ret < 0)
					debug("QCE1204: Port %d: MAC5 RX SRDS0 set_rate failed: %d\n",
					      port->port_id, ret);
			}
			mdelay(10);
		}

		/*
		 * Enable/disable MAC5 parent gate clocks and XGMII gate clocks.
		 * Clock hierarchy:
		 *   MAC5_TX_CLK (0x220) → MAC5_TX_SRDS0_CLK (0x224) → port->tx_clk
		 *   MAC5_TX_SRDS0_CH0_XGMII_CLK (0x228) — XGMII path
		 *   MAC5_RX_CLK (0x23c) → MAC5_RX_SRDS0_CLK (0x244) → port->rx_clk
		 *   MAC5_RX_SRDS0_CH0_XGMII_CLK (0x248) — XGMII path
		 * All parent gates must be enabled before the child gates.
		 */
		{
			struct qce1204_shared_clk_data *sclk =
				qce1204_get_shared_clk_data(phydev);

			if (sclk) {
				qce1204_clk_enable(phydev, &sclk->mac5_tx_clk, clk_en);
				qce1204_clk_enable(phydev, &sclk->mac5_tx_xgmii_clk, clk_en);
				qce1204_clk_enable(phydev, &sclk->mac5_rx_clk, clk_en);
				qce1204_clk_enable(phydev, &sclk->mac5_rx_xgmii_clk, clk_en);
			}
		}

		/* Enable or disable MAC5 TX/RX SRDS0 clocks */
		ret = qce1204_switch_port_clk_set(phydev, port, clk_en);
		if (ret < 0)
			return ret;

		mdelay(10);

		/* Reset MAC5 TX/RX SRDS0 clocks */
		ret = qce1204_switch_port_clk_reset(phydev, port);
		if (ret < 0)
			return ret;

		{
			struct qce1204_shared_clk_data *sclk2 =
				qce1204_get_shared_clk_data(phydev);

			if (sclk2) {
				qce1204_reset_assert(phydev,
						     &sclk2->mac5_tx_xgmii_reset, true);
				qce1204_reset_assert(phydev,
						     &sclk2->mac5_rx_xgmii_reset, true);
				mdelay(1);
				qce1204_reset_assert(phydev,
						     &sclk2->mac5_tx_xgmii_reset, false);
				qce1204_reset_assert(phydev,
						     &sclk2->mac5_rx_xgmii_reset, false);
			}
		}

		debug("QCE1204: Port %d (external): MAC5 SRDS0 clk fixup done (speed=%d link=%d rate=%lu)\n",
		      port->port_id, speed, link, clk_rate);

		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_VEND2,
					      QCE1204_PCS_MMD_MII_CTRL,
					      (BIT(13) | BIT(6) | BIT(5) | BIT(8)),
					      (uxsgmii_speed_cfg | BIT(8)));
		if (ret < 0)
			return ret;
		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
					      QCE1204_PCS_MMD3_DIG_CTRL1,
					      BIT(10), BIT(10));
		if (ret < 0)
			return ret;

		/* Enable or disable PCS channel clocks (GMII + XGMII TX/RX) */
		ret = qce1204_pcs_clk_set(phydev, 1, clk_en);
		if (ret < 0)
			return ret;

		/* Reset PCS channel clocks (assert then deassert) */
		ret = qce1204_pcs_clk_reset(phydev, 1);
		if (ret < 0)
			return ret;

		return 0;
	}

	/* Set PCS clock rate for this channel when link is up */
	if (link) {
		ret = qce1204_pcs_speed_clock_set(phydev, channel, speed);
		if (ret < 0) {
			debug("QCE1204: Port %d: failed to set PCS speed clocks: %d\n",
			      port->port_id, ret);
			return ret;
		}
		mdelay(10);
	}

	/* Enable or disable PCS channel clocks (GMII + XGMII TX/RX) */
	ret = qce1204_pcs_clk_set(phydev, 1, clk_en);
	if (ret < 0)
		return ret;

	/*
	 * Enable or disable per-port MAC TX/RX clocks (from port DT node).
	 * Mirrors qce1204_phy_clk_set() in PHY mode.
	 */
	ret = qce1204_switch_port_clk_set(phydev, port, clk_en);
	if (ret < 0)
		return ret;

	mdelay(100);

	/* Reset PCS channel clocks (assert then deassert) */
	ret = qce1204_pcs_clk_reset(phydev, 1);
	if (ret < 0)
		return ret;

	/*
	 * Reset per-port MAC TX/RX clocks (from port DT node).
	 * Mirrors qce1204_phy_clk_reset() in PHY mode.
	 */
	ret = qce1204_switch_port_clk_reset(phydev, port);
	if (ret < 0)
		return ret;

	/*
	 * FIFO reset on the port's internal GEPHY.
	 * Create a local phydev copy with the port's MDIO address so the
	 * FIFO reset register write targets the correct PHY, not the switch
	 * master node at addr 0.
	 */
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = port->phy_addr;

	ret = qce1204_phy_fifo_reset(&local_phydev, true);
	if (ret < 0)
		return ret;

	mdelay(1);

	if (link) {
		ret = qce1204_phy_fifo_reset(&local_phydev, false);
		if (ret < 0)
			return ret;
	}

	debug("QCE1204: Port %d (PHY@%d): speed fix-up done (ch=%d speed=%d link=%d)\n",
	      port->port_id, port->phy_addr, channel, speed, link);
	return 0;
}

static int qce1204_switch_configure_ports(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;
	int i, link, speed, duplex, ret;
	int any_link_up = 0;

	if (!priv || priv->package_mode != PHY_INTERFACE_MODE_INTERNAL)
		return 1;

	printf("QCE1204-switch status:\n");

	for (i = 0; i < QCE1204_MAX_SWITCH_PORTS; i++) {
		struct qce1204_switch_port *port = &priv->sw_ports[i];

		if (!port->valid)
			continue;

		/*
		 * For external PHY ports (QCA81xx on port 5): use the standard
		 * PHY framework (phy_startup) if the PHY was successfully
		 * connected via phy_connect().  Fall back to direct register
		 * read if phy_connect() was not called or failed.
		 */
		if (port->is_external && port->ext_phydev) {
			ret = phy_startup(port->ext_phydev);
			if (ret < 0) {
				debug("QCE1204: Port %d: phy_startup failed: %d\n",
				      port->port_id, ret);
				continue;
			}
			link   = port->ext_phydev->link;
			speed  = port->ext_phydev->speed;
			duplex = port->ext_phydev->duplex;
		} else {
			ret = qce1204_switch_port_read_link(phydev, port->phy_addr,
							    &link, &speed, &duplex);
			if (ret < 0) {
				debug("QCE1204: Port %d: failed to read link: %d\n",
				      port->port_id, ret);
				continue;
			}
		}

		printf("PORT%d %s Speed :%d %s duplex\n",
		       port->port_id,
		       link ? "Up" : "Down",
		       speed,
		       duplex == DUPLEX_FULL ? "Full" : "Half");

		if (!link) {
			debug("QCE1204: Port %d (PHY@%d): link down, skipping\n",
			      port->port_id, port->phy_addr);
			if (port->last_link) {
				port->last_link  = 0;
				port->configured = false;
			}
			continue;
		}

		any_link_up = 1;

		if (port->configured &&
		    port->last_link   == link  &&
		    port->last_speed  == speed &&
		    port->last_duplex == duplex) {
			debug("QCE1204: Port %d: state unchanged (link=%d speed=%d duplex=%d), skipping\n",
			      port->port_id, link, speed, duplex);
			continue;
		}

		ret = qce1204_switch_port_speed_fixup(phydev, port, speed, link);
		if (ret < 0) {
			debug("QCE1204: Port %d: speed fix-up failed: %d\n",
			      port->port_id, ret);
		}

		ret = qce2204_port_link_up(phydev, port->port_id,
					   speed, duplex,
					   port->is_external ? PHY_INTERFACE_MODE_USXGMII
							     : phydev->interface,
					   true, true);
		if (ret < 0) {
			debug("QCE1204: Port %d: qce2204_port_link_up failed: %d\n",
			      port->port_id, ret);
		}

		port->last_link   = link;
		port->last_speed  = speed;
		port->last_duplex = duplex;
		port->configured  = true;
	}

	return any_link_up ? 0 : 1;
}
#endif

static int qce1204_phy_package_mode_probe(struct phy_device *phydev,
					  struct qce1204_priv *priv)
{
	ofnode phy_node, mdio_node;
	const char *mode_str;
	u8 base_addr;
	int ret;

	phy_node = phydev->node;
	if (!ofnode_valid(phy_node)) {
		priv->package_mode = PHY_INTERFACE_MODE_NA;
		return 0;
	}

	mode_str = ofnode_read_string(phy_node, "qcom,package-mode");
	if (!mode_str) {
		mdio_node = ofnode_get_parent(phy_node);
		if (ofnode_valid(mdio_node))
			mode_str = ofnode_read_string(mdio_node, "qcom,package-mode");
	}

	if (!mode_str) {
		priv->package_mode = PHY_INTERFACE_MODE_NA;
		return 0;
	}

	if (!strcmp(mode_str, "qusgmii"))
		priv->package_mode = PHY_INTERFACE_MODE_QUSGMII;
	else if (!strcmp(mode_str, "internal"))
		priv->package_mode = PHY_INTERFACE_MODE_INTERNAL;
	else
		priv->package_mode = PHY_INTERFACE_MODE_NA;

	ret = ofnode_read_u8(phy_node, "base_addr", &base_addr);
	if (ret)
		return ret;
	priv->base_phy_addr = base_addr;

#ifdef CONFIG_PHY_QCE_2204
	if (priv->package_mode == PHY_INTERFACE_MODE_INTERNAL) {
		ret = qce1204_switch_ports_parse(phydev, priv);
		if (ret < 0)
			return ret;
	}
#endif

	return 0;
}

static int qce1204_phy_tlmm_init(struct phy_device *phydev)
{
	int ret, i;

	/* GPIO0, FUNC 1 */
	ret = qce1204_soc_modify(phydev, TO_TLMM_CFG_REG(QCE1204_GPIO0_PHY_INT),
				 QCE1204_TLMM_FUNC_MASK, BIT(2));
	if (ret < 0)
		return ret;

	/* GPIO1~GPIO4, FUNC 1, LED_MODE, DRV_16_MA, NO_PULL */
	for (i = QCE1204_GPIO1_P0_LED_0; i <= QCE1204_GPIO4_P3_LED_0; i++) {
		ret = qce1204_soc_modify(phydev, TO_TLMM_CFG_REG(i),
					 QCE1204_TLMM_GPIO_PULL | QCE1204_TLMM_FUNC_MASK |
					 QCE1204_TLMM_DRV | QCE1204_TLMM_LED_MODE,
					 BIT(2) | QCE1204_TLMM_DRV_16_MA | QCE1204_TLMM_LED_MODE);
		if (ret < 0)
			return ret;
	}

	/* GPIO9, FUNC 4 */
	ret = qce1204_soc_modify(phydev, TO_TLMM_CFG_REG(QCE1204_GPIO9_P0_WOL_INT),
				 QCE1204_TLMM_FUNC_MASK, BIT(4));
	if (ret < 0)
		return ret;

	/* GPIO15~GPIO17, FUNC 1, LED_MODE, DRV_16_MA, NO_PULL */
	for (i = QCE1204_GPIO15_P1_WOL_INT; i <= QCE1204_GPIO17_P3_WOL_INT; i++) {
		ret = qce1204_soc_modify(phydev, TO_TLMM_CFG_REG(i),
					 QCE1204_TLMM_FUNC_MASK, BIT(3));
		if (ret < 0)
			return ret;
	}

	return 0;
}

static bool qce1204_phy_reg_valid(struct phy_device *phydev, unsigned int reg)
{
	if (reg > 0xFFFF) {
		debug("QCE1204: Invalid debug register address: 0x%x\n", reg);
		return false;
	}
	return true;
}

static int qce1204_phy_debug_write(struct phy_device *phydev, unsigned int reg, u16 val)
{
	int ret;

	if (!qce1204_phy_reg_valid(phydev, reg))
		return -EINVAL;

	debug("QCE1204: Debug write: reg=0x%x, val=0x%x\n", reg, val);

	ret = phy_write(phydev, MDIO_MMD_VEND2, QCE1204_DEBUG_ADDR, reg);
	if (ret < 0) {
		debug("QCE1204: Failed to write debug address 0x%x: %d\n", reg, ret);
		return ret;
	}

	ret = phy_write(phydev, MDIO_MMD_VEND2, QCE1204_DEBUG_DATA, val);
	if (ret < 0) {
		debug("QCE1204: Failed to write debug data 0x%x to reg 0x%x: %d\n",
		      val, reg, ret);
		return ret;
	}

	return 0;
}

static int qce1204_phy_debug_read(struct phy_device *phydev, unsigned int reg)
{
	int ret;

	if (!qce1204_phy_reg_valid(phydev, reg))
		return -EINVAL;

	ret = phy_write(phydev, MDIO_MMD_VEND2, QCE1204_DEBUG_ADDR, reg);
	if (ret < 0) {
		debug("QCE1204: Failed to write debug address 0x%x: %d\n", reg, ret);
		return ret;
	}

	ret = phy_read(phydev, MDIO_MMD_VEND2, QCE1204_DEBUG_DATA);
	if (ret < 0) {
		debug("QCE1204: Failed to read debug data from reg 0x%x: %d\n", reg, ret);
		return ret;
	}

	return ret;
}

static int qce1204_phy_debug_modify(struct phy_device *phydev, unsigned int reg,
				    u16 mask, u16 set)
{
	int val;

	val = qce1204_phy_debug_read(phydev, reg);
	if (val < 0)
		return val;

	val = (val & ~mask) | set;

	return qce1204_phy_debug_write(phydev, reg, val);
}

static int qce1204_phy_10m_dac_init(struct phy_device *phydev)
{
	int ret;

	/* adjust the voltage amplitude for 10BASE-T */
	ret = qce1204_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL0,
				      QCE1204_DEBUG_ANA_10M_DAC_CTRL0_VAL);
	if (ret < 0)
		return ret;

	ret = qce1204_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL1,
				      QCE1204_DEBUG_ANA_10M_DAC_CTRL1_VAL);
	if (ret < 0)
		return ret;

	ret = qce1204_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL2,
				      QCE1204_DEBUG_ANA_10M_DAC_CTRL2_VAL);
	if (ret < 0)
		return ret;

	if (phydev->drv->uid == IPQ52XX_PHY_ID) {
		ret = qce1204_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL3,
			QCE1204_DEBUG_ANA_10M_DAC_CTRL3_VAL);
	}
	return ret;
}

static int qce1204_phy_eee_init(struct phy_device *phydev)
{
	int ret;

	/* reduce the delay to response the quiet signal for 2.5G EEE */
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			    QCE1024_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL0,
			    QCE1024_PHY_2P5G_EEE_TX_QUIET_TIME0);
	if (ret < 0)
		return ret;

	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			    QCE1024_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL1,
			    QCE1024_PHY_2P5G_EEE_TX_QUIET_TIME1);
	if (ret < 0)
		return ret;

	/* reduce the delay to response the wake up signal for 2.5G EEE */
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
			    QCE1024_PHY_2P5G_EEE_TX_LPI_WAKE_CTRL,
			    QCE1024_PHY_2P5G_EEE_TX_WAKE_TIME);
	return ret;
}

static int qce1204_phy_cdt_thresh_init(struct phy_device *phydev)
{
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL2,
		      QCE1204_MMD3_CDT_THRESH_CTRL2_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL3,
		      QCE1204_MMD3_CDT_THRESH_CTRL3_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL4,
		      QCE1204_MMD3_CDT_THRESH_CTRL4_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL5,
		      QCE1204_MMD3_CDT_THRESH_CTRL5_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL6,
		      QCE1204_MMD3_CDT_THRESH_CTRL6_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL7,
		      QCE1204_MMD3_CDT_THRESH_CTRL7_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL9,
		      QCE1204_MMD3_CDT_THRESH_CTRL9_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL13,
		      QCE1204_MMD3_CDT_THRESH_CTRL13_VAL);
	phy_write_mmd(phydev, MDIO_MMD_PCS,
		      QCE1204_MMD3_CDT_THRESH_CTRL14,
		      QCE1204_MMD3_CDT_THRESH_CTRL14_VAL);

	return 0;
}

static int qce1204_pcs_calibration(struct phy_device *phydev)
{
	u16 pcs_data = 0;
	u32 retries = 100;
	u32 calibration_done = 0;

	/* Poll for calibration done */
	while (calibration_done != QCE1204_PCS_MMD1_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0) {
			debug("QCE1204: PCS calibration timeout after 100ms!\n");
			return -ETIMEDOUT;
		}

		pcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PMAPMD,
						QCE1204_PCS_MMD1_CALIBRATION4);
		calibration_done = (pcs_data & QCE1204_PCS_MMD1_CALIBRATION_DONE);
	}

	debug("QCE1204: PCS calibration completed successfully (reg=0x%x)\n", pcs_data);
	return 0;
}

static int qce1204_pcs_assert(struct phy_device *phydev, bool assert)
{
	int ret;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_PLL_POWER_ON_AND_RESET,
				     QCE1204_PCS_MMD1_ANA_SOFT_RESET_MASK,
				     assert ? QCE1204_PCS_MMD1_ANA_SOFT_RESET :
					     QCE1204_PCS_MMD1_ANA_SOFT_RELEASE);

	if (ret < 0) {
		debug("QCE1204: Analog soft reset %s failed: %d\n",
		      assert ? "assert" : "release", ret);
	} else {
		debug("QCE1204: Analog soft reset %s completed\n",
		      assert ? "assert" : "release");
	}

	return ret;
}

static int qce1204_pcs_10g_linkup(struct phy_device *phydev)
{
	u16 xpcs_data = 0;
	u32 retries = 100;
	u32 linkup = 0;

	while (linkup != QCE1204_PCS_MMD3_10GBASE_UP) {
		mdelay(1);
		if (retries-- == 0) {
			debug("QCE1204: 10G Base-R link up timeout after 100ms!\n");
			return -ETIMEDOUT;
		}

		xpcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PCS,
						 QCE1204_PCS_MMD3_10GBASE_PCS_STATUS1);
		linkup = (xpcs_data & QCE1204_PCS_MMD3_10GBASE_UP);
	}

	return 0;
}

static int qce1204_pcs_soft_reset(struct phy_device *phydev)
{
	int ret;
	u16 pcs_data = 0;
	u32 retries = 100;
	u32 reset_done = QCE1204_PCS_MMD3_XPCS_SOFT_RESET;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_DIG_CTRL1,
				     0x8000,
				     QCE1204_PCS_MMD3_XPCS_SOFT_RESET);
	if (ret < 0)
		return ret;

	while (reset_done) {
		mdelay(1);
		if (retries-- == 0)
			return -ETIMEDOUT;

		pcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PCS,
						QCE1204_PCS_MMD3_DIG_CTRL1);
		reset_done = (pcs_data & QCE1204_PCS_MMD3_XPCS_SOFT_RESET);
	}

	return 0;
}

static int qce1204_pcs_8023az_enable(struct phy_device *phydev)
{
	u16 pcs_data = 0;

	pcs_data = qce1204_pcs_read_mmd(phydev, MDIO_MMD_PCS,
					QCE1204_PCS_MMD3_AN_LP_BASE_ABL2);
	if (!(pcs_data & QCE1204_PCS_MMD3_XPCS_EEE_CAP))
		return -EOPNOTSUPP;

	/* Configure the EEE related timer */
	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
			       QCE1204_PCS_MMD3_EEE_MODE_CTRL, 0x0f40,
			       QCE1204_PCS_MMD3_EEE_RES_REGS |
			       QCE1204_PCS_MMD3_EEE_SIGN_BIT_REGS);

	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
			       QCE1204_PCS_MMD3_EEE_TX_TIMER, 0x1fff,
			       QCE1204_PCS_MMD3_EEE_TSL_REGS |
			       QCE1204_PCS_MMD3_EEE_TLU_REGS |
			       QCE1204_PCS_MMD3_EEE_TWL_REGS);

	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
			       QCE1204_PCS_MMD3_EEE_RX_TIMER, 0x1fff,
			       QCE1204_PCS_MMD3_EEE_100US_REG_REGS |
			       QCE1204_PCS_MMD3_EEE_RWR_REG_REGS);

	/* enable TRN_LPI */
	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
			       QCE1204_PCS_MMD3_EEE_MODE_CTRL1, 0x101,
			       QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE |
			       QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);

	/* enable TX/RX LPI pattern */
	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
			       QCE1204_PCS_MMD3_EEE_MODE_CTRL, 0x3, QCE1204_PCS_MMD3_EEE_EN);

	return 0;
}

#ifdef CONFIG_PHY_QCE_2204
/**
 * qce1204_pcs0_usxgmii_mode_set - Initialize PCS0 (SRDS0) for USXGMII
 * @phydev: QCE1204 master PHY device
 *
 * Initializes the PCS0 SerDes for USXGMII to connect port 5 to an external
 * QCA81xx PHY. Follows the same sequence as PCS1 QUSGMII init but targets
 * PCS0 (MDIO addr = base + 4) and enables USXGMII AN in MMD31.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce1204_pcs0_usxgmii_mode_set(struct phy_device *phydev)
{
	struct qce1204_shared_clk_data *clk_data;
	u16 pcs_data;
	u32 retries = 100;
	int eee_cap, ret;

	debug("QCE1204: PCS0 USXGMII init starting (port 5 external PHY)\n");

	clk_data = qce1204_get_shared_clk_data(phydev);

	/* Enable SRDS0 sys clock if available */
	if (clk_data && clk_data->srds0_sys_clk.dev) {
		clk_set_rate(&clk_data->srds0_sys_clk, 25000000);
		ret = clk_enable(&clk_data->srds0_sys_clk);
		if (ret < 0)
			debug("QCE1204: PCS0 srds0_sys_clk enable failed: %d\n", ret);
	}

	/* Assert then deassert SRDS0 sys reset */
	if (clk_data && clk_data->srds0_sys_reset.dev) {
		reset_assert(&clk_data->srds0_sys_reset);
		mdelay(20);
		reset_deassert(&clk_data->srds0_sys_reset);
	}

	/* Uniphy MSLDO settings */
	ret = qce1204_pcs0_write_mmd(phydev, MDIO_MMD_PMAPMD, QCE1204_PCS_MMD1_MS_LDO0, 0xcd);
	if (ret < 0) {
		debug("QCE1204: PCS0 LDO0 write failed: %d\n", ret);
		return ret;
	}
	ret = qce1204_pcs0_write_mmd(phydev, MDIO_MMD_PMAPMD, QCE1204_PCS_MMD1_MS_LDO1, 0x7f6d);
	if (ret < 0) {
		debug("QCE1204: PCS0 LDO1 write failed: %d\n", ret);
		return ret;
	}

	/* Assert SRDS0 XPCS reset */
	if (clk_data && clk_data->srds0_xpcs_reset.dev)
		reset_assert(&clk_data->srds0_xpcs_reset);

	/* Select XPCS mode */
	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				      QCE1204_PCS_MMD1_MODE_CTRL,
				      0x1f00, QCE1204_PCS_MMD1_XPCS_MODE);
	if (ret < 0) {
		debug("QCE1204: PCS0 XPCS mode select failed: %d\n", ret);
		return ret;
	}

	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				      0x182 /* QP_USXG_OPTION3 */,
				      GENMASK(4, 1), GENMASK(4, 1));
	if (ret < 0)
		return ret;

	if (clk_data) {
		qce1204_reset_assert(phydev, &clk_data->mac5_rx_srds0_reset, true);
		qce1204_reset_assert(phydev, &clk_data->mac5_tx_srds0_reset, true);
		qce1204_reset_assert(phydev, &clk_data->mac5_rx_xgmii_reset, true);
		qce1204_reset_assert(phydev, &clk_data->mac5_tx_xgmii_reset, true);
	}
	mdelay(1);
	if (clk_data) {
		qce1204_reset_assert(phydev, &clk_data->mac5_rx_srds0_reset, false);
		qce1204_reset_assert(phydev, &clk_data->mac5_tx_srds0_reset, false);
		qce1204_reset_assert(phydev, &clk_data->mac5_rx_xgmii_reset, false);
		qce1204_reset_assert(phydev, &clk_data->mac5_tx_xgmii_reset, false);
	}

	/* Wait for PCS0 calibration */
	retries = 100;
	while (retries--) {
		mdelay(1);
		pcs_data = qce1204_pcs0_read_mmd(phydev, MDIO_MMD_PMAPMD,
						 QCE1204_PCS_MMD1_CALIBRATION4);
		if (pcs_data & QCE1204_PCS_MMD1_CALIBRATION_DONE) {
			debug("QCE1204: PCS0 calibration done (reg=0x%x)\n", pcs_data);
			break;
		}
	}
	if (!retries)
		debug("QCE1204: PCS0 calibration timeout (non-fatal)\n");

	/* Enable SSC clock */
	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				      QCE1204_PCS_MMD1_SSC_CLK,
				      QCE1204_PCS_MMD1_SSC_CLK_EN,
				      QCE1204_PCS_MMD1_SSC_CLK_EN);
	if (ret < 0)
		return ret;

	/* Enable SSCG */
	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				      QCE1204_PCS_MMD1_CDA_CONTROL1,
				      0x8, QCE1204_PCS_MMD1_SSCG_ENABLE);
	if (ret < 0)
		return ret;

	/* De-assert SRDS0 XPCS reset */
	if (clk_data && clk_data->srds0_xpcs_reset.dev)
		reset_deassert(&clk_data->srds0_xpcs_reset);

	/* Set 10GBASE-R mode */
	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
				      QCE1204_PCS_MMD3_PCS_CTRL2,
				      0xf, QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret < 0)
		return ret;

	/* Wait for 10G Base-R link up on PCS0 */
	retries = 100;
	while (retries--) {
		mdelay(1);
		pcs_data = qce1204_pcs0_read_mmd(phydev, MDIO_MMD_PCS,
						 QCE1204_PCS_MMD3_10GBASE_PCS_STATUS1);
		if (pcs_data & QCE1204_PCS_MMD3_10GBASE_UP) {
			debug("QCE1204: PCS0 10G Base-R link up\n");
			break;
		}
	}
	if (!retries)
		debug("QCE1204: PCS0 10G Base-R link up timeout (non-fatal)\n");

	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
				      QCE1204_PCS_MMD3_DIG_CTRL1,
				      BIT(9), BIT(9));
	if (ret < 0)
		return ret;

	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_VEND2,
				      QCE1204_PCS_MMD_MII_CTRL,
				      (BIT(13) | BIT(6) | BIT(5) | BIT(8)),
				      (BIT(13) | BIT(6) | BIT(8)));
	if (ret < 0)
		return ret;

	/* XPCS soft reset on PCS0 */
	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
				      QCE1204_PCS_MMD3_DIG_CTRL1,
				      0x8000, QCE1204_PCS_MMD3_XPCS_SOFT_RESET);
	if (ret < 0)
		return ret;

	/* Wait for soft reset to clear */
	retries = 100;
	while (retries--) {
		mdelay(1);
		pcs_data = qce1204_pcs0_read_mmd(phydev, MDIO_MMD_PCS,
						 QCE1204_PCS_MMD3_DIG_CTRL1);
		if (!(pcs_data & QCE1204_PCS_MMD3_XPCS_SOFT_RESET))
			break;
	}

	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_VEND2,
				      QCE1204_PCS_MMD_MII_AN_INT_MSK, /* 0x8001 */
				      (BIT(8) | BIT(3)), BIT(8));
	if (ret < 0)
		return ret;

	ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_VEND2,
				      QCE1204_PCS_MMD_MII_CTRL,
				      QCE1204_PCS_MMD_MII_AN_ENABLE,
				      QCE1204_PCS_MMD_MII_AN_ENABLE);
	if (ret < 0) {
		debug("QCE1204: PCS0 USXGMII AN enable failed: %d\n", ret);
		return ret;
	}

	eee_cap = qce1204_pcs0_read_mmd(phydev, MDIO_MMD_PCS,
					QCE1204_PCS_MMD3_AN_LP_BASE_ABL2);

	if (eee_cap >= 0 && (eee_cap & QCE1204_PCS_MMD3_XPCS_EEE_CAP)) {
		debug("QCE1204: PCS0 XPCS EEE capability detected, enabling\n");

		/* LCT_RES[11:8] = 0x2, clear SIGN[6] */
		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
					      QCE1204_PCS_MMD3_EEE_MODE_CTRL,
					      GENMASK(11, 8) | BIT(6),
					      FIELD_PREP(GENMASK(11, 8), 0x2));
		if (ret < 0)
			return ret;

		/* TSL_RES[5:0] = 0x7 */
		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
					      QCE1204_PCS_MMD3_EEE_TX_TIMER,
					      GENMASK(5, 0),
					      FIELD_PREP(GENMASK(5, 0), 0x7));
		if (ret < 0)
			return ret;

		/* 100US_RES[7:0] = 0xa6 */
		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
					      QCE1204_PCS_MMD3_EEE_RX_TIMER,
					      GENMASK(7, 0),
					      FIELD_PREP(GENMASK(7, 0), 0xa6));
		if (ret < 0)
			return ret;

		/* Enable EEE LPI: TRANS_RX_LPI_EN[8] + TRANS_LPI_EN[0] */
		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
					      QCE1204_PCS_MMD3_EEE_MODE_CTRL1,
					      QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE |
					      QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE,
					      QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE |
					      QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);
		if (ret < 0)
			return ret;

		/* Enable TX/RX LPI pattern: LRX_EN[1] + LTX_EN[0] */
		ret = qce1204_pcs0_modify_mmd(phydev, MDIO_MMD_PCS,
					      QCE1204_PCS_MMD3_EEE_MODE_CTRL,
					      QCE1204_PCS_MMD3_EEE_EN,
					      QCE1204_PCS_MMD3_EEE_EN);
		if (ret < 0)
			return ret;

		debug("QCE1204: PCS0 XPCS EEE enabled\n");
	}

	debug("QCE1204: PCS0 USXGMII init completed for port 5\n");
	return 0;
}
#endif /* CONFIG_PHY_QCE_2204 */

static int qce1204_pcs_qusgmii_mode_set_internal(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;
	int ret = 0;
	u32 channel = 0;

	/* Uniphy MSLDO settings */
	ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PMAPMD, QCE1204_PCS_MMD1_MS_LDO0, 0xcd);
	if (ret < 0)
		return ret;
	ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PMAPMD, QCE1204_PCS_MMD1_MS_LDO1, 0x7f6d);
	if (ret < 0)
		return ret;

	/* Assert XPCS */
	ret = qce1204_xpcs_reset_assert(phydev, true);
	if (ret < 0)
		return ret;

	/* Select XPCS mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_MODE_CTRL,
				     0x1f00, QCE1204_PCS_MMD1_XPCS_MODE);
	if (ret < 0)
		return ret;

	/* Disable PCS GMII/XGMII clock */
	for (channel = 1; channel <= 4; channel++) {
		ret = qce1204_pcs_clk_set(phydev, channel, false);
		if (ret < 0)
			return ret;
	}

	/*
	 * Set QP_USXG_OPTION3 BIT(1): required for USXGMII/QUSGMII operation.
	 * Present in experimental tree after XPCS mode select; absent in
	 * earlier production code.  Mirrors the experimental driver sequence.
	 */
	if (priv && priv->package_mode == PHY_INTERFACE_MODE_INTERNAL) {
		ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
					     0x182 /*QP_USXG_OPTION3*/, BIT(1), BIT(1));
		if (ret < 0)
			return ret;
	}
	/* Reset and release PCS GMII/XGMII and PHY GMII */
	debug("QCE1204: Reset and release PCS GMII/XGMII and PHY GMII\n");
	for (channel = 1; channel <= 4; channel++)
		qce1204_pcs_clk_reset(phydev, channel);

	/* Analog soft reset assert */
	ret = qce1204_pcs_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(10);

	/* Analog soft reset release */
	ret = qce1204_pcs_assert(phydev, false);
	if (ret < 0)
		return ret;

	/* Wait for calibration */
	qce1204_pcs_calibration(phydev);

	/* Open SSC clock */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_SSC_CLK,
				     QCE1204_PCS_MMD1_SSC_CLK_EN,
				     QCE1204_PCS_MMD1_SSC_CLK_EN);
	if (ret < 0)
		return ret;

	/* Enable SSCG */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_CDA_CONTROL1,
				     0x8, QCE1204_PCS_MMD1_SSCG_ENABLE);
	if (ret < 0)
		return ret;

	/* De-assert XPCS */
	debug("QCE1204: De-assert XPCS\n");
	ret = qce1204_xpcs_reset_assert(phydev, false);
	if (ret < 0)
		return ret;

	/* Set BaseR mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCE1204_PCS_MMD3_PCS_CTRL2,
				     0xf,
				     QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret < 0)
		return ret;

	/* Wait for 10G Base-R link up */
	ret = qce1204_pcs_10g_linkup(phydev);
	if (ret < 0)
		return ret;

	if (priv && priv->package_mode == PHY_INTERFACE_MODE_QUSGMII) {
		/* Enable QUSGMII mode */
		ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
					     QCE1204_PCS_MMD3_DIG_CTRL1,
					     0x200, QCE1204_PCS_MMD3_QUSGMII_EN);
		if (ret < 0)
			return ret;

		/* Set QUSGMII mode */
		ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
					     QCE1204_PCS_MMD3_VR_RPCS_TPC,
					     0x1c00, QCE1204_PCS_MMD3_QUSGMII_MODE);
		if (ret < 0)
			return ret;

		/* Set initial per-channel XPCS speed to 10M */
		for (channel = 1; channel <= 4; channel++) {
			ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
							     QCE1204_PCS_MMD_MII_CTRL,
							     QCE1204_PCS_SPEED_MASK,
							     QCE1204_PCS_SPEED_10M);
			if (ret < 0)
				return ret;
		}

		/* Set AM interval */
		ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PCS,
					    QCE1204_PCS_MMD3_MII_AM_INTERVAL,
					    QCE1204_PCS_MMD3_MII_AM_INTERVAL_VAL);
		if (ret < 0)
			return ret;
	}

	/* XPCS soft reset */
	ret = qce1204_pcs_soft_reset(phydev);

	return ret;
}

static int qce1204_pcs_qusgmii_mode_set(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;
	int ret = 0;
	u32 channel = 0;

	/* Disable IPG tuning bypass */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_BYPASS_TUNING_IPG,
				     0x0fff, 0);
	if (ret < 0)
		return ret;

	/* Configure PCS: calibration, 10G Base-R link-up, optional QUSGMII framing */
	ret = qce1204_pcs_qusgmii_mode_set_internal(phydev);
	if (ret < 0)
		return ret;

	if (priv && priv->package_mode == PHY_INTERFACE_MODE_QUSGMII) {
		for (channel = 1; channel <= 4; channel++) {
			debug("QCE1204: Configuring channel %d\n", channel);

			/* Enable auto-neg complete interrupt, MII 4-bits mode, TX config */
			ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
							     QCE1204_PCS_MMD_MII_AN_INT_MSK,
							     0x109,
							     QCE1204_PCS_MMD_AN_COMPLETE_INT |
							     QCE1204_PCS_MMD_MII_4BITS_CTRL |
							     QCE1204_PCS_MMD_TX_CONFIG_CTRL);
			if (ret < 0)
				return ret;

			/* Enable autoneg ability */
			ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
							     QCE1204_PCS_MMD_MII_CTRL,
							     QCE1204_PCS_MMD_MII_AN_ENABLE,
							     QCE1204_PCS_MMD_MII_AN_ENABLE);
			if (ret < 0)
				return ret;

			/* Disable TICD (TX IPG Check Disable) */
			ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
							     QCE1204_PCS_MMD_MII_XAUI_MODE_CTRL,
							     QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE,
							     QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE);
			if (ret < 0)
				return ret;

			/*
			 * Enable PHY mode control: tells the XPCS to sync link
			 * state from the external PHY.  PHY mode only.
			 */
			ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
							     QCE1204_PCS_MMD_MII_DIG_CTRL,
							     BIT(0),
							     QCE1204_PCS_MMD_PHY_MODE_CTRL_EN);
			if (ret < 0)
				return ret;
		}

		/*
		 * Enable EEE (802.3az) on the XPCS.  PHY mode only – in switch
		 * mode the switch fabric manages EEE independently and the PCS
		 * EEE configuration is not required.
		 */
		qce1204_pcs_8023az_enable(phydev);
	}

	return 0;
}

static int qce1204_phy_channel_get(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;

	return (phydev->addr - priv->base_phy_addr + 1);
}

static int qce1204_phy_fifo_reset(struct phy_device *phydev, bool enable)
{
	u16 phy_data = 0;

	if (!enable)
		phy_data |= QCE1204_PHY_FIFO_RESET;

	return phy_modify(phydev, MDIO_MMD_VEND2,
			  QCE1204_PHY_CONTROL,
			  QCE1204_PHY_FIFO_RESET,
			  phy_data);
}

static int qce1204_pcs_qusgmii_reset(struct phy_device *phydev, u32 channel)
{
	int ret;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_QUSGMII_RESET,
				     BIT(channel - 1), 0);
	if (ret < 0)
		return ret;

	mdelay(1);

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCE1204_PCS_MMD1_QUSGMII_RESET,
				     BIT(channel - 1), BIT(channel - 1));

	return ret;
}

static int qce1204_pcs_qusgmii_function_reset(struct phy_device *phydev, u32 channel)
{
	int ret;

	if (channel == 1) {
		ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
					     QCE1204_PCS_MMD_MII_DIG_CTRL,
					     0x400, QCE1204_PCS_MMD3_QUSGMII_FIFO_RESET);
	} else {
		int mmd_id = qce1204_pcs_mmd_get(phydev, channel);

		if (mmd_id < 0)
			return -EOPNOTSUPP;

		ret = qce1204_pcs_modify_mmd(phydev, mmd_id,
					     QCE1204_PCS_MMD_MII_DIG_CTRL,
					     0x20, 0x20);
	}

	return ret;
}

static int qce1204_pcs_speed_clock_set(struct phy_device *phydev, u32 channel, u32 speed)
{
	unsigned long gmii_clk_rate, xgmii_clk_rate;

	switch (speed) {
	case SPEED_2500:
		gmii_clk_rate = QCE1204_CLK_RATE_312P5M;
		xgmii_clk_rate = QCE1204_CLK_RATE_78P125M;
		break;
	case SPEED_1000:
		gmii_clk_rate = QCE1204_CLK_RATE_125M;
		xgmii_clk_rate = QCE1204_CLK_RATE_125M;
		break;
	case SPEED_100:
		gmii_clk_rate = QCE1204_CLK_RATE_25M;
		xgmii_clk_rate = QCE1204_CLK_RATE_25M;
		break;
	case SPEED_10:
		gmii_clk_rate = QCE1204_CLK_RATE_2P5M;
		xgmii_clk_rate = QCE1204_CLK_RATE_2P5M;
		break;
	default:
		debug("QCE1204: Unsupported speed: %d\n", speed);
		return -EOPNOTSUPP;
	}

	qce1204_pcs_clk_set_rate(phydev, channel, gmii_clk_rate, xgmii_clk_rate);

	return 0;
}

static int qce1204_phy_qusgmii_speed_fix_up(struct phy_device *phydev)
{
	u32 channel;
	int ret;
	bool clk_en = false;
	bool pcs_clk_enabled = false;
	bool phy_clk_enabled = false;

	/* channel is from 1 ~ 4 */
	channel = qce1204_phy_channel_get(phydev);

	/*
	 * enable pcs clocks and phy clocks for link up
	 * disable pcs clocks and phy clocks for link down
	 */
	if (phydev->link) {
		ret = qce1204_pcs_speed_clock_set(phydev, channel, phydev->speed);
		if (ret < 0) {
			debug("QCE1204: Failed to set speed clocks for CH%d: %d\n", channel, ret);
			return ret;
		}
		mdelay(10);
		clk_en = true;
	}

	ret = qce1204_pcs_clk_set(phydev, channel, clk_en);
	if (ret < 0)
		return ret;

	pcs_clk_enabled = clk_en;

	ret = qce1204_phy_clk_set(phydev, clk_en);
	if (ret < 0)
		goto err_disable_pcs_clk;

	phy_clk_enabled = clk_en;

	mdelay(100);

	/* reset pcs clocks and phy clocks */
	ret = qce1204_pcs_clk_reset(phydev, channel);
	if (ret < 0)
		goto err_disable_clks;

	ret = qce1204_phy_clk_reset(phydev);
	if (ret < 0)
		goto err_disable_clks;

	ret = qce1204_pcs_qusgmii_reset(phydev, channel);
	if (ret < 0)
		goto err_disable_clks;

	ret = qce1204_pcs_qusgmii_function_reset(phydev, channel);
	if (ret < 0)
		goto err_disable_clks;

	ret = qce1204_phy_fifo_reset(phydev, true);
	if (ret < 0)
		goto err_disable_clks;

	mdelay(1);

	if (phydev->link) {
		ret = qce1204_phy_fifo_reset(phydev, false);
		if (ret < 0)
			goto err_disable_clks;
	}

	/* change IPG from 10 to 11 for 1G speed (QUSGMII mode only) */
	ret = phy_modify(phydev, MDIO_MMD_AN, QCE1204_PHY_MMD7_IPG_OP,
			 QCE1204_PHY_IPG_10_TO_11_EN, phydev->speed == SPEED_1000 ?
			 QCE1204_PHY_IPG_10_TO_11_EN : 0);
	if (ret < 0)
		goto err_disable_clks;

	return 0;

err_disable_clks:
	if (phy_clk_enabled)
		qce1204_phy_clk_set(phydev, false);
err_disable_pcs_clk:
	if (pcs_clk_enabled)
		qce1204_pcs_clk_set(phydev, channel, false);
	return ret;
}

static int qce1204_probe(struct phy_device *phydev)
{
	struct qce1204_priv *priv;

	priv = malloc(sizeof(*priv));
	if (!priv)
		return -ENOMEM;

	memset(priv, 0, sizeof(*priv));
	phydev->priv = priv;

	priv->clocks_initialized = false;
	priv->package_mode = PHY_INTERFACE_MODE_NA;
	priv->shared_clk_data = NULL;

	return 0;
}

int qce2204_phy_stats_enable(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			     QCA81XX_MMD3_10G_FRAME_CHECK_CTRL,
			     QCA81XX_MMD3_10G_FRAME_CHECK_EN,
			     QCA81XX_MMD3_10G_FRAME_CHECK_EN);
	if (ret < 0)
		return ret;
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN,
			     QCA81XX_MMD7_COUNTER_CTRL,
			     QCA81XX_MMD7_FRAME_CHECK_EN,
			     QCA81XX_MMD7_FRAME_CHECK_EN);

	return ret;
}

static int qce1204_config(struct phy_device *phydev)
{
	struct qce1204_priv *priv = phydev->priv;
	int ret = 0;

	if (!priv->clocks_initialized) {
		if (!g_shared_clk_data) {
			g_shared_clk_data = malloc(sizeof(*g_shared_clk_data));
			if (!g_shared_clk_data)
				return -ENOMEM;
			memset(g_shared_clk_data, 0, sizeof(*g_shared_clk_data));

			ret = qce1204_phy_package_mode_probe(phydev, priv);
			if (ret < 0)
				return ret;

			ret = qce1204_phy_shared_clk_init(phydev, g_shared_clk_data);
			if (ret < 0)
				return ret;
		}
		priv->shared_clk_data = g_shared_clk_data;

		ret = qce1204_phy_clk_init(phydev, &priv->clk_data);
		if (ret < 0)
			return ret;

		priv->clocks_initialized = true;

		/* Enable PCS SYS clock */
		ret = qce1204_pcs_sys_clk_set_rate(phydev, 25000000);
		if (ret < 0)
			return ret;

		/* Enable PCS SYS clock */
		ret = qce1204_pcs_sys_clk_set(phydev, true);
		if (ret < 0)
			return ret;

		if (priv->package_mode == PHY_INTERFACE_MODE_QUSGMII) {
			/* Set Work Mode */
			ret = qce1204_soc_modify(phydev, QCE1204_WORK_MODE_SEL,
						 QCE1204_PHY_MODE_MASK, QCE1204_PHY_MODE);
			if (ret < 0)
				return ret;

			ret = qce1204_switch_clks_reset_assert(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_set_srds_mux(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_pcs_sys_reset(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_pcs_qusgmii_mode_set(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_ahb_clk_set_rate(phydev, QCE1204_CLK_RATE_104M);
			if (ret < 0)
				return ret;
		}
#if CONFIG_PHY_QCE_2204
		else if (priv->package_mode == PHY_INTERFACE_MODE_INTERNAL) {
			phydev->interface = PHY_INTERFACE_MODE_10GBASER;

			ret = qce1204_switch_clks_reset_assert(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_switch_clks_reset_deassert(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_pcs_sys_clk_set(phydev, true);
			if (ret < 0)
				return ret;

			ret = qce1204_set_srds_mux(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_pcs_sys_reset(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_switch_clks_enable(phydev);
			if (ret < 0)
				return ret;

			ret = qce1204_ahb_clk_set_rate(phydev, QCE1204_CLK_RATE_104M);
			if (ret < 0)
				return ret;

			ret = qce1204_pcs_qusgmii_mode_set_internal(phydev);
			if (ret < 0)
				return ret;

			if (!g_switch_ppe_initialized) {
				ret = qce2204_ppe_hw_init(phydev);
				if (ret < 0)
					return ret;

				ret = qce2204_port_mac_init(phydev);
				if (ret < 0)
					return ret;

				ret = qce2204_setup_none_tag_vsi(phydev);
				if (ret < 0)
					return ret;

				g_switch_ppe_initialized = true;
				dev_dbg(phydev->dev,
					"QCE1204: PPE hardware initialized\n");
			}

			ret = qce1204_soc_modify(phydev, QCE1204_WORK_MODE_SEL,
						 QCE1204_SWITCH_MODE_MASK, QCE1204_SWITCH_MODE);
			if (ret < 0)
				return ret;

			/*
			 * Connect the external QCA81xx PHY on port 5 via the
			 * standard U-Boot PHY framework so its probe()/config()
			 * hooks run (USXGMII SerDes init on the PHY side).
			 * Must happen after PCS0 is up so the PHY can train.
			 */
			{
				struct qce1204_switch_port *p5 =
					&priv->sw_ports[QCE1204_MAX_SWITCH_PORTS - 1];

				if (p5 && p5->valid && p5->is_external && !p5->ext_phydev) {
					/* Initialize PCS0 (SRDS0) for port 5 USXGMII */
					ret = qce1204_pcs0_usxgmii_mode_set(phydev);
					if (ret < 0) {
						dev_warn(phydev->dev,
							 "QCE1204: PCS0 USXGMII init failed: %d (port 5 may not work)\n",
							 ret);
						/* Non-fatal: continue without port 5 */
					}

					p5->ext_phydev = phy_connect(
						phydev->bus,
						p5->phy_addr,
						phydev->dev,
						PHY_INTERFACE_MODE_USXGMII);
					if (p5->ext_phydev) {
						ret = phy_config(p5->ext_phydev);
						if (ret < 0)
							dev_warn(phydev->dev,
								 "QCE1204: Port 5 phy_config failed: %d\n",
								 ret);
						else
							dev_dbg(phydev->dev,
								"QCE1204: Port 5 QCA81xx probed OK (addr=%d)\n",
								p5->phy_addr);
					} else {
						dev_warn(phydev->dev,
							 "QCE1204: Port 5 phy_connect failed (addr=%d)\n",
							 p5->phy_addr);
					}
				}
			}

			ret = qce2204_phy_stats_enable(phydev);
			if (ret < 0)
				return ret;
		}
#endif
		ret = qce1204_phy_tlmm_init(phydev);
		if (ret < 0)
			return ret;

		ret = qce1204_soc_modify(phydev, QCE1204_EPHY_CFG, QCE1204_EPHY_LDO_CTRL, 0);
		if (ret < 0)
			return ret;
		mdelay(10);
	}

	ret = qce1204_phy_sys_clk_set(phydev, true);
	if (ret < 0)
		return ret;

	ret = qce1204_phy_sys_reset(phydev);
	if (ret < 0)
		return ret;

	ret = qce1204_phy_eee_init(phydev);
	if (ret < 0)
		return ret;

	ret = qce1204_phy_10m_dac_init(phydev);
	if (ret < 0)
		return ret;

	ret = qce1204_phy_cdt_thresh_init(phydev);
	if (ret < 0) {
		debug("QCE1204: CONFIG - CDT init failed: %d\n", ret);
		return ret;
	}

	/* adjust the tx gain to improve 2.5G performance */
	ret = qce1204_phy_debug_write(phydev, QCE1204_DEBUG_ANA_2P5G_TX_GAIN_CTRL,
				      QCE1204_DEBUG_ANA_2P5G_TX_GAIN_VAL);
	if (ret < 0)
		return ret;
	/* force pll0 on to improve traffic performance */
	ret = qce1204_phy_debug_modify(phydev, QCE1204_DEBUG_PLL_CTRL,
				       QCE1204_DEBUG_PLL0_FORCE_ON, QCE1204_DEBUG_PLL0_FORCE_ON);
	if (ret < 0)
		return ret;

	/* 10M speed also use led0 in default as other speeds */
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, QCE1204_MMD7_LED0_CTRL,
			     QCE1204_SPEED_10M_ON, QCE1204_SPEED_10M_ON);
	if (ret < 0)
		return ret;

#ifdef CONFIG_PHY_QCE_2204
	if (priv && priv->package_mode == PHY_INTERFACE_MODE_INTERNAL) {
		struct phy_device local_phydev;
		int i;

		memcpy(&local_phydev, phydev, sizeof(struct phy_device));
		for (i = 0; i < QCE1204_MAX_SWITCH_PORTS; i++) {
			if (!priv->sw_ports[i].valid)
				continue;
			/* External PHY ports (QCA81xx) are handled via phy_config() */
			if (priv->sw_ports[i].is_external)
				continue;
			local_phydev.addr = priv->sw_ports[i].phy_addr;
			ret = phy_modify(&local_phydev, MDIO_MMD_VEND2,
					 MII_BMCR, BMCR_RESET, BMCR_RESET);
			if (ret < 0)
				return ret;
		}
	} else
#endif
	{
		ret = phy_modify(phydev, MDIO_MMD_VEND2, MII_BMCR, BMCR_RESET, BMCR_RESET);
	}
	return ret;
}

static int qce1204_startup(struct phy_device *phydev)
{
	u16 phy_data;
	u16 speed_bits;
	int link, speed;
	int old_link = phydev->link;
	int old_speed = phydev->speed;
	int ret;
#ifdef CONFIG_PHY_QCE_2204
	struct qce1204_priv *priv = phydev->priv;

	if (priv && priv->package_mode == PHY_INTERFACE_MODE_INTERNAL) {
		if (!qce1204_switch_configure_ports(phydev)) {
			phydev->speed  = SPEED_2500;
			phydev->duplex = DUPLEX_FULL;
			phydev->link   = 1;
		}
		return 0;
	}
#endif

	phy_data = phy_read(phydev, MDIO_MMD_VEND2, QCE1204_PHY_SPEC_STATUS);

	link = (phy_data & QCE1204_PHY_SS_LINK_STATUS) ? 1 : 0;
	speed_bits = phy_data & QCE1204_PHY_SS_SPEED_MASK;

	switch (speed_bits) {
	case QCE1204_PHY_SS_SPEED_2500:
		speed = SPEED_2500;
		break;
	case QCE1204_PHY_SS_SPEED_1000:
		speed = SPEED_1000;
		break;
	case QCE1204_PHY_SS_SPEED_100:
		speed = SPEED_100;
		break;
	case QCE1204_PHY_SS_SPEED_10:
		speed = SPEED_10;
		break;
	default:
		speed = SPEED_UNKNOWN;
	}

	if (phy_data & QCE1204_PHY_SS_DUPLEX_FULL)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	phydev->link = link;
	phydev->speed = speed;

	if (old_link != link || old_speed != speed) {
		ret = qce1204_phy_qusgmii_speed_fix_up(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ipq52xx_phy_clk_set(struct phy_device *phydev, bool enable)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	int ret;

	if (!priv)
		return -EINVAL;

	if (priv->ephy_rx_clk.dev) {
		ret = enable ? clk_enable(&priv->ephy_rx_clk)
			     : clk_disable(&priv->ephy_rx_clk);
		if (ret)
			return ret;
	}

	if (priv->ephy_rx_mux_reg)
		writel(priv->ephy_rx_mux_val, priv->ephy_rx_mux_reg);

	if (priv->ephy_tx_clk.dev) {
		ret = enable ? clk_enable(&priv->ephy_tx_clk)
			     : clk_disable(&priv->ephy_tx_clk);
		if (ret)
			return ret;
	}

	if (priv->ephy_tx_mux_reg)
		writel(priv->ephy_tx_mux_val, priv->ephy_tx_mux_reg);

	return 0;
}

static int ipq52xx_phy_clk_reset(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	int ret;

	if (!priv)
		return -EINVAL;

	if (priv->ephy_rx_reset.dev) {
		ret = reset_assert(&priv->ephy_rx_reset);
		if (ret)
			return ret;
	}
	if (priv->ephy_tx_reset.dev) {
		ret = reset_assert(&priv->ephy_tx_reset);
		if (ret)
			return ret;
	}

	mdelay(1);

	if (priv->ephy_rx_reset.dev) {
		ret = reset_deassert(&priv->ephy_rx_reset);
		if (ret)
			return ret;
	}
	if (priv->ephy_tx_reset.dev) {
		ret = reset_deassert(&priv->ephy_tx_reset);
		if (ret)
			return ret;
	}

	return 0;
}

static int ipq52xx_phy_sys_reset(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	int ret;

	if (!priv)
		return -EINVAL;

	if (priv->sys_clk_reset.dev) {
		ret = reset_assert(&priv->sys_clk_reset);
		if (ret)
			return ret;
	}

	mdelay(10);

	if (priv->sys_clk_reset.dev) {
		ret = reset_deassert(&priv->sys_clk_reset);
		if (ret)
			return ret;
	}

	return 0;
}

static int ipq52xx_phy_ldo_loading_enble(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	u32 val;

	if (!priv || !priv->ldo_bias_reg)
		return -EINVAL;

	val = readl(priv->ldo_bias_reg);
	val &= ~GPHY_LDO_BIAS_EN;
	writel(val, priv->ldo_bias_reg);

	return 0;
}

static int ipq52xx_phy_internal_speed_fix_up(struct phy_device *phydev)
{
	bool clk_en = false;
	int ret;
	u32 val = 0;
	struct ipq52xx_phy_priv *priv = phydev->priv;

	if (phydev->link) {
		val = readl(priv->pll_src_sel_reg);
		if (phydev->speed == SPEED_2500)
			val |= CMN_PLL_312P5M_SEL;
		else
			val &= ~CMN_PLL_312P5M_SEL;
		writel(val, priv->pll_src_sel_reg);
		clk_en = true;
	}
	ret = ipq52xx_phy_clk_set(phydev, clk_en);
	if (ret < 0)
		return ret;

	ret = ipq52xx_phy_clk_reset(phydev);
	if (ret < 0)
		return ret;
	mdelay(1);

	ret = qce1204_phy_fifo_reset(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qce1204_phy_fifo_reset(phydev, false);
	if (ret < 0)
		return ret;

	return 0;
}

static int ipq52xx_startup(struct phy_device *phydev)
{
	u16 phy_data;
	u16 speed_bits;
	int link, speed;
	int old_link = phydev->link;
	int old_speed = phydev->speed;
	int ret;

	phy_data = phy_read(phydev, MDIO_MMD_VEND2, QCE1204_PHY_SPEC_STATUS);

	link = (phy_data & QCE1204_PHY_SS_LINK_STATUS) ? 1 : 0;
	speed_bits = phy_data & QCE1204_PHY_SS_SPEED_MASK;

	switch (speed_bits) {
	case QCE1204_PHY_SS_SPEED_2500:
		speed = SPEED_2500;
		break;
	case QCE1204_PHY_SS_SPEED_1000:
		speed = SPEED_1000;
		break;
	case QCE1204_PHY_SS_SPEED_100:
		speed = SPEED_100;
		break;
	case QCE1204_PHY_SS_SPEED_10:
		speed = SPEED_10;
		break;
	default:
		speed = SPEED_UNKNOWN;
	}

	if (phy_data & QCE1204_PHY_SS_DUPLEX_FULL)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	phydev->link = link;
	phydev->speed = speed;

	if (old_link != link || old_speed != speed) {
		ret = ipq52xx_phy_internal_speed_fix_up(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ipq52xx_phy_probe(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv;

	priv = malloc(sizeof(*priv));
	if (!priv)
		return -ENOMEM;

	memset(priv, 0, sizeof(*priv));
	phydev->priv = priv;

	priv->ldo_bias_reg    = (u32 *)(uintptr_t)TCSR_GPHY_LDO_BIAS_EN;
	priv->pll_src_sel_reg = (u32 *)(uintptr_t)CMN_PLL_SRC_SEL_REG;
	priv->ephy_rx_mux_reg = (u32 *)(uintptr_t)NSS_CC_EPHY_RX_MUX_SEL;
	priv->ephy_tx_mux_reg = (u32 *)(uintptr_t)NSS_CC_EPHY_TX_MUX_SEL;

	return 0;
}

static int ipq52xx_phy_dt_init(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	ofnode node = phydev->node;
	u32 addr;
	int ret;

	if (!priv)
		return -EINVAL;

	if (!ofnode_valid(node)) {
		debug("IPQ52xx PHY: no DT node, using hard-coded register defaults\n");
		return 0;
	}

	ret = clk_get_by_name_nodev(node, "ephy_rx_clk", &priv->ephy_rx_clk);
	if (ret < 0)
		debug("IPQ52xx PHY: ephy_rx_clk not found (%d), using default\n", ret);

	ret = clk_get_by_name_nodev(node, "ephy_tx_clk", &priv->ephy_tx_clk);
	if (ret < 0)
		debug("IPQ52xx PHY: ephy_tx_clk not found (%d), using default\n", ret);

	ret = clk_get_by_name_nodev(node, "sys_clk", &priv->sys_clk);
	if (ret < 0)
		debug("IPQ52xx PHY: sys_clk not found (%d), using default\n", ret);

	ret = reset_get_by_index_nodev(node, 0, &priv->ephy_rx_reset);
	if (ret < 0)
		debug("IPQ52xx PHY: ephy_rx_reset not found (%d)\n", ret);

	ret = reset_get_by_index_nodev(node, 1, &priv->ephy_tx_reset);
	if (ret < 0)
		debug("IPQ52xx PHY: ephy_tx_reset not found (%d)\n", ret);

	ret = reset_get_by_index_nodev(node, 2, &priv->sys_clk_reset);
	if (ret < 0)
		debug("IPQ52xx PHY: sys_clk_reset not found (%d)\n", ret);

	if (ofnode_read_u32(node, "qcom,ldo-bias-reg", &addr) == 0)
		priv->ldo_bias_reg = (u32 *)(uintptr_t)addr;

	if (ofnode_read_u32(node, "qcom,pll-src-sel-reg", &addr) == 0)
		priv->pll_src_sel_reg = (u32 *)(uintptr_t)addr;

	{
		u32 mux_cfg[2] = { NSS_CC_EPHY_RX_MUX_SEL, 0 };

		if (ofnode_read_u32_array(node, "qcom,ephy-rx-mux-reg", mux_cfg, 2) == 0 ||
		    ofnode_read_u32(node, "qcom,ephy-rx-mux-reg", &mux_cfg[0]) == 0) {
			priv->ephy_rx_mux_reg = (u32 *)(uintptr_t)mux_cfg[0];
			priv->ephy_rx_mux_val = mux_cfg[1];
		}
	}

	{
		u32 mux_cfg[2] = { NSS_CC_EPHY_TX_MUX_SEL, 0 };

		if (ofnode_read_u32_array(node, "qcom,ephy-tx-mux-reg", mux_cfg, 2) == 0 ||
		    ofnode_read_u32(node, "qcom,ephy-tx-mux-reg", &mux_cfg[0]) == 0) {
			priv->ephy_tx_mux_reg = (u32 *)(uintptr_t)mux_cfg[0];
			priv->ephy_tx_mux_val = mux_cfg[1];
		}
	}

	return 0;
}

int ipq52xx_phy_config_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = ipq52xx_phy_dt_init(phydev);
	if (ret < 0)
		return ret;

	/* enable efuse loading into analog circuit */
	ret = ipq52xx_phy_ldo_loading_enble(phydev);
	if (ret < 0)
		return ret;
	mdelay(10);
	ret = ipq52xx_phy_sys_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_eee_init(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_10m_dac_init(phydev);
	if (ret < 0)
		return ret;
	ret = qce2204_phy_stats_enable(phydev);
	if (ret < 0)
		return ret;
	ret = phy_modify(phydev, MDIO_MMD_VEND2, MII_BMCR, BMCR_RESET, BMCR_RESET);
	if (ret < 0)
		return ret;

	return 0;
}

U_BOOT_PHY_DRIVER(qce1204_driver) = {
	.name = "QCE1204 PHY Driver",
	.uid = QCE1204_PHY_ID,
	.mask = 0xfffffff0,
	.features = PHY_GBIT_FEATURES,
	.probe = &qce1204_probe,
	.config = &qce1204_config,
	.startup = &qce1204_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(ipq52xx_driver) = {
	.name = "IPQ52XX PHY Driver",
	.uid = IPQ52XX_PHY_ID,
	.mask = 0xffffffff,
	.features = PHY_GBIT_FEATURES,
	.probe = &ipq52xx_phy_probe,
	.config = &ipq52xx_phy_config_init,
	.startup = &ipq52xx_startup,
	.shutdown = &genphy_shutdown,
};
