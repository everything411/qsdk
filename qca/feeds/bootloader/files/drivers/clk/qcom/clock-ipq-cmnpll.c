// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * CMN PLL block expects the reference clock from on-board Wi-Fi block,
 * and supplies fixed rate clocks as output to the networking hardware
 * blocks and to GCC. The networking related blocks include PPE (packet
 * process engine), the externally connected PHY or switch devices, and
 * the PCS.
 *
 * On the IPQ9574 SoC, there are three clocks with 50 MHZ and one clock
 * with 25 MHZ which are output from the CMN PLL to Ethernet PHY (or switch),
 * and one clock with 353 MHZ to PPE. The other fixed rate output clocks
 * are supplied to GCC (24 MHZ as XO and 32 KHZ as sleep clock), and to PCS
 * with 31.25 MHZ.
 *
 * On the IPQ5424 SoC, there is an output clock from CMN PLL to PPE at 375 MHZ,
 * and an output clock to NSS (network subsystem) at 300 MHZ. The other output
 * clocks from CMN PLL on IPQ5424 are the same as IPQ9574.
 *
 * On the IPQ5332 SoC, the CMN PLL provides a single 50 MHZ clock output to
 * the Ethernet PHY (or switch) via the UNIPHY (PCS). It also supplies a 200
 * MHZ clock to the PPE. The remaining fixed-rate clocks to the GCC and PCS
 * are the same as those in the IPQ9574 SoC.
 *
 *               +---------+
 *               |   GCC   |
 *               +--+---+--+
 *           AHB CLK|   |SYS CLK
 *                  V   V
 *          +-------+---+------+
 *          |                  +-------------> eth0-50mhz
 * REF CLK  |     IPQ9574      |
 * -------->+                  +-------------> eth1-50mhz
 *          |  CMN PLL block   |
 *          |                  +-------------> eth2-50mhz
 *          |                  |
 *          +----+----+----+---+-------------> eth-25mhz
 *               |    |    |
 *               V    V    V
 *              GCC  PCS  NSS/PPE
 */

#include <linux/types.h>
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/bug.h>
#include <linux/math64.h>
#include <dm/device-internal.h>

/* CMN PLL Common Register Offsets */
#define CMN_PLL_REFCLK_SRC_SELECTION		0x28
#define CMN_PLL_REFCLK_SRC_DIV			GENMASK(9, 8)

#define CMN_PLL_LOCKED				0x64
#define CMN_PLL_CLKS_LOCKED			BIT(8)

#define CMN_PLL_NSS_PPE_FREQ_CTRL		0x98
#define CMN_PLL_NSS_CLK_SEL			GENMASK(13, 8)
#define CMN_PLL_PPE_CLK_SEL			GENMASK(5, 0)
/* CMNPLL divider for NSS/PPE: 6-bit field, valid range 8-63. */
#define CMN_PLL_NSS_PPE_DIV_MIN			8
#define CMN_PLL_NSS_PPE_DIV_MAX			63

#define CMN_PLL_PCS_CLK_CTRL			0x41c
#define CMN_PLL_PCS2_CLK_DIVSEL			GENMASK(8, 7)
#define CMN_PLL_PCS1_CLK_DIVSEL			GENMASK(6, 5)
#define CMN_PLL_PCS0_CLK_DIVSEL			GENMASK(4, 3)
#define CMN_PLL_PCS2_CLK_EN			BIT(2)
#define CMN_PLL_PCS1_CLK_EN			BIT(1)
#define CMN_PLL_PCS0_CLK_EN			BIT(0)

#define CMN_PLL_PON_CONFIG			0x42c
#define CMN_PLL_GEPHY_312P5M_125M_SEL		BIT(10)
#define CMN_PLL_PON_MODE_SEL			BIT(9)
#define CMN_PLL_PON_EN				BIT(8)
#define CMN_PLL_PON_DIV_CTRL			GENMASK(7, 0)

#define CMN_PLL_POWER_ON_AND_RESET		0x780
#define CMN_ANA_EN_SW_RSTN			BIT(6)

#define CMN_PLL_REFCLK_CONFIG			0x784
#define CMN_PLL_REFCLK_EXTERNAL			BIT(9)
#define CMN_PLL_REFCLK_DIV			GENMASK(8, 4)
#define CMN_PLL_REFCLK_INDEX			GENMASK(3, 0)

#define CMN_PLL_CTRL				0x78c
#define CMN_PLL_CTRL_LOCK_DETECT_EN		BIT(15)

#define CMN_PLL_DIVIDER_CTRL			0x794
#define CMN_PLL_DIVIDER_CTRL_FACTOR		GENMASK(9, 0)

#define CMN_PLL_OUTPUT_RELATED_1		0x79c
#define CLK25M_EN_BIT				15
#define CLK50M_EN_BIT3_BIT			14
#define CLK250M_EN_BIT				13
#define CLK31P25M_EN_BIT			12
#define CLK50M_EN_BIT				11
#define CLK50M_EN_BIT2_BIT			10

#define CMN_PLL_OUTPUT_RELATED_2		0x7a0
#define CMN_PLL_OUTPUT_MUX_SEL			BIT(4)

/* Clock types */
enum ipq_cmnpll_clk_type {
	CLK_TYPE_FIXED,		/* Fixed rate clock */
	CLK_TYPE_GATE,		/* Gated clock */
	CLK_TYPE_DIVIDER,	/* Configurable divider clock */
	CLK_TYPE_PON,		/* PON reference clock */
	CLK_TYPE_PCS,		/* PCS clock */
	CLK_TYPE_EPHY_RAW,	/* EPHY raw clock (125 MHz / 312.5 MHz) */
};

/**
 * struct ipq_cmnpll_clk_desc - Clock descriptor
 * @id: Clock ID
 * @name: Clock name
 * @type: Clock type
 * @rate: Fixed rate (for FIXED type)
 * @gate_bit: Gate bit position (for GATE type)
 */
struct ipq_cmnpll_clk_desc {
	unsigned int id;
	const char *name;
	enum ipq_cmnpll_clk_type type;
	unsigned long rate;
	int gate_bit;
};

/**
 * struct ipq_cmnpll_ops - Platform-specific operations
 * @get_rate: Get clock rate
 * @set_rate: Set clock rate
 * @enable: Enable clock
 * @disable: Disable clock
 */
struct ipq_cmnpll_ops {
	ulong (*get_rate)(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc);
	ulong (*set_rate)(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc, ulong rate);
	int (*enable)(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc);
	int (*disable)(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc);
};

/**
 * struct ipq_cmnpll_data - Platform-specific data
 * @clk_descs: Array of clock descriptors
 * @num_clks: Number of clocks
 * @ops: Platform-specific operations
 */
struct ipq_cmnpll_data {
	const struct ipq_cmnpll_clk_desc *clk_descs;
	unsigned int num_clks;
	const struct ipq_cmnpll_ops *ops;
};

/**
 * struct ipq_cmnpll_priv - Driver private data
 * @base: Register base address
 * @pll_rate: PLL rate (u64 to avoid 32-bit overflow on 32-bit platforms,
 *            since CMN PLL rate ~12 GHz exceeds unsigned long on 32-bit)
 * @data: Platform-specific data
 */
struct ipq_cmnpll_priv {
	void __iomem *base;
	u64 pll_rate;
	const struct ipq_cmnpll_data *data;
};

/* ========== Generic CMN PLL Functions ========== */

/**
 * ipq_cmnpll_find_freq_index - Find reference clock index
 */
static int ipq_cmnpll_find_freq_index(unsigned long parent_rate)
{
	switch (parent_rate) {
	case 25000000:
		return 3;
	case 31250000:
		return 4;
	case 40000000:
		return 6;
	case 48000000:
	case 96000000:
		/*
		 * Parent clock rate 48 MHZ and 96 MHZ take the same value
		 * of reference clock index. 96 MHZ needs the source clock
		 * divider to be programmed as 2.
		 */
		return 7;
	case 50000000:
		return 8;
	default:
		return -EINVAL;
	}
}

/**
 * ipq_cmnpll_recalc_rate - Calculate CMN PLL output rate
 *
 * Returns the actual CMN PLL rate as u64. On 32-bit platforms the result
 * (~12 GHz) exceeds unsigned long, so we always compute it as u64 to
 * avoid truncation in callers.
 */
static u64 ipq_cmnpll_recalc_rate(struct ipq_cmnpll_priv *priv,
				   unsigned long parent_rate)
{
	u32 val, factor, ref_div;

	/*
	 * The value of CMN_PLL_DIVIDER_CTRL_FACTOR is automatically adjusted
	 * by HW according to the parent clock rate.
	 */
	val = readl(priv->base + CMN_PLL_DIVIDER_CTRL);
	factor = FIELD_GET(CMN_PLL_DIVIDER_CTRL_FACTOR, val);
	if (factor == 0)
		factor = 1;

	val = readl(priv->base + CMN_PLL_REFCLK_CONFIG);
	ref_div = FIELD_GET(CMN_PLL_REFCLK_DIV, val);
	if (ref_div == 0)
		ref_div = 1;

	return div_u64((u64)parent_rate * 2 * factor, ref_div);
}

/**
 * ipq_cmnpll_ana_soft_reset - Perform analog soft reset and wait for PLL lock
 *
 * Resets the CMN PLL analog block and waits for the output clocks to lock.
 * This must be called after any clock rate change to ensure the new
 * configuration takes effect.
 */
static int ipq_cmnpll_ana_soft_reset(struct ipq_cmnpll_priv *priv)
{
	u32 val;
	int timeout;

	val = readl(priv->base + CMN_PLL_POWER_ON_AND_RESET);
	val &= ~CMN_ANA_EN_SW_RSTN;
	writel(val, priv->base + CMN_PLL_POWER_ON_AND_RESET);

	udelay(1200);

	val = readl(priv->base + CMN_PLL_POWER_ON_AND_RESET);
	val |= CMN_ANA_EN_SW_RSTN;
	writel(val, priv->base + CMN_PLL_POWER_ON_AND_RESET);

	/* Stability check of CMN PLL output clocks */
	timeout = 100000;
	do {
		val = readl(priv->base + CMN_PLL_LOCKED);
		if (val & CMN_PLL_CLKS_LOCKED)
			break;
		udelay(1);
	} while (--timeout > 0);

	if (timeout <= 0) {
		pr_err("CMN PLL failed to lock\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/**
 * ipq_cmnpll_init - Initialize CMN PLL
 */
static int ipq_cmnpll_init(struct ipq_cmnpll_priv *priv, unsigned long parent_rate)
{
	int index;
	u32 val;

	index = ipq_cmnpll_find_freq_index(parent_rate);
	if (index < 0) {
		pr_err("Unsupported parent rate: %lu\n", parent_rate);
		return index;
	}

	/* Configure reference clock index */
	val = readl(priv->base + CMN_PLL_REFCLK_CONFIG);
	val &= ~CMN_PLL_REFCLK_INDEX;
	val |= FIELD_PREP(CMN_PLL_REFCLK_INDEX, index);
	writel(val, priv->base + CMN_PLL_REFCLK_CONFIG);

	/*
	 * Update the source clock rate selection and source clock
	 * divider as 2 when the parent clock rate is 96 MHZ.
	 */
	if (parent_rate == 96000000) {
		val = readl(priv->base + CMN_PLL_REFCLK_CONFIG);
		val &= ~CMN_PLL_REFCLK_DIV;
		val |= FIELD_PREP(CMN_PLL_REFCLK_DIV, 2);
		writel(val, priv->base + CMN_PLL_REFCLK_CONFIG);

		val = readl(priv->base + CMN_PLL_REFCLK_SRC_SELECTION);
		val &= ~CMN_PLL_REFCLK_SRC_DIV;
		val |= FIELD_PREP(CMN_PLL_REFCLK_SRC_DIV, 0);
		writel(val, priv->base + CMN_PLL_REFCLK_SRC_SELECTION);
	}

	/* Enable PLL locked detect */
	val = readl(priv->base + CMN_PLL_CTRL);
	val |= CMN_PLL_CTRL_LOCK_DETECT_EN;
	writel(val, priv->base + CMN_PLL_CTRL);

	/*
	 * Reset the CMN PLL block to ensure the updated configurations
	 * take effect.
	 */
	if (ipq_cmnpll_ana_soft_reset(priv)) {
		pr_err("Failed to initialize CMN PLL\n");
		return -ETIMEDOUT;
	}

	priv->pll_rate = ipq_cmnpll_recalc_rate(priv, parent_rate);
	return 0;
}

/**
 * ipq_cmnpll_gate_enable - Enable gate clock
 */
static int ipq_cmnpll_gate_enable(struct ipq_cmnpll_priv *priv, int bit)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
	val |= BIT(bit);
	writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_1);

	return 0;
}

/**
 * ipq_cmnpll_gate_disable - Disable gate clock
 */
static void ipq_cmnpll_gate_disable(struct ipq_cmnpll_priv *priv, int bit)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
	val &= ~BIT(bit);
	writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_1);
}

/**
 * ipq_cmnpll_nss_set_rate - Set NSS clock rate
 *
 * The NSS clock is derived from CMN PLL rate / 2, then divided by
 * a configurable 6-bit divider (8-63). Uses u64 arithmetic to avoid
 * 32-bit overflow on 32-bit platforms where pll_rate (~12 GHz) exceeds
 * unsigned long.
 */
static int ipq_cmnpll_nss_set_rate(struct ipq_cmnpll_priv *priv, unsigned long rate)
{
	unsigned long div;
	u32 val;

	div = (unsigned long)div_u64(priv->pll_rate + (u64)rate, 2ULL * rate);

	if (div < CMN_PLL_NSS_PPE_DIV_MIN || div > CMN_PLL_NSS_PPE_DIV_MAX) {
		pr_err("NSS divider %lu out of range\n", div);
		return -EINVAL;
	}

	val = readl(priv->base + CMN_PLL_NSS_PPE_FREQ_CTRL);
	val &= ~CMN_PLL_NSS_CLK_SEL;
	val |= FIELD_PREP(CMN_PLL_NSS_CLK_SEL, div);
	writel(val, priv->base + CMN_PLL_NSS_PPE_FREQ_CTRL);

	return ipq_cmnpll_ana_soft_reset(priv);
}

/**
 * ipq_cmnpll_ppe_set_rate - Set PPE clock rate
 *
 * The PPE clock is derived from CMN PLL rate / 2, then divided by
 * a configurable 6-bit divider (8-63). Uses u64 arithmetic to avoid
 * 32-bit overflow on 32-bit platforms where pll_rate (~12 GHz) exceeds
 * unsigned long.
 */
static int ipq_cmnpll_ppe_set_rate(struct ipq_cmnpll_priv *priv, unsigned long rate)
{
	unsigned long div;
	u32 val;

	div = (unsigned long)div_u64(priv->pll_rate + (u64)rate, 2ULL * rate);

	if (div < CMN_PLL_NSS_PPE_DIV_MIN || div > CMN_PLL_NSS_PPE_DIV_MAX) {
		pr_err("PPE divider %lu out of range\n", div);
		return -EINVAL;
	}

	val = readl(priv->base + CMN_PLL_NSS_PPE_FREQ_CTRL);
	val &= ~CMN_PLL_PPE_CLK_SEL;
	val |= FIELD_PREP(CMN_PLL_PPE_CLK_SEL, div);
	writel(val, priv->base + CMN_PLL_NSS_PPE_FREQ_CTRL);

	return ipq_cmnpll_ana_soft_reset(priv);
}

/**
 * ipq_cmnpll_pon_set_rate - Set PON reference clock rate
 *
 * The PON refclk is derived from CMN PLL rate / 2, then divided by
 * a configurable 8-bit divider (1-255). Uses u64 arithmetic to avoid
 * 32-bit overflow on 32-bit platforms where pll_rate (~12 GHz) exceeds
 * unsigned long.
 */
static int ipq_cmnpll_pon_set_rate(struct ipq_cmnpll_priv *priv, unsigned long rate)
{
	unsigned long div;
	u32 val;

	/* UNIPHY fixed rate */
	if (rate == 31250000) {
		val = readl(priv->base + CMN_PLL_PON_CONFIG);
		val &= ~CMN_PLL_PON_MODE_SEL;
		writel(val, priv->base + CMN_PLL_PON_CONFIG);
		return 0;
	}

	/* PON mode with divider */
	div = (unsigned long)div_u64(priv->pll_rate + (u64)rate, 2ULL * rate);

	/* Constrain divider to 8-bit register width: [1, 255] */
	if (div == 0 || div > 255) {
		pr_err("PON divider %lu out of range\n", div);
		return -EINVAL;
	}

	/* Switch to PON mode (bit 9 = 1) */
	val = readl(priv->base + CMN_PLL_PON_CONFIG);
	val |= CMN_PLL_PON_MODE_SEL;
	writel(val, priv->base + CMN_PLL_PON_CONFIG);

	/* Update divider field */
	val = readl(priv->base + CMN_PLL_PON_CONFIG);
	val &= ~CMN_PLL_PON_DIV_CTRL;
	val |= FIELD_PREP(CMN_PLL_PON_DIV_CTRL, div);
	writel(val, priv->base + CMN_PLL_PON_CONFIG);

	return ipq_cmnpll_ana_soft_reset(priv);
}

/**
 * ipq_cmnpll_pon_enable - Enable PON reference clock
 */
static int ipq_cmnpll_pon_enable(struct ipq_cmnpll_priv *priv)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_PON_CONFIG);
	val |= CMN_PLL_PON_EN;
	writel(val, priv->base + CMN_PLL_PON_CONFIG);

	return 0;
}

/**
 * ipq_cmnpll_pon_disable - Disable PON reference clock
 */
static void ipq_cmnpll_pon_disable(struct ipq_cmnpll_priv *priv)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_PON_CONFIG);
	val &= ~CMN_PLL_PON_EN;
	writel(val, priv->base + CMN_PLL_PON_CONFIG);
}

/**
 * ipq_cmnpll_ephy_raw_get_rate - Get EPHY raw clock rate
 *
 * The output clock rate is determined by bit 10 of CMN_PLL_PON_CONFIG.
 * 0: 125 MHz (for link speeds other than 2.5G)
 * 1: 312.5 MHz (for 2.5G link speed)
 */
static unsigned long ipq_cmnpll_ephy_raw_get_rate(struct ipq_cmnpll_priv *priv)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_PON_CONFIG);
	if (val & CMN_PLL_GEPHY_312P5M_125M_SEL)
		return 312500000UL;

	return 125000000UL;
}

/**
 * ipq_cmnpll_ephy_raw_set_rate - Set EPHY raw clock rate
 *
 * Configures the EPHY raw clock to either 125 MHz or 312.5 MHz by
 * setting/clearing bit 10 of CMN_PLL_PON_CONFIG, then performs an
 * analog soft reset to apply the change.
 */
static int ipq_cmnpll_ephy_raw_set_rate(struct ipq_cmnpll_priv *priv, unsigned long rate)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_PON_CONFIG);
	if (rate == 312500000UL)
		val |= CMN_PLL_GEPHY_312P5M_125M_SEL;
	else
		val &= ~CMN_PLL_GEPHY_312P5M_125M_SEL;
	writel(val, priv->base + CMN_PLL_PON_CONFIG);

	return ipq_cmnpll_ana_soft_reset(priv);
}

/* ========== IPQ5210-Specific Implementation ========== */

/* IPQ5210 Clock IDs */
enum ipq5210_cmnpll_clk_id {
	IPQ5210_XO_24MHZ_CLK = 0,
	IPQ5210_SLEEP_32KHZ_CLK,
	IPQ5210_PCS_31P25MHZ_CLK,
	IPQ5210_ETH0_50MHZ_CLK,
	IPQ5210_ETH1_50MHZ_CLK,
	IPQ5210_ETH2_50MHZ_CLK,
	IPQ5210_EPHY_50MHZ_CLK,
	IPQ5210_ETH_25MHZ_CLK,
	IPQ5210_NSS_CLK,
	IPQ5210_PPE_CLK,
	IPQ5210_PON_REFCLK,
	IPQ5210_EPHY_RAW_CLK,
	IPQ5210_CMN_PLL_CLK,
};

/* IPQ9650 Clock IDs */
enum ipq9650_cmnpll_clk_id {
	IPQ9650_XO_24MHZ_CLK = 0,
	IPQ9650_SLEEP_32KHZ_CLK,
	IPQ9650_NSS_CLK,
	IPQ9650_PPE_CLK,
	IPQ9650_PCS0_CLK,
	IPQ9650_PCS1_CLK,
	IPQ9650_PCS2_CLK,
	IPQ9650_ETH_PON_CLK,
	IPQ9650_ETH0_50MHZ_CLK,
	IPQ9650_ETH1_50MHZ_CLK,
	IPQ9650_ETH2_50MHZ_CLK,
	IPQ9650_ETH_25MHZ_CLK,
	IPQ9650_CMN_PLL_CLK,
};

/* IPQ5210 Clock Descriptors */
static const struct ipq_cmnpll_clk_desc ipq5210_clk_descs[] = {
	{ IPQ5210_XO_24MHZ_CLK, "xo-24mhz", CLK_TYPE_FIXED, 24000000, -1 },
	{ IPQ5210_SLEEP_32KHZ_CLK, "sleep-32khz", CLK_TYPE_FIXED, 32000, -1 },
	{ IPQ5210_PCS_31P25MHZ_CLK, "pcs-31p25mhz", CLK_TYPE_GATE, 31250000, CLK31P25M_EN_BIT },
	{ IPQ5210_ETH0_50MHZ_CLK, "eth0-50mhz", CLK_TYPE_GATE, 50000000, CLK50M_EN_BIT },
	{ IPQ5210_ETH1_50MHZ_CLK, "eth1-50mhz", CLK_TYPE_GATE, 50000000, CLK50M_EN_BIT2_BIT },
	{ IPQ5210_ETH2_50MHZ_CLK, "eth2-50mhz", CLK_TYPE_GATE, 50000000, CLK50M_EN_BIT3_BIT },
	{ IPQ5210_EPHY_50MHZ_CLK, "ephy-50mhz", CLK_TYPE_GATE, 50000000, CLK250M_EN_BIT },
	{ IPQ5210_ETH_25MHZ_CLK, "eth-25mhz", CLK_TYPE_GATE, 25000000, CLK25M_EN_BIT },
	{ IPQ5210_NSS_CLK, "nss", CLK_TYPE_DIVIDER, 0, -1 },
	{ IPQ5210_PPE_CLK, "ppe", CLK_TYPE_DIVIDER, 0, -1 },
	{ IPQ5210_PON_REFCLK, "pon", CLK_TYPE_PON, 0, -1 },
	{ IPQ5210_EPHY_RAW_CLK, "ephy-raw", CLK_TYPE_EPHY_RAW, 0, -1 },
	{ IPQ5210_CMN_PLL_CLK, "cmn-pll", CLK_TYPE_FIXED, 0, -1 },
};

/* IPQ5210 get_rate operation */
static ulong ipq5210_get_rate(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);

	if (desc->type == CLK_TYPE_FIXED || desc->type == CLK_TYPE_GATE)
		return desc->rate;

	if (clk->id == IPQ5210_CMN_PLL_CLK)
		return (ulong)priv->pll_rate;

	if (clk->id == IPQ5210_EPHY_RAW_CLK)
		return ipq_cmnpll_ephy_raw_get_rate(priv);

	return 0;
}

/* IPQ5210 set_rate operation */
static ulong ipq5210_set_rate(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc, ulong rate)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);
	int ret;

	switch (desc->type) {
	case CLK_TYPE_DIVIDER:
		if (clk->id == IPQ5210_NSS_CLK)
			ret = ipq_cmnpll_nss_set_rate(priv, rate);
		else if (clk->id == IPQ5210_PPE_CLK)
			ret = ipq_cmnpll_ppe_set_rate(priv, rate);
		else
			return -EOPNOTSUPP;
		break;
	case CLK_TYPE_PON:
		ret = ipq_cmnpll_pon_set_rate(priv, rate);
		break;
	case CLK_TYPE_EPHY_RAW:
		ret = ipq_cmnpll_ephy_raw_set_rate(priv, rate);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return ret ? ret : rate;
}

/* IPQ5210 enable operation */
static int ipq5210_enable(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);

	switch (desc->type) {
	case CLK_TYPE_GATE:
		return ipq_cmnpll_gate_enable(priv, desc->gate_bit);
	case CLK_TYPE_PON:
		return ipq_cmnpll_pon_enable(priv);
	default:
		return 0;
	}
}

/* IPQ5210 disable operation */
static int ipq5210_disable(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);

	switch (desc->type) {
	case CLK_TYPE_GATE:
		ipq_cmnpll_gate_disable(priv, desc->gate_bit);
		break;
	case CLK_TYPE_PON:
		ipq_cmnpll_pon_disable(priv);
		break;
	default:
		break;
	}

	return 0;
}

/* IPQ5210 operations */
static const struct ipq_cmnpll_ops ipq5210_ops = {
	.get_rate = ipq5210_get_rate,
	.set_rate = ipq5210_set_rate,
	.enable = ipq5210_enable,
	.disable = ipq5210_disable,
};

/* IPQ5210 platform data */
static const struct ipq_cmnpll_data ipq5210_data = {
	.clk_descs = ipq5210_clk_descs,
	.num_clks = ARRAY_SIZE(ipq5210_clk_descs),
	.ops = &ipq5210_ops,
};

/* ========== IPQ9650-Specific Implementation ========== */

/* PCS frequency table - maps frequency to divsel value */
struct pcs_freq_map {
	unsigned long freq;
	u32 divsel;
};

/*
 * PCS clock operations for IPQ9650.
 * The PCS clocks are controlled via the UPHY_REFCLK_CTRL register (0x41C).
 * Each of the three PCS clocks (PCS0/1/2) can be independently enabled and
 * configured to output one of four frequencies: 46.875, 93.75, 31.25, or 62.5 MHz.
 *
 * Hardware divsel encoding:
 *   0b00 (0) -> 46.875 MHz
 *   0b01 (1) -> 93.75 MHz
 *   0b10 (2) -> 31.25 MHz
 *   0b11 (3) -> 62.5 MHz
 */
static const struct pcs_freq_map pcs_freq_table[] = {
	{ 31250000UL, 2 },  /* 31.25 MHz */
	{ 46875000UL, 0 },  /* 46.875 MHz */
	{ 62500000UL, 3 },  /* 62.5 MHz */
	{ 93750000UL, 1 },  /* 93.75 MHz */
};

/**
 * ipq9650_pcs_parse_index - Extract PCS index from clock ID
 */
static int ipq9650_pcs_parse_index(unsigned long clk_id)
{
	if (clk_id == IPQ9650_PCS0_CLK)
		return 0;
	else if (clk_id == IPQ9650_PCS1_CLK)
		return 1;
	else if (clk_id == IPQ9650_PCS2_CLK)
		return 2;
	return -EINVAL;
}

/**
 * ipq9650_pcs_get_enable_bit - Get PCS enable bit
 */
static u32 ipq9650_pcs_get_enable_bit(int idx)
{
	static const u32 enable_bits[] = {
		CMN_PLL_PCS0_CLK_EN,
		CMN_PLL_PCS1_CLK_EN,
		CMN_PLL_PCS2_CLK_EN,
	};
	return enable_bits[idx];
}

/**
 * ipq9650_pcs_get_divsel_mask - Get PCS divsel mask
 */
static u32 ipq9650_pcs_get_divsel_mask(int idx)
{
	static const u32 divsel_masks[] = {
		CMN_PLL_PCS0_CLK_DIVSEL,
		CMN_PLL_PCS1_CLK_DIVSEL,
		CMN_PLL_PCS2_CLK_DIVSEL,
	};
	return divsel_masks[idx];
}

/**
 * ipq9650_pcs_freq_to_divsel - Convert frequency to divsel value
 */
static int ipq9650_pcs_freq_to_divsel(unsigned long rate, u32 *divsel)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcs_freq_table); i++) {
		if (pcs_freq_table[i].freq == rate) {
			*divsel = pcs_freq_table[i].divsel;
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * ipq9650_pcs_divsel_to_freq - Convert divsel to frequency
 */
static unsigned long ipq9650_pcs_divsel_to_freq(u32 divsel)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcs_freq_table); i++) {
		if (pcs_freq_table[i].divsel == divsel)
			return pcs_freq_table[i].freq;
	}
	return 0;
}

/**
 * ipq9650_pcs_get_rate - Get PCS clock rate
 */
static unsigned long ipq9650_pcs_get_rate(struct ipq_cmnpll_priv *priv, unsigned long clk_id)
{
	int idx;
	u32 val, divsel, divsel_mask;

	idx = ipq9650_pcs_parse_index(clk_id);
	if (idx < 0)
		return 0;

	divsel_mask = ipq9650_pcs_get_divsel_mask(idx);
	val = readl(priv->base + CMN_PLL_PCS_CLK_CTRL);
	divsel = (val & divsel_mask) >> __ffs(divsel_mask);

	return ipq9650_pcs_divsel_to_freq(divsel);
}

/**
 * ipq9650_pcs_set_rate - Set PCS clock rate
 */
static int ipq9650_pcs_set_rate(struct ipq_cmnpll_priv *priv, unsigned long clk_id,
				unsigned long rate)
{
	int idx;
	u32 divsel, divsel_mask, val;
	int ret;

	idx = ipq9650_pcs_parse_index(clk_id);
	if (idx < 0)
		return idx;

	ret = ipq9650_pcs_freq_to_divsel(rate, &divsel);
	if (ret)
		return ret;

	divsel_mask = ipq9650_pcs_get_divsel_mask(idx);
	val = readl(priv->base + CMN_PLL_PCS_CLK_CTRL);
	val &= ~divsel_mask;
	val |= (divsel << __ffs(divsel_mask));
	writel(val, priv->base + CMN_PLL_PCS_CLK_CTRL);

	return ipq_cmnpll_ana_soft_reset(priv);
}

/**
 * ipq9650_pcs_enable - Enable PCS clock
 */
static int ipq9650_pcs_enable(struct ipq_cmnpll_priv *priv, unsigned long clk_id)
{
	int idx;
	u32 enable_bit, val;

	idx = ipq9650_pcs_parse_index(clk_id);
	if (idx < 0)
		return idx;

	enable_bit = ipq9650_pcs_get_enable_bit(idx);
	val = readl(priv->base + CMN_PLL_PCS_CLK_CTRL);
	val |= enable_bit;
	writel(val, priv->base + CMN_PLL_PCS_CLK_CTRL);

	return 0;
}

/**
 * ipq9650_pcs_disable - Disable PCS clock
 */
static void ipq9650_pcs_disable(struct ipq_cmnpll_priv *priv, unsigned long clk_id)
{
	int idx;
	u32 enable_bit, val;

	idx = ipq9650_pcs_parse_index(clk_id);
	if (idx < 0)
		return;

	enable_bit = ipq9650_pcs_get_enable_bit(idx);
	val = readl(priv->base + CMN_PLL_PCS_CLK_CTRL);
	val &= ~enable_bit;
	writel(val, priv->base + CMN_PLL_PCS_CLK_CTRL);
}

/**
 * ipq9650_eth_pon_get_rate - Get ETH-PON clock rate
 *
 * The output clock rate is determined by bit 4 of CMN_PLL_OUTPUT_RELATED_2.
 * 0: 25 MHz
 * 1: 31.25 MHz
 */
static unsigned long ipq9650_eth_pon_get_rate(struct ipq_cmnpll_priv *priv)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_2);
	if (val & CMN_PLL_OUTPUT_MUX_SEL)
		return 31250000UL;
	return 25000000UL;
}

/**
 * ipq9650_eth_pon_is_enabled - Check if ETH-PON clock output is enabled
 */
static bool ipq9650_eth_pon_is_enabled(struct ipq_cmnpll_priv *priv)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
	return !!(val & (BIT(CLK25M_EN_BIT) | BIT(CLK31P25M_EN_BIT)));
}

/**
 * ipq9650_eth_pon_set_rate - Set ETH-PON clock rate
 *
 * Disables the clock output if currently enabled, switches the mux to
 * select the requested rate (25 MHz or 31.25 MHz), re-enables the output
 * if it was enabled, then performs an analog soft reset.
 */
static int ipq9650_eth_pon_set_rate(struct ipq_cmnpll_priv *priv, unsigned long rate)
{
	u32 val;
	bool enabled;

	if (rate != 25000000 && rate != 31250000)
		return -EINVAL;

	/* Check if clock is currently enabled */
	enabled = ipq9650_eth_pon_is_enabled(priv);

	/* Disable clock output if enabled */
	if (enabled) {
		val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
		val &= ~(BIT(CLK31P25M_EN_BIT) | BIT(CLK25M_EN_BIT));
		writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_1);
	}

	/* Set the clock rate via mux select */
	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_2);
	if (rate == 25000000)
		val &= ~CMN_PLL_OUTPUT_MUX_SEL;
	else
		val |= CMN_PLL_OUTPUT_MUX_SEL;
	writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_2);

	/* Re-enable clock output if it was enabled */
	if (enabled) {
		val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
		if (rate == 25000000)
			val |= BIT(CLK25M_EN_BIT);
		else
			val |= BIT(CLK31P25M_EN_BIT);
		writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_1);
	}

	return ipq_cmnpll_ana_soft_reset(priv);
}

/**
 * ipq9650_eth_pon_enable - Enable ETH-PON clock
 */
static int ipq9650_eth_pon_enable(struct ipq_cmnpll_priv *priv)
{
	unsigned long rate;
	u32 val, enable_bit;

	rate = ipq9650_eth_pon_get_rate(priv);

	if (rate == 25000000) {
		enable_bit = CLK25M_EN_BIT;
		val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_2);
		val &= ~CMN_PLL_OUTPUT_MUX_SEL;
		writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_2);
	} else {
		enable_bit = CLK31P25M_EN_BIT;
		val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_2);
		val |= CMN_PLL_OUTPUT_MUX_SEL;
		writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_2);
	}

	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
	val |= BIT(enable_bit);
	writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_1);

	return 0;
}

/**
 * ipq9650_eth_pon_disable - Disable ETH-PON clock
 */
static void ipq9650_eth_pon_disable(struct ipq_cmnpll_priv *priv)
{
	u32 val;

	val = readl(priv->base + CMN_PLL_OUTPUT_RELATED_1);
	val &= ~(BIT(CLK25M_EN_BIT) | BIT(CLK31P25M_EN_BIT));
	writel(val, priv->base + CMN_PLL_OUTPUT_RELATED_1);
}

/* IPQ9650 Clock Descriptors */
static const struct ipq_cmnpll_clk_desc ipq9650_clk_descs[] = {
	{ IPQ9650_XO_24MHZ_CLK, "xo-24mhz", CLK_TYPE_FIXED, 24000000, -1 },
	{ IPQ9650_SLEEP_32KHZ_CLK, "sleep-32khz", CLK_TYPE_FIXED, 32000, -1 },
	{ IPQ9650_NSS_CLK, "nss", CLK_TYPE_DIVIDER, 0, -1 },
	{ IPQ9650_PPE_CLK, "ppe", CLK_TYPE_DIVIDER, 0, -1 },
	{ IPQ9650_PCS0_CLK, "pcs0", CLK_TYPE_PCS, 0, -1 },
	{ IPQ9650_PCS1_CLK, "pcs1", CLK_TYPE_PCS, 0, -1 },
	{ IPQ9650_PCS2_CLK, "pcs2", CLK_TYPE_PCS, 0, -1 },
	{ IPQ9650_ETH_PON_CLK, "eth-pon", CLK_TYPE_PON, 0, -1 },
	{ IPQ9650_ETH0_50MHZ_CLK, "eth0-50mhz", CLK_TYPE_GATE, 50000000, CLK50M_EN_BIT },
	{ IPQ9650_ETH1_50MHZ_CLK, "eth1-50mhz", CLK_TYPE_GATE, 50000000, CLK50M_EN_BIT2_BIT },
	{ IPQ9650_ETH2_50MHZ_CLK, "eth2-50mhz", CLK_TYPE_GATE, 50000000, CLK50M_EN_BIT3_BIT },
	{ IPQ9650_ETH_25MHZ_CLK, "eth-25mhz", CLK_TYPE_GATE, 25000000, CLK25M_EN_BIT },
	{ IPQ9650_CMN_PLL_CLK, "cmn-pll", CLK_TYPE_FIXED, 0, -1 },
};

/* IPQ9650 get_rate operation */
static ulong ipq9650_get_rate(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);

	if (desc->type == CLK_TYPE_FIXED || desc->type == CLK_TYPE_GATE)
		return desc->rate;

	if (clk->id == IPQ9650_CMN_PLL_CLK)
		return (ulong)priv->pll_rate;

	if (desc->type == CLK_TYPE_PCS)
		return ipq9650_pcs_get_rate(priv, clk->id);

	if (clk->id == IPQ9650_ETH_PON_CLK)
		return ipq9650_eth_pon_get_rate(priv);

	return 0;
}

/* IPQ9650 set_rate operation */
static ulong ipq9650_set_rate(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc, ulong rate)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);
	int ret;

	switch (desc->type) {
	case CLK_TYPE_DIVIDER:
		if (clk->id == IPQ9650_NSS_CLK)
			ret = ipq_cmnpll_nss_set_rate(priv, rate);
		else if (clk->id == IPQ9650_PPE_CLK)
			ret = ipq_cmnpll_ppe_set_rate(priv, rate);
		else
			return -EOPNOTSUPP;
		break;
	case CLK_TYPE_PON:
		if (clk->id == IPQ9650_ETH_PON_CLK)
			ret = ipq9650_eth_pon_set_rate(priv, rate);
		else
			ret = ipq_cmnpll_pon_set_rate(priv, rate);
		break;
	case CLK_TYPE_PCS:
		ret = ipq9650_pcs_set_rate(priv, clk->id, rate);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return ret ? ret : rate;
}

/* IPQ9650 enable operation */
static int ipq9650_enable(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);

	switch (desc->type) {
	case CLK_TYPE_GATE:
		return ipq_cmnpll_gate_enable(priv, desc->gate_bit);
	case CLK_TYPE_PON:
		if (clk->id == IPQ9650_ETH_PON_CLK)
			return ipq9650_eth_pon_enable(priv);
		return ipq_cmnpll_pon_enable(priv);
	case CLK_TYPE_PCS:
		return ipq9650_pcs_enable(priv, clk->id);
	default:
		return 0;
	}
}

/* IPQ9650 disable operation */
static int ipq9650_disable(struct clk *clk, const struct ipq_cmnpll_clk_desc *desc)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);

	switch (desc->type) {
	case CLK_TYPE_GATE:
		ipq_cmnpll_gate_disable(priv, desc->gate_bit);
		break;
	case CLK_TYPE_PON:
		if (clk->id == IPQ9650_ETH_PON_CLK)
			ipq9650_eth_pon_disable(priv);
		else
			ipq_cmnpll_pon_disable(priv);
		break;
	case CLK_TYPE_PCS:
		ipq9650_pcs_disable(priv, clk->id);
		break;
	default:
		break;
	}

	return 0;
}

/* IPQ9650 operations */
static const struct ipq_cmnpll_ops ipq9650_ops = {
	.get_rate = ipq9650_get_rate,
	.set_rate = ipq9650_set_rate,
	.enable = ipq9650_enable,
	.disable = ipq9650_disable,
};

/* IPQ9650 platform data */
static const struct ipq_cmnpll_data ipq9650_data = {
	.clk_descs = ipq9650_clk_descs,
	.num_clks = ARRAY_SIZE(ipq9650_clk_descs),
	.ops = &ipq9650_ops,
};

/* ========== Generic Driver Implementation ========== */

static const struct ipq_cmnpll_clk_desc *ipq_cmnpll_get_desc(struct ipq_cmnpll_priv *priv,
							     unsigned long id)
{
	int i;

	for (i = 0; i < priv->data->num_clks; i++) {
		if (priv->data->clk_descs[i].id == id)
			return &priv->data->clk_descs[i];
	}

	return NULL;
}

static ulong ipq_cmnpll_clk_get_rate(struct clk *clk)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);
	const struct ipq_cmnpll_clk_desc *desc;

	desc = ipq_cmnpll_get_desc(priv, clk->id);
	if (!desc)
		return 0;

	if (priv->data->ops->get_rate)
		return priv->data->ops->get_rate(clk, desc);

	return 0;
}

static ulong ipq_cmnpll_clk_set_rate(struct clk *clk, ulong rate)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);
	const struct ipq_cmnpll_clk_desc *desc;

	desc = ipq_cmnpll_get_desc(priv, clk->id);
	if (!desc)
		return -EINVAL;

	if (priv->data->ops->set_rate)
		return priv->data->ops->set_rate(clk, desc, rate);

	return -EOPNOTSUPP;
}

static int ipq_cmnpll_clk_enable(struct clk *clk)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);
	const struct ipq_cmnpll_clk_desc *desc;

	desc = ipq_cmnpll_get_desc(priv, clk->id);
	if (!desc)
		return -EINVAL;

	if (priv->data->ops->enable)
		return priv->data->ops->enable(clk, desc);

	return 0;
}

static int ipq_cmnpll_clk_disable(struct clk *clk)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(clk->dev);
	const struct ipq_cmnpll_clk_desc *desc;

	desc = ipq_cmnpll_get_desc(priv, clk->id);
	if (!desc)
		return -EINVAL;

	if (priv->data->ops->disable)
		return priv->data->ops->disable(clk, desc);

	return 0;
}

static int ipq_cmnpll_probe(struct udevice *dev)
{
	struct ipq_cmnpll_priv *priv = dev_get_priv(dev);
	struct clk parent_clk;
	unsigned long parent_rate;
	int ret;

	priv->data = (const struct ipq_cmnpll_data *)dev_get_driver_data(dev);
	if (!priv->data) {
		pr_err("No platform data\n");
		return -EINVAL;
	}

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base) {
		pr_err("Failed to get CMN PLL base address\n");
		return -EINVAL;
	}

	ret = clk_get_by_index(dev, 0, &parent_clk);
	if (ret) {
		pr_err("Failed to get parent clock: %d\n", ret);
		return ret;
	}

	parent_rate = clk_get_rate(&parent_clk);
	if (!parent_rate) {
		pr_err("Invalid parent clock rate\n");
		return -EINVAL;
	}

	ret = ipq_cmnpll_init(priv, parent_rate);
	if (ret) {
		pr_err("Failed to initialize CMN PLL: %d\n", ret);
		return ret;
	}

	pr_debug("IPQ CMN PLL initialized: parent=%lu Hz, pll=%llu Hz\n",
		 parent_rate, priv->pll_rate);

	return 0;
}

static const struct clk_ops ipq_cmnpll_clk_ops = {
	.get_rate = ipq_cmnpll_clk_get_rate,
	.set_rate = ipq_cmnpll_clk_set_rate,
	.enable = ipq_cmnpll_clk_enable,
	.disable = ipq_cmnpll_clk_disable,
};

static const struct udevice_id ipq_cmnpll_ids[] = {
	{ .compatible = "qcom,ipq5210-cmn-pll", .data = (ulong)&ipq5210_data },
	{ .compatible = "qcom,ipq9650-cmn-pll", .data = (ulong)&ipq9650_data },
	{ }
};

U_BOOT_DRIVER(ipq_cmnpll) = {
	.name = "ipq_cmnpll",
	.id = UCLASS_CLK,
	.of_match = ipq_cmnpll_ids,
	.ops = &ipq_cmnpll_clk_ops,
	.probe = ipq_cmnpll_probe,
	.priv_auto = sizeof(struct ipq_cmnpll_priv),
	.flags = DM_FLAG_PRE_RELOC,
};
