/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <miiphy.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <asm/arch/dt-bindings/clock/qcom,qce2204-nsscc.h>
#include <asm/arch/dt-bindings/reset/qcom,qce2204-nsscc.h>
#include <reset-uclass.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <log.h>

#include "clock-qcom.h"

#ifndef clamp_t
#define clamp_t(type, val, min, max) min_t(type, max_t(type, val, min), max)
#endif

#define QCE2204_CLK_REG_BASE		0x6800000

#define RCG_CFG_REG		0x4
#define RCG_M_REG		0x8
#define RCG_N_REG		0xc
#define RCG_D_REG		0x10

#define GCC_SWITCH_CORE_CMD_RCGR		0x000
#define GCC_MAC0_TX_CMD_RCGR			0x078
#define GCC_MAC0_RX_CMD_RCGR			0x094
#define GCC_MAC1_TX_CMD_RCGR			0x0B4
#define GCC_MAC1_RX_CMD_RCGR			0x0E0
#define GCC_MAC2_TX_CMD_RCGR			0x110
#define GCC_MAC2_RX_CMD_RCGR			0x13C
#define GCC_MAC3_TX_CMD_RCGR			0x164
#define GCC_MAC3_RX_CMD_RCGR			0x190
#define GCC_MAC4_TX_CMD_RCGR			0x1BC
#define GCC_MAC4_RX_CMD_RCGR			0x1E4
#define GCC_MAC5_TX_CMD_RCGR			0x210
#define GCC_MAC5_RX_CMD_RCGR			0x230
#define GCC_AHB_CMD_RCGR			0x250
#define GCC_SYS_CMD_RCGR			0x278
#define GCC_SEC_CTRL_CMD_RCGR			0x2A8
#define GCC_SLEEP_CMD_RCGR			0x2BC

#define GCC_MAC0_TX_DIV_CDIVR			0x080
#define GCC_MAC0_RX_DIV_CDIVR			0x09C
#define GCC_MAC1_TX_DIV_CDIVR			0x0BC
#define GCC_MAC1_SRDS1_CH0_XGMII_RX_DIV_CDIVR	0x0C4
#define GCC_MAC1_RX_DIV_CDIVR			0x0E8
#define GCC_MAC1_SRDS1_CH0_XGMII_TX_DIV_CDIVR	0x0F0
#define GCC_MAC2_TX_DIV_CDIVR			0x118
#define GCC_MAC2_SRDS1_CH1_XGMII_RX_DIV_CDIVR	0x120
#define GCC_MAC2_RX_DIV_CDIVR			0x144
#define GCC_MAC2_SRDS1_CH1_XGMII_TX_DIV_CDIVR	0x14C
#define GCC_MAC3_TX_DIV_CDIVR			0x16C
#define GCC_MAC3_SRDS1_CH2_XGMII_RX_DIV_CDIVR	0x174
#define GCC_MAC3_RX_DIV_CDIVR			0x198
#define GCC_MAC3_SRDS1_CH2_XGMII_TX_DIV_CDIVR	0x1A0
#define GCC_MAC4_TX_DIV_CDIVR			0x1C4
#define GCC_MAC4_SRDS1_CH3_XGMII_RX_DIV_CDIVR	0x1CC
#define GCC_MAC4_RX_DIV_CDIVR			0x1EC
#define GCC_MAC4_SRDS1_CH3_XGMII_TX_DIV_CDIVR	0x1F4
#define GCC_MAC5_TX_DIV_CDIVR			0x218
/* MAC5 SRDS0 XGMII uses the same divider as the main TX clock (no separate CDIVR) */
#define GCC_MAC5_RX_DIV_CDIVR			0x238
/* MAC5 SRDS0 XGMII uses the same divider as the main RX clock (no separate CDIVR) */
#define GCC_SLEEP_DIV_CDIVR			0x2C4
#define GCC_DEBUG_DIV_CDIVR			0x2D8

#define CLK_1_25_MHZ			(1250000UL)
#define CLK_2_5_MHZ			(2500000UL)
#define CLK_12_5_MHZ			(12500000UL)
#define CLK_25_MHZ			(25000000UL)
#define CLK_78_125_MHZ			(78125000UL)
#define CLK_50_MHZ			(50000000UL)
#define CLK_125_MHZ			(125000000UL)
#define CLK_156_25_MHZ			(156250000UL)
#define CLK_312_5_MHZ			(312500000UL)

struct nsscc_qce2204_priv {
	struct udevice *mdio_bus;
	int phy_addr;
	struct msm_clk_data *data;
};

static inline void qce2204_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid,
				      u16 *reg_high)
{
	*reg_low = FIELD_GET(GENMASK(3, 0), regaddr);
	*reg_low &= 0xc;
	*reg_low <<= 1;

	*reg_mid = FIELD_GET(GENMASK(19, 4), regaddr);

	*reg_high = FIELD_GET(GENMASK(23, 20), regaddr);
	*reg_high <<= 1;
	*reg_high |= BIT(0);
}

static int qce2204_ahb_read(struct nsscc_qce2204_priv *priv, u32 reg, u32 *val)
{
	u16 reg_low, reg_mid, reg_high;
	int ret, data;
	struct udevice *bus = priv->mdio_bus;
	int addr = 6;

	qce2204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	dm_mdio_write(bus, addr, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(200);

	ret = dm_mdio_read(bus, addr, MDIO_DEVAD_NONE, reg_low);
	if (ret >= 0) {
		data = ret;
		ret = dm_mdio_read(bus, addr, MDIO_DEVAD_NONE,
				   (reg_low | BIT(2)));
		if (ret >= 0)
			*val = data | ret << 16;
		debug("##%s | SOC[0x%08x]: 0x%08x\n", __func__, reg, *val);
	}

	return ret < 0 ? ret : 0;
}

static int qce2204_ahb_write(struct nsscc_qce2204_priv *priv, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	int ret;
	struct udevice *bus = priv->mdio_bus;
	int addr = 6;

	qce2204_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	dm_mdio_write(bus, addr, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(200);

	ret = dm_mdio_write(bus, addr, MDIO_DEVAD_NONE, reg_low, val & 0xffff);
	if (!ret)
		ret = dm_mdio_write(bus, addr, MDIO_DEVAD_NONE,
				    (reg_low | BIT(2)), (val >> 16) & 0xffff);

	if (IS_ENABLED(CONFIG_DEBUG)) {
		u32 l_val = 0;

		if (!qce2204_ahb_read(priv, reg, &l_val))
			debug("## %s | %d | reg : 0x%x | val: 0x%x [%s]\n",
			      __func__, __LINE__, reg, val,
			      l_val == val ? "MATCH" : "NO-MATCH");
	}

	return ret;
}

static int qce2204_ahb_update(struct nsscc_qce2204_priv *priv, u32 reg,
			      u32 mask, u32 value)
{
	u32 val;
	int ret;

	ret = qce2204_ahb_read(priv, reg, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= value;

	return qce2204_ahb_write(priv, reg, val);
}

#define APPS_CMD_RCGR_UPDATE BIT(0)
#define APPS_CMD_RCGR_ROOT_EN BIT(1)

static void qce2204_clk_bcr_update(struct nsscc_qce2204_priv *priv,
				   u32 apps_cmd_rcgr)
{
	u32 val;
	int ret;
	int timeout = 1000;

	qce2204_ahb_update(priv, apps_cmd_rcgr, APPS_CMD_RCGR_UPDATE,
			   APPS_CMD_RCGR_UPDATE);

	while (timeout--) {
		ret = qce2204_ahb_read(priv, apps_cmd_rcgr, &val);
		if (ret)
			break;
		if (!(val & APPS_CMD_RCGR_UPDATE)) {
			qce2204_ahb_update(priv, apps_cmd_rcgr,
					   APPS_CMD_RCGR_ROOT_EN,
					   APPS_CMD_RCGR_ROOT_EN);
			return;
		}
		udelay(100);
	}

	if (ret || timeout <= 0)
		printf("RCG @ %#x stuck at off\n", apps_cmd_rcgr);
}

#define CFG_MASK		0x3FFF
#define CFG_SRC_DIV_MASK	0x1F
#define CFG_SRC_SEL_SHIFT	8
#define CFG_SRC_SEL_MASK	(0x7 << CFG_SRC_SEL_SHIFT)
#define CFG_MODE_SHIFT		12
#define CFG_MODE_MASK		(0x3 << CFG_MODE_SHIFT)
#define CFG_MODE_DUAL_EDGE	(0x2 << CFG_MODE_SHIFT)
#define CFG_HW_CLK_CTRL_MASK	BIT(20)

static void __maybe_unused
qce2204_clk_rcg_set_rate_mnd(struct nsscc_qce2204_priv *priv, u32 cmd_rcgr,
			     int div, int m, int n, int source, u8 mnd_width)
{
	u32 cfg;
	u32 m_val = m;
	u32 n_minus_m = n - m;
	u32 n_val = ~n_minus_m * !!(n);
	u32 d_val = ~(clamp_t(u32, n, m, n_minus_m));
	u32 mask = BIT(mnd_width) - 1;
	u32 reg = cmd_rcgr + QCE2204_CLK_REG_BASE;

	qce2204_ahb_write(priv, reg + RCG_M_REG, m_val & mask);
	qce2204_ahb_write(priv, reg + RCG_N_REG, n_val & mask);
	qce2204_ahb_write(priv, reg + RCG_D_REG, d_val & mask);

	qce2204_ahb_read(priv, reg + RCG_CFG_REG, &cfg);
	cfg &= ~(CFG_SRC_SEL_MASK | CFG_MODE_MASK | CFG_HW_CLK_CTRL_MASK |
		 CFG_SRC_DIV_MASK);
	cfg |= source & CFG_SRC_SEL_MASK;

	if (div)
		cfg |= div & CFG_SRC_DIV_MASK;

	if (n && n != m)
		cfg |= CFG_MODE_DUAL_EDGE;

	qce2204_ahb_write(priv, reg + RCG_CFG_REG, cfg);

	qce2204_clk_bcr_update(priv, reg);
}

static void __maybe_unused
qce2204_clk_rcg_set_rate_v2(struct nsscc_qce2204_priv *priv, u32 cmd_rcgr,
			    u32 div_cdivr, int div, int cdiv, int source)
{
	u32 cfg;
	u32 reg = cmd_rcgr + QCE2204_CLK_REG_BASE;

	qce2204_ahb_read(priv, reg + RCG_CFG_REG, &cfg);
	cfg &= ~CFG_MASK;
	cfg |= source & CFG_CLK_SRC_MASK;

	if (div)
		cfg |= div & CFG_SRC_DIV_MASK;

	qce2204_ahb_write(priv, reg + RCG_CFG_REG, cfg);

	if (div_cdivr)
		qce2204_ahb_write(priv, div_cdivr + QCE2204_CLK_REG_BASE, cdiv);

	qce2204_clk_bcr_update(priv, reg);
}

static int qce2204_enable(struct clk *clk)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(clk->dev);
	const struct gate_clk *gclk;
	u32 reg;

	if (clk->id >= priv->data->num_clks)
		return -EINVAL;

	gclk = &priv->data->clks[clk->id];
	if (!gclk->en_val)
		return 0;

	reg = gclk->reg + QCE2204_CLK_REG_BASE;

	return qce2204_ahb_update(priv, reg, gclk->en_val, gclk->en_val);
}

static int qce2204_disable(struct clk *clk)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(clk->dev);
	const struct gate_clk *gclk;
	u32 reg;

	if (clk->id >= priv->data->num_clks)
		return -EINVAL;

	gclk = &priv->data->clks[clk->id];
	if (!gclk->en_val)
		return 0;

	reg = gclk->reg + QCE2204_CLK_REG_BASE;

	return qce2204_ahb_update(priv, reg, gclk->en_val, 0);
}

static int calc_div_for_nss_port_clk(struct clk *clk, ulong rate,
				     int *div, int *cdiv, int *xgmii_dev)
{
	*xgmii_dev = 0;
	*cdiv = 0;
	*div = 0;
	switch (rate) {
	case CLK_2_5_MHZ:
		*div = 24;
		*cdiv = 9;
		*xgmii_dev = 9;
		break;
	case CLK_12_5_MHZ:
		*cdiv = 3;
		break;
	case CLK_25_MHZ:
		*div = 0x18;
		break;
	case CLK_78_125_MHZ:
		*div = 1;
		*cdiv = 3;
		break;
	case CLK_125_MHZ:
		*div = 4;
		break;
	case CLK_156_25_MHZ:
		*div = 1;
		*cdiv = 1;
		break;
	case CLK_312_5_MHZ:
		*div = 1;
		*xgmii_dev = 3;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static ulong qce2204_set_rate(struct clk *clk, ulong rate)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(clk->dev);
	int ret, div = 0, cdiv = 0, xgmii_dev = 0;

	switch (clk->id) {
	case QCE2204_NSSCC_SWITCH_CORE_CLK:
		qce2204_clk_rcg_set_rate_v2(priv, GCC_SWITCH_CORE_CMD_RCGR, 0, 1, 0,
					    1 << 8);
		break;
	case QCE2204_NSSCC_MAC0_TX_CLK:
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC0_TX_CMD_RCGR, 0, 1, 0,
					    2 << 8);
		break;
	case QCE2204_NSSCC_MAC0_RX_CLK:
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC0_RX_CMD_RCGR, 0, 1, 0,
					    1 << 8);
		break;
	case QCE2204_NSSCC_MAC1_TX_CLK:
	case QCE2204_NSSCC_MAC1_SRDS1_CH0_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC1_TX_CMD_RCGR,
					    GCC_MAC1_TX_DIV_CDIVR, div, cdiv,
					    7 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC1_SRDS1_CH0_XGMII_TX_DIV_CDIVR,
				  xgmii_dev);
		break;
	case QCE2204_NSSCC_MAC2_TX_CLK:
	case QCE2204_NSSCC_MAC2_SRDS1_CH1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC2_TX_CMD_RCGR,
					    GCC_MAC2_TX_DIV_CDIVR, div, cdiv,
					    7 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC2_SRDS1_CH1_XGMII_TX_DIV_CDIVR,
				  xgmii_dev);
		break;
	case QCE2204_NSSCC_MAC3_TX_CLK:
	case QCE2204_NSSCC_MAC3_SRDS1_CH2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC3_TX_CMD_RCGR,
					    GCC_MAC3_TX_DIV_CDIVR, div, cdiv,
					    7 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC3_SRDS1_CH2_XGMII_TX_DIV_CDIVR,
				  xgmii_dev);
		break;
	case QCE2204_NSSCC_MAC4_TX_CLK:
	case QCE2204_NSSCC_MAC4_SRDS1_CH3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC4_TX_CMD_RCGR,
					    GCC_MAC4_TX_DIV_CDIVR, div, cdiv,
					    7 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC4_SRDS1_CH3_XGMII_TX_DIV_CDIVR,
				  xgmii_dev);
		break;

	case QCE2204_NSSCC_MAC1_RX_CLK:
	case QCE2204_NSSCC_MAC1_SRDS1_CH0_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC1_RX_CMD_RCGR,
					    GCC_MAC1_RX_DIV_CDIVR, div, cdiv,
					    6 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC1_SRDS1_CH0_XGMII_RX_DIV_CDIVR,
				  xgmii_dev);
		break;
	case QCE2204_NSSCC_MAC2_RX_CLK:
	case QCE2204_NSSCC_MAC2_SRDS1_CH1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC2_RX_CMD_RCGR,
					    GCC_MAC2_RX_DIV_CDIVR, div, cdiv,
					    6 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC2_SRDS1_CH1_XGMII_RX_DIV_CDIVR,
				  xgmii_dev);
		break;
	case QCE2204_NSSCC_MAC3_RX_CLK:
	case QCE2204_NSSCC_MAC3_SRDS1_CH2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC3_RX_CMD_RCGR,
					    GCC_MAC3_RX_DIV_CDIVR, div, cdiv,
					    6 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC3_SRDS1_CH2_XGMII_RX_DIV_CDIVR,
				  xgmii_dev);
		break;
	case QCE2204_NSSCC_MAC4_RX_CLK:
	case QCE2204_NSSCC_MAC4_SRDS1_CH3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC4_RX_CMD_RCGR,
					    GCC_MAC4_RX_DIV_CDIVR, div, cdiv,
					    6 << 8);
		qce2204_ahb_write(priv,
				  QCE2204_CLK_REG_BASE +
				  GCC_MAC4_SRDS1_CH3_XGMII_RX_DIV_CDIVR,
				  xgmii_dev);
		break;

	case QCE2204_NSSCC_AHB_CLK:
		qce2204_clk_rcg_set_rate_v2(priv, GCC_AHB_CMD_RCGR, 0, 5, 0,
					    2 << 8);
		break;

	case QCE2204_NSSCC_SRDS1_SYS_CLK:
		qce2204_clk_rcg_set_rate_v2(priv, GCC_SYS_CMD_RCGR, 0, 3, 0, 0);
		break;

	/*
	 * MAC5 TX/RX clock rate — used for port 5 USXGMII (external QCA81xx PHY).
	 * Source 7 (TX) / 6 (RX) selects the SRDS path; the SRDS0/SRDS1 MUX_SEL
	 * bits in register 0x300 (set by qce1204_set_srds_mux) determine which
	 * SerDes is actually routed to MAC5.
	 */
	/*
	 * MAC5 TX/RX clock rate — port 5 USXGMII (external QCA81xx PHY).
	 *
	 * Linux kernel reference (nsscc-qce2204.c):
	 *   nsscc_mac5_tx_clk_src: cmd_rcgr=0x210
	 *   nsscc_mac5_tx_div_clk_src: reg=0x218  ← single divider for both GMII and XGMII
	 *   nsscc_mac5_tx_srds0_ch0_xgmii_clk: gate=0x228 (parent = srds0_xgmii_tx_mux_sel
	 *     which selects nsscc_mac5_tx_div_clk_src — NO separate XGMII CDIVR)
	 *
	 * Source 7 (TX) / 6 (RX) selects the SRDS path in the RCG.
	 * The SRDS0/SRDS1 MUX_SEL bits in register 0x300 (set by qce1204_set_srds_mux)
	 * determine which SerDes is actually routed to MAC5.
	 */
	case QCE2204_NSSCC_MAC5_TX_CLK:
	case QCE2204_NSSCC_MAC5_TX_SRDS0_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC5_TX_CMD_RCGR,
					    GCC_MAC5_TX_DIV_CDIVR, div, cdiv,
					    rate == CLK_12_5_MHZ ? 0 : 2 << 8);
		/* No separate XGMII CDIVR for MAC5 SRDS0 — XGMII uses same DIV (0x218) */
		break;

	case QCE2204_NSSCC_MAC5_RX_CLK:
	case QCE2204_NSSCC_MAC5_RX_SRDS0_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv,
						&xgmii_dev);
		qce2204_clk_rcg_set_rate_v2(priv, GCC_MAC5_RX_CMD_RCGR,
					    GCC_MAC5_RX_DIV_CDIVR, div, cdiv,
					    rate == CLK_12_5_MHZ ? 0 : 1 << 8);
		/* No separate XGMII CDIVR for MAC5 SRDS0 — XGMII uses same DIV (0x238) */
		break;

	default:
		return rate;
	}
	return rate;
}

static int nsscc_qce2204_clk_disable(struct clk *clk)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->disable)
		return priv->data->disable(clk);

	return 0;
}

static int nsscc_qce2204_clk_enable(struct clk *clk)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->enable)
		return priv->data->enable(clk);

	return 0;
}

static ulong nsscc_qce2204_clk_set_rate(struct clk *clk, ulong rate)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->set_rate)
		return priv->data->set_rate(clk, rate);

	return 0;
}

static ulong nsscc_qce2204_clk_get_rate(struct clk *clk)
{
	return 0;
}

static const struct clk_ops nsscc_qce2204_clk_ops = {
	.disable = nsscc_qce2204_clk_disable,
	.enable = nsscc_qce2204_clk_enable,
	.get_rate = nsscc_qce2204_clk_get_rate,
	.set_rate = nsscc_qce2204_clk_set_rate,
};

static int nsscc_qce2204_clk_probe(struct udevice *dev)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(dev);
	struct udevice *parent = dev_get_parent(dev);

	if (!parent) {
		dev_err(dev, "No parent device found\n");
		return -ENODEV;
	}

	priv->data = (struct msm_clk_data *)dev_get_driver_data(parent);
	if (!priv->data) {
		dev_err(dev, "No driver data from parent\n");
		return -EINVAL;
	}

	priv->mdio_bus = dev_get_parent(parent);
	if (!priv->mdio_bus) {
		dev_err(dev, "No MDIO bus found\n");
		return -ENODEV;
	}

	if (dev_read_u32(parent, "reg", &priv->phy_addr)) {
		dev_err(dev, "Missing reg property in parent node\n");
		return -EINVAL;
	}

	if (priv->phy_addr != 6) {
		dev_warn(dev, "Unexpected PHY address: %d (expected 6)\n",
			 priv->phy_addr);
	}

	dev_info(dev, "Clock controller probed: MDIO addr=%d\n", priv->phy_addr);
	return 0;
}

U_BOOT_DRIVER(nsscc_qce2204_clk) = {
	.name = "nsscc_qce2204_clk",
	.id = UCLASS_CLK,
	.ops = &nsscc_qce2204_clk_ops,
	.probe = nsscc_qce2204_clk_probe,
	.priv_auto = sizeof(struct nsscc_qce2204_priv),
	.flags = DM_FLAG_PRE_RELOC,
};

static int nsscc_qce2204_rst_set(struct reset_ctl *rst, bool assert)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(rst->dev);
	const struct qcom_reset_map *map;
	u32 reg;

	if (rst->id >= priv->data->num_resets)
		return -EINVAL;

	map = &priv->data->resets[rst->id];
	reg = map->reg + QCE2204_CLK_REG_BASE;

	return qce2204_ahb_update(priv, reg, BIT(map->bit),
				  assert ? BIT(map->bit) : 0);
}

static int nsscc_qce2204_rst_assert(struct reset_ctl *rst)
{
	return nsscc_qce2204_rst_set(rst, true);
}

static int nsscc_qce2204_rst_deassert(struct reset_ctl *rst)
{
	return nsscc_qce2204_rst_set(rst, false);
}

static const struct reset_ops nsscc_qce2204_rst_ops = {
	.rst_assert = nsscc_qce2204_rst_assert,
	.rst_deassert = nsscc_qce2204_rst_deassert,
};

static int nsscc_qce2204_rst_probe(struct udevice *dev)
{
	struct nsscc_qce2204_priv *priv = dev_get_priv(dev);
	struct udevice *parent = dev_get_parent(dev);

	if (!parent) {
		dev_err(dev, "No parent device found\n");
		return -ENODEV;
	}

	priv->data = (struct msm_clk_data *)dev_get_driver_data(parent);
	if (!priv->data) {
		dev_err(dev, "No driver data from parent\n");
		return -EINVAL;
	}

	priv->mdio_bus = dev_get_parent(parent);
	if (!priv->mdio_bus) {
		dev_err(dev, "No MDIO bus found\n");
		return -ENODEV;
	}

	if (dev_read_u32(parent, "reg", &priv->phy_addr)) {
		dev_err(dev, "Missing reg property in parent node\n");
		return -EINVAL;
	}

	dev_info(dev, "Reset controller probed: MDIO addr=%d\n", priv->phy_addr);
	return 0;
}

U_BOOT_DRIVER(nsscc_qce2204_reset) = {
	.name = "nsscc_qce2204_reset",
	.id = UCLASS_RESET,
	.ops = &nsscc_qce2204_rst_ops,
	.probe = nsscc_qce2204_rst_probe,
	.priv_auto = sizeof(struct nsscc_qce2204_priv),
	.flags = DM_FLAG_PRE_RELOC,
};

static const struct gate_clk nsscc_qce2204_clks[] = {
	GATE_CLK(QCE2204_NSSCC_SWITCH_CORE_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_SWITCH_CORE_CLK,                        0x10,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_IPE_CLK,                         0x18,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_BTQ_CLK,                         0x20,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_CFG_CLK,                         0x28,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_MAC0_CLK,                        0x30,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_MAC1_CLK,                        0x38,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_MAC2_CLK,                        0x40,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_MAC3_CLK,                        0x48,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_MAC4_CLK,                        0x50,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_MAC5_CLK,                        0x58,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_XGMAC0_PTP_REF_CLK,                     0x60,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_XGMAC1_PTP_REF_CLK,                     0x68,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_APB_BRIDGE_CLK,                         0x70,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC0_TX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC0_TX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC0_TX_CLK,                            0x88,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC0_TX_SRDS1_CLK,                      0x8c,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC0_RX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC0_RX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC0_RX_CLK,                            0xa4,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC0_RX_SRDS1_CLK,                      0xac,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC1_TX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC1_TX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC1_SRDS1_CH0_XGMII_RX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC1_SRDS1_CH0_RX_CLK,                  0xcc,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC1_TX_CLK,                            0xd0,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC1_GEPHY0_TX_CLK,                     0xd4,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC0_1_SRDS1_CH0_XGMII_RX_CLK,          0xd8,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC1_RX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC1_RX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC1_SRDS1_CH0_XGMII_TX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC1_SRDS1_CH0_TX_CLK,                  0xf8,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC1_RX_CLK,                            0xfc,   BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC1_GEPHY0_RX_CLK,                     0x104,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC0_1_SRDS1_CH0_XGMII_TX_CLK,          0x108,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_TX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC2_TX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC2_SRDS1_CH1_XGMII_RX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC2_SRDS1_CH1_RX_CLK,                  0x128,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_TX_CLK,                            0x12c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_GEPHY1_TX_CLK,                     0x130,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_SRDS1_CH1_XGMII_RX_CLK,            0x134,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_RX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC2_RX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC2_SRDS1_CH1_XGMII_TX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC2_SRDS1_CH1_TX_CLK,                  0x150,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_RX_CLK,                            0x154,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_GEPHY1_RX_CLK,                     0x158,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC2_SRDS1_CH1_XGMII_TX_CLK,            0x15c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_TX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC3_TX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC3_SRDS1_CH2_XGMII_RX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC3_SRDS1_CH2_RX_CLK,                  0x17c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_TX_CLK,                            0x180,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_GEPHY2_TX_CLK,                     0x184,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_SRDS1_CH2_XGMII_RX_CLK,            0x188,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_RX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC3_RX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC3_SRDS1_CH2_XGMII_TX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC3_SRDS1_CH2_TX_CLK,                  0x1a8,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_RX_CLK,                            0x1ac,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_GEPHY2_RX_CLK,                     0x1b0,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC3_SRDS1_CH2_XGMII_TX_CLK,            0x1b4,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_TX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC4_TX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC4_SRDS1_CH3_XGMII_RX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC4_SRDS1_CH3_RX_CLK,                  0x1d0,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_TX_CLK,                            0x1d4,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_GEPHY3_TX_CLK,                     0x1d8,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_SRDS1_CH3_XGMII_RX_CLK,            0x1dc,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_RX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC4_RX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC4_SRDS1_CH3_XGMII_TX_DIV_CLK_SRC,    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC4_SRDS1_CH3_TX_CLK,                  0x1fc,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_RX_CLK,                            0x200,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_GEPHY3_RX_CLK,                     0x204,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC4_SRDS1_CH3_XGMII_TX_CLK,            0x208,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC5_TX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC5_TX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC5_TX_CLK,                            0x220,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC5_TX_SRDS0_CLK,                      0x224,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC5_TX_SRDS0_CH0_XGMII_CLK,            0x228,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC5_RX_CLK_SRC,                        0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC5_RX_DIV_CLK_SRC,                    0,      0),
	GATE_CLK(QCE2204_NSSCC_MAC5_RX_CLK,                            0x23c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC5_RX_SRDS0_CLK,                      0x244,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MAC5_RX_SRDS0_CH0_XGMII_CLK,            0x248,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_AHB_CLK_SRC,                            0,      0),
	GATE_CLK(QCE2204_NSSCC_AHB_CLK,                                0x258,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SEC_CTRL_AHB_CLK,                       0x25c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_TLMM_CLK,                               0x260,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_TLMM_AHB_CLK,                           0x264,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_CNOC_AHB_CLK,                           0x268,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MDIO_AHB_CLK,                           0x26c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_MDIO_MASTER_AHB_CLK,                    0x270,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_TSENS_AHB_CLK,                          0x274,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SYS_CLK_SRC,                            0,      0),
	GATE_CLK(QCE2204_NSSCC_SRDS0_SYS_CLK,                          0x280,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SRDS1_SYS_CLK,                          0x284,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_GEPHY0_SYS_CLK,                         0x288,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_GEPHY1_SYS_CLK,                         0x28c,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_GEPHY2_SYS_CLK,                         0x290,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_GEPHY3_SYS_CLK,                         0x294,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_KDF_CLK,                                0x298,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_TSENS_EXT_CLK,                          0x2a0,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SEC_CTRL_CLK_SRC,                       0,      0),
	GATE_CLK(QCE2204_NSSCC_SEC_CTRL_CLK,                           0x2b0,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SEC_CTRL_SENSE_CLK,                     0x2b8,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SLEEP_CLK_SRC,                          0,      0),
	GATE_CLK(QCE2204_NSSCC_SLEEP_DIV_CLK_SRC,                      0,      0),
	GATE_CLK(QCE2204_NSSCC_SLEEP_CLK,                              0x2cc,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_TS_SLEEP_CLK,                           0x2d0,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_DEBUG_DIV_CLK_SRC,                      0,      0),
	GATE_CLK(QCE2204_NSSCC_DEBUG_CLK,                              0x2e0,  BIT(0)),
	GATE_CLK(QCE2204_NSSCC_SWITCH_CORE_DIV_CLK_SRC,		       0,  0),
	GATE_CLK(QCE2204_NSSCC_SRDS0_RX_MUX_SEL,                       0x300,  BIT(19)),
	GATE_CLK(QCE2204_NSSCC_SRDS0_TX_MUX_SEL,                       0x300,  BIT(18)),
	GATE_CLK(QCE2204_NSSCC_SRDS0_XGMII_RX_MUX_SEL,                 0x300,  BIT(17)),
	GATE_CLK(QCE2204_NSSCC_SRDS0_XGMII_TX_MUX_SEL,                 0x300,  BIT(16)),
	GATE_CLK(QCE2204_NSSCC_SRDS1_RX_MUX_SEL,                       0x300,  BIT(15)),
	GATE_CLK(QCE2204_NSSCC_SRDS1_TX_MUX_SEL,                       0x300,  BIT(14)),
	GATE_CLK(QCE2204_NSSCC_SRDS1_XGMII_RX_MUX_SEL,                 0x300,  BIT(13)),
	GATE_CLK(QCE2204_NSSCC_SRDS1_XGMII_TX_MUX_SEL,                 0x300,  BIT(12)),
};

static const struct qcom_reset_map nsscc_qce2204_resets_map[] = {
	[QCE2204_NSSCC_SWITCH_CORE_BCR] = { 0x314, 0 },
	[QCE2204_NSSCC_SWITCH_CORE_ARES] = { 0x10, 2 },
	[QCE2204_NSSCC_SWITCH_IPE_ARES] = { 0x18, 2 },
	[QCE2204_NSSCC_SWITCH_BTQ_ARES] = { 0x20, 2 },
	[QCE2204_NSSCC_SWITCH_CFG_ARES] = { 0x28, 2 },
	[QCE2204_NSSCC_SWITCH_MAC0_ARES] = { 0x30, 2 },
	[QCE2204_NSSCC_SWITCH_MAC1_ARES] = { 0x38, 2 },
	[QCE2204_NSSCC_SWITCH_MAC2_ARES] = { 0x40, 2 },
	[QCE2204_NSSCC_SWITCH_MAC3_ARES] = { 0x48, 2 },
	[QCE2204_NSSCC_SWITCH_MAC4_ARES] = { 0x50, 2 },
	[QCE2204_NSSCC_SWITCH_MAC5_ARES] = { 0x58, 2 },
	[QCE2204_NSSCC_XGMAC0_PTP_REF_ARES] = { 0x60, 2 },
	[QCE2204_NSSCC_XGMAC1_PTP_REF_ARES] = { 0x68, 2 },
	[QCE2204_NSSCC_APB_BRIDGE_ARES] = { 0x70, 2 },
	[QCE2204_NSSCC_MAC0_TX_ARES] = { 0x88, 2 },
	[QCE2204_NSSCC_MAC0_TX_SRDS1_ARES] = { 0x8c, 2 },
	[QCE2204_NSSCC_MAC0_RX_ARES] = { 0xa4, 2 },
	[QCE2204_NSSCC_MAC0_RX_SRDS1_ARES] = { 0xa8, 2 },
	[QCE2204_NSSCC_MAC1_SRDS1_CH0_RX_ARES] = { 0xcc, 2 },
	[QCE2204_NSSCC_MAC1_TX_ARES] = { 0xd0, 2 },
	[QCE2204_NSSCC_MAC1_GEPHY0_TX_ARES] = { 0xd4, 2 },
	[QCE2204_NSSCC_MAC0_1_SRDS1_CH0_XGMII_RX_ARES] = { 0xd8, 2 },
	[QCE2204_NSSCC_MAC1_SRDS1_CH0_TX_ARES] = { 0xf8, 2 },
	[QCE2204_NSSCC_MAC1_RX_ARES] = { 0xfc, 2 },
	[QCE2204_NSSCC_MAC1_GEPHY0_RX_ARES] = { 0x104, 2 },
	[QCE2204_NSSCC_MAC0_1_SRDS1_CH0_XGMII_TX_ARES] = { 0x108, 2 },
	[QCE2204_NSSCC_MAC2_SRDS1_CH1_RX_ARES] = { 0x128, 2 },
	[QCE2204_NSSCC_MAC2_TX_ARES] = { 0x12c, 2 },
	[QCE2204_NSSCC_MAC2_GEPHY1_TX_ARES] = { 0x130, 2 },
	[QCE2204_NSSCC_MAC2_SRDS1_CH1_XGMII_RX_ARES] = { 0x134, 2 },
	[QCE2204_NSSCC_MAC2_SRDS1_CH1_TX_ARES] = { 0x150, 2 },
	[QCE2204_NSSCC_MAC2_RX_ARES] = { 0x154, 2 },
	[QCE2204_NSSCC_MAC2_GEPHY1_RX_ARES] = { 0x158, 2 },
	[QCE2204_NSSCC_MAC2_SRDS1_CH1_XGMII_TX_ARES] = { 0x15c, 2 },
	[QCE2204_NSSCC_MAC3_SRDS1_CH2_RX_ARES] = { 0x17c, 2 },
	[QCE2204_NSSCC_MAC3_TX_ARES] = { 0x180, 2 },
	[QCE2204_NSSCC_MAC3_GEPHY2_TX_ARES] = { 0x184, 2 },
	[QCE2204_NSSCC_MAC3_SRDS1_CH2_XGMII_RX_ARES] = { 0x188, 2 },
	[QCE2204_NSSCC_MAC3_SRDS1_CH2_TX_ARES] = { 0x1a8, 2 },
	[QCE2204_NSSCC_MAC3_RX_ARES] = { 0x1ac, 2 },
	[QCE2204_NSSCC_MAC3_GEPHY2_RX_ARES] = { 0x1b0, 2 },
	[QCE2204_NSSCC_MAC3_SRDS1_CH2_XGMII_TX_ARES] = { 0x1b4, 2 },
	[QCE2204_NSSCC_MAC4_SRDS1_CH3_RX_ARES] = { 0x1d0, 2 },
	[QCE2204_NSSCC_MAC4_TX_ARES] = { 0x1d4, 2 },
	[QCE2204_NSSCC_MAC4_GEPHY3_TX_ARES] = { 0x1d8, 2 },
	[QCE2204_NSSCC_MAC4_SRDS1_CH3_XGMII_RX_ARES] = { 0x1dc, 2 },
	[QCE2204_NSSCC_MAC4_SRDS1_CH3_TX_ARES] = { 0x1fc, 2 },
	[QCE2204_NSSCC_MAC4_RX_ARES] = { 0x200, 2 },
	[QCE2204_NSSCC_MAC4_GEPHY3_RX_ARES] = { 0x204, 2 },
	[QCE2204_NSSCC_MAC4_SRDS1_CH3_XGMII_TX_ARES] = { 0x208, 2 },
	[QCE2204_NSSCC_MAC5_TX_ARES] = { 0x220, 2 },
	[QCE2204_NSSCC_MAC5_TX_SRDS0_ARES] = { 0x224, 2 },
	[QCE2204_NSSCC_MAC5_TX_SRDS0_CH0_XGMII_ARES] = { 0x228, 2 },
	[QCE2204_NSSCC_MAC5_RX_ARES] = { 0x23c, 2 },
	[QCE2204_NSSCC_MAC5_RX_SRDS0_ARES] = { 0x244, 2 },
	[QCE2204_NSSCC_MAC5_RX_SRDS0_CH0_XGMII_ARES] = { 0x248, 2 },
	[QCE2204_NSSCC_AHB_ARES] = { 0x258, 2 },
	[QCE2204_NSSCC_SEC_CTRL_AHB_ARES] = { 0x25c, 2 },
	[QCE2204_NSSCC_TLMM_ARES] = { 0x260, 2 },
	[QCE2204_NSSCC_TLMM_AHB_ARES] = { 0x264, 2 },
	[QCE2204_NSSCC_CNOC_AHB_ARES] = { 0x268, 2 },
	[QCE2204_NSSCC_MDIO_AHB_ARES] = { 0x26c, 2 },
	[QCE2204_NSSCC_MDIO_MASTER_AHB_ARES] = { 0x270, 2 },
	[QCE2204_NSSCC_TSENS_AHB_ARES] = { 0x274, 2 },
	[QCE2204_NSSCC_SRDS0_SYS_ARES] = { 0x280, 2 },
	[QCE2204_NSSCC_SRDS1_SYS_ARES] = { 0x284, 2 },
	[QCE2204_NSSCC_GEPHY0_SYS_ARES] = { 0x288, 2 },
	[QCE2204_NSSCC_GEPHY1_SYS_ARES] = { 0x28c, 2 },
	[QCE2204_NSSCC_GEPHY2_SYS_ARES] = { 0x290, 2 },
	[QCE2204_NSSCC_GEPHY3_SYS_ARES] = { 0x294, 2 },
	[QCE2204_NSSCC_KDF_ARES] = { 0x298, 2 },
	[QCE2204_NSSCC_TSENS_EXT_ARES] = { 0x2a0, 2 },
	[QCE2204_NSSCC_SEC_CTRL_ARES] = { 0x2b0, 2 },
	[QCE2204_NSSCC_SEC_CTRL_SENSE_ARES] = { 0x2b8, 2 },
	[QCE2204_NSSCC_SLEEP_ARES] = { 0x2cc, 2 },
	[QCE2204_NSSCC_TS_SLEEP_ARES] = { 0x2d0, 2 },
	[QCE2204_NSSCC_DEBUG_ARES] = { 0x2e0, 2 },
	[QCE2204_NSSCC_GEPHY0_ARES] = { 0x304, 0 },
	[QCE2204_NSSCC_GEPHY1_ARES] = { 0x304, 1 },
	[QCE2204_NSSCC_GEPHY2_ARES] = { 0x304, 2 },
	[QCE2204_NSSCC_GEPHY3_ARES] = { 0x304, 3 },
	[QCE2204_NSSCC_DSP_ARES] = { 0x304, 4 },
	[QCE2204_NSSCC_GLOBAL_ARES] = { 0x308, 0 },
	[QCE2204_NSSCC_SRDS0_XPCS_ARES] = { 0x30c, 0 },
	[QCE2204_NSSCC_SRDS1_XPCS_ARES] = { 0x310, 0 },
};

static struct msm_clk_data nsscc_qce2204_data = {
	.clks = nsscc_qce2204_clks,
	.num_clks = ARRAY_SIZE(nsscc_qce2204_clks),
	.resets = nsscc_qce2204_resets_map,
	.num_resets = ARRAY_SIZE(nsscc_qce2204_resets_map),
	.enable = qce2204_enable,
	.disable = qce2204_disable,
	.set_rate = qce2204_set_rate,
};

static int nsscc_qce2204_bind(struct udevice *parent)
{
	struct msm_clk_data *data = (struct msm_clk_data *)dev_get_driver_data(parent);
	struct udevice *clkdev = NULL, *rstdev = NULL;
	ofnode node = dev_ofnode(parent);
	int ret;

	dev_info(parent, "Binding QCE2204 clock controller\n");

	ret = device_bind(parent, DM_DRIVER_GET(nsscc_qce2204_clk),
			  "nsscc_qce2204_clk", NULL, node, &clkdev);
	if (ret) {
		dev_err(parent, "Failed to bind clock driver to node: %d\n", ret);
		return ret;
	}
	dev_info(parent, "Clock driver bound to DT node successfully\n");

	if (data->resets) {
		ret = device_bind(parent, DM_DRIVER_GET(nsscc_qce2204_reset),
				  "nsscc_qce2204_reset", NULL, node, &rstdev);
		if (ret) {
			device_unbind(clkdev);
			dev_err(parent, "Failed to bind reset driver to node: %d\n", ret);
			return ret;
		}
		dev_info(parent, "Reset driver bound to DT node successfully\n");
	}

	dev_info(parent, "QCE2204 clock controller bind complete\n");
	return 0;
}

static const struct udevice_id nsscc_qce2204_ids[] = {
	{ .compatible = "qcom,qce2204-nsscc", .data = (ulong)&nsscc_qce2204_data },
	{ }
};

U_BOOT_DRIVER(nsscc_qce2204) = {
	.name = "nsscc_qce2204",
	.id = UCLASS_NOP,
	.of_match = nsscc_qce2204_ids,
	.bind = nsscc_qce2204_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
