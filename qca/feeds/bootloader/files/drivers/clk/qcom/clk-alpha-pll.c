// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015, 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <clk-uclass.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <div64.h>
#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <asm/io.h>
#include <clk.h>
#include <clk-uclass.h>
#include "clk-alpha-pll.h"

#define PLL_MODE(p)		((p)->offset + 0x0)
#define PLL_OUTCTRL		BIT(0)
#define PLL_BYPASSNL		BIT(1)
#define PLL_RESET_N		BIT(2)
#define PLL_OFFLINE_REQ		BIT(7)
#define PLL_LOCK_COUNT_SHIFT	8
#define PLL_LOCK_COUNT_MASK	0x3f
#define PLL_BIAS_COUNT_SHIFT	14
#define PLL_BIAS_COUNT_MASK	0x3f
#define PLL_VOTE_FSM_ENA	BIT(20)
#define PLL_FSM_ENA		BIT(20)
#define PLL_VOTE_FSM_RESET	BIT(21)
#define PLL_UPDATE		BIT(22)
#define PLL_UPDATE_BYPASS	BIT(23)
#define PLL_FSM_LEGACY_MODE	BIT(24)
#define PLL_OFFLINE_ACK		BIT(28)
#define ALPHA_PLL_ACK_LATCH	BIT(29)
#define PLL_ACTIVE_FLAG		BIT(30)
#define PLL_LOCK_DET		BIT(31)

#define PLL_L_VAL(p)		((p)->offset + (p)->regs[PLL_OFF_L_VAL])
#define PLL_CAL_L_VAL(p)	((p)->offset + (p)->regs[PLL_OFF_CAL_L_VAL])
#define PLL_ALPHA_VAL(p)	((p)->offset + (p)->regs[PLL_OFF_ALPHA_VAL])
#define PLL_ALPHA_VAL_U(p)	((p)->offset + (p)->regs[PLL_OFF_ALPHA_VAL_U])

#define PLL_USER_CTL(p)		((p)->offset + (p)->regs[PLL_OFF_USER_CTL])
#define PLL_POST_DIV_SHIFT	8
#define PLL_PRE_DIV_SHIFT	12
#define PLL_AUX_POST_DIV_SHIFT	15
#define PLL_POST_DIV_MASK(p)	GENMASK((p)->width - 1, 0)
#define PLL_ALPHA_EN		BIT(24)
#define PLL_ALPHA_MODE		BIT(25)
#define PLL_VCO_SHIFT		20
#define PLL_VCO_MASK		0x3
#define PLL_LOCK_DET_EN		BIT(2)

#define PLL_USER_CTL_U(p)	((p)->offset + (p)->regs[PLL_OFF_USER_CTL_U])
#define PLL_USER_CTL_U1(p)	((p)->offset + (p)->regs[PLL_OFF_USER_CTL_U1])

#define PLL_CONFIG_CTL(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL])
#define PLL_CONFIG_CTL_U(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL_U])
#define PLL_CONFIG_CTL_U1(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL_U1])
#define PLL_CONFIG_CTL_U2(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL_U2])
#define PLL_TEST_CTL(p)		((p)->offset + (p)->regs[PLL_OFF_TEST_CTL])
#define PLL_TEST_CTL_U(p)	((p)->offset + (p)->regs[PLL_OFF_TEST_CTL_U])
#define PLL_TEST_CTL_U1(p)	((p)->offset + (p)->regs[PLL_OFF_TEST_CTL_U1])
#define PLL_TEST_CTL_U2(p)	((p)->offset + (p)->regs[PLL_OFF_TEST_CTL_U2])
#define PLL_STATUS(p)		((p)->offset + (p)->regs[PLL_OFF_STATUS])
#define PLL_OPMODE(p)		((p)->offset + (p)->regs[PLL_OFF_OPMODE])
#define PLL_FRAC(p)		((p)->offset + (p)->regs[PLL_OFF_FRAC])

const u8 clk_alpha_pll_regs[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_DEFAULT] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_USER_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_HUAYRA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL_U] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_BRAMMO] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_FABIA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_USER_CTL_U] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL_U] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
		[PLL_OFF_OPMODE] = 0x2c,
		[PLL_OFF_FRAC] = 0x38,
	},
	[CLK_ALPHA_PLL_TYPE_TRION] = {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_CAL_L_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_USER_CTL_U] = 0x10,
		[PLL_OFF_USER_CTL_U1] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL_U1] = 0x20,
		[PLL_OFF_TEST_CTL] = 0x24,
		[PLL_OFF_TEST_CTL_U] = 0x28,
		[PLL_OFF_TEST_CTL_U1] = 0x2c,
		[PLL_OFF_STATUS] = 0x30,
		[PLL_OFF_OPMODE] = 0x38,
		[PLL_OFF_ALPHA_VAL] = 0x40,
	},
	[CLK_ALPHA_PLL_TYPE_AGERA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_CONFIG_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL_U] = 0x14,
		[PLL_OFF_TEST_CTL] = 0x18,
		[PLL_OFF_TEST_CTL_U] = 0x1c,
		[PLL_OFF_STATUS] = 0x2c,
	},
	[CLK_ALPHA_PLL_TYPE_ZONDA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_CONFIG_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL_U1] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_TEST_CTL_U1] = 0x24,
		[PLL_OFF_OPMODE] = 0x28,
		[PLL_OFF_STATUS] = 0x38,
	},
	[CLK_ALPHA_PLL_TYPE_LUCID_EVO] = {
		[PLL_OFF_OPMODE] = 0x04,
		[PLL_OFF_STATUS] = 0x0c,
		[PLL_OFF_L_VAL] = 0x10,
		[PLL_OFF_ALPHA_VAL] = 0x14,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_USER_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL] = 0x20,
		[PLL_OFF_CONFIG_CTL_U] = 0x24,
		[PLL_OFF_CONFIG_CTL_U1] = 0x28,
		[PLL_OFF_TEST_CTL] = 0x2c,
		[PLL_OFF_TEST_CTL_U] = 0x30,
		[PLL_OFF_TEST_CTL_U1] = 0x34,
	},
	[CLK_ALPHA_PLL_TYPE_LUCID_OLE] = {
		[PLL_OFF_OPMODE] = 0x04,
		[PLL_OFF_STATE] = 0x08,
		[PLL_OFF_STATUS] = 0x0c,
		[PLL_OFF_L_VAL] = 0x10,
		[PLL_OFF_ALPHA_VAL] = 0x14,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_USER_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL] = 0x20,
		[PLL_OFF_CONFIG_CTL_U] = 0x24,
		[PLL_OFF_CONFIG_CTL_U1] = 0x28,
		[PLL_OFF_TEST_CTL] = 0x2c,
		[PLL_OFF_TEST_CTL_U] = 0x30,
		[PLL_OFF_TEST_CTL_U1] = 0x34,
		[PLL_OFF_TEST_CTL_U2] = 0x38,
	},
	[CLK_ALPHA_PLL_TYPE_LUCID_FAST_N6RF] = {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_CAL_L_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_USER_CTL_U] = 0x10,
		[PLL_OFF_USER_CTL_U1] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL_U1] = 0x20,
		[PLL_OFF_TEST_CTL] = 0x24,
		[PLL_OFF_TEST_CTL_U] = 0x28,
		[PLL_OFF_TEST_CTL_U1] = 0x2c,
		[PLL_OFF_STATUS] = 0x30,
		[PLL_OFF_OPMODE] = 0x38,
		[PLL_OFF_ALPHA_VAL] = 0x40,
	},
	[CLK_ALPHA_PLL_TYPE_RIVIAN_EVO] = {
		[PLL_OFF_OPMODE] = 0x04,
		[PLL_OFF_STATUS] = 0x0c,
		[PLL_OFF_L_VAL] = 0x10,
		[PLL_OFF_USER_CTL] = 0x14,
		[PLL_OFF_USER_CTL_U] = 0x18,
		[PLL_OFF_CONFIG_CTL] = 0x1c,
		[PLL_OFF_CONFIG_CTL_U] = 0x20,
		[PLL_OFF_CONFIG_CTL_U1] = 0x24,
		[PLL_OFF_TEST_CTL] = 0x28,
		[PLL_OFF_TEST_CTL_U] = 0x2c,
	},
	[CLK_ALPHA_PLL_TYPE_DEFAULT_EVO] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_TEST_CTL] = 0x10,
		[PLL_OFF_TEST_CTL_U] = 0x14,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_USER_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_BRAMMO_EVO] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_TEST_CTL] = 0x10,
		[PLL_OFF_TEST_CTL_U] = 0x14,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL] = 0x1C,
		[PLL_OFF_STATUS] = 0x20,
	},
	[CLK_ALPHA_PLL_TYPE_STROMER] = {
		[PLL_OFF_L_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL] = 0x10,
		[PLL_OFF_ALPHA_VAL_U] = 0x14,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_USER_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL] = 0x20,
		[PLL_OFF_TEST_CTL] = 0x30,
		[PLL_OFF_TEST_CTL_U] = 0x34,
		[PLL_OFF_STATUS] = 0x28,
	},
	[CLK_ALPHA_PLL_TYPE_STROMER_PLUS] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_USER_CTL] = 0x08,
		[PLL_OFF_USER_CTL_U] = 0x0c,
		[PLL_OFF_CONFIG_CTL] = 0x10,
		[PLL_OFF_TEST_CTL] = 0x14,
		[PLL_OFF_TEST_CTL_U] = 0x18,
		[PLL_OFF_STATUS] = 0x1c,
		[PLL_OFF_ALPHA_VAL] = 0x24,
		[PLL_OFF_ALPHA_VAL_U] = 0x28,
	},
	[CLK_ALPHA_PLL_TYPE_ZONDA_OLE] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_USER_CTL_U] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL_U] = 0x18,
		[PLL_OFF_CONFIG_CTL_U1] = 0x1c,
		[PLL_OFF_CONFIG_CTL_U2] = 0x20,
		[PLL_OFF_TEST_CTL] = 0x24,
		[PLL_OFF_TEST_CTL_U] = 0x28,
		[PLL_OFF_TEST_CTL_U1] = 0x2c,
		[PLL_OFF_OPMODE] = 0x30,
		[PLL_OFF_STATUS] = 0x3c,
	},
	[CLK_ALPHA_PLL_TYPE_NSS_HUAYRA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_TEST_CTL] = 0x0c,
		[PLL_OFF_TEST_CTL_U] = 0x10,
		[PLL_OFF_USER_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL_U] = 0x1c,
		[PLL_OFF_STATUS] = 0x20,
	},

};

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH	40
#define ALPHA_REG_16BIT_WIDTH	16
#define ALPHA_BITWIDTH		32U
#define ALPHA_SHIFT(w)		min(w, ALPHA_BITWIDTH)

#define	ALPHA_PLL_STATUS_REG_SHIFT	8

#define PLL_HUAYRA_M_WIDTH		8
#define PLL_HUAYRA_M_SHIFT		8
#define PLL_HUAYRA_M_MASK		0xff
#define PLL_HUAYRA_N_SHIFT		0
#define PLL_HUAYRA_N_MASK		0xff
#define PLL_HUAYRA_ALPHA_WIDTH		16

#define PLL_STANDBY		0x0
#define PLL_RUN			0x1
#define PLL_OUT_MASK		0x7
#define PLL_RATE_MARGIN		500

/*
 * TRION PLL specific settings and offsets
 */
#define TRION_PLL_CAL_VAL	0x44
#define TRION_PCAL_DONE		BIT(26)

/*
 * LUCID PLL specific settings and offsets
 */
#define LUCID_PCAL_DONE		BIT(27)

/*
 * LUCID 5LPE PLL specific settings and offsets
 */
#define LUCID_5LPE_PCAL_DONE		BIT(11)
#define LUCID_5LPE_ALPHA_PLL_ACK_LATCH	BIT(13)
#define LUCID_5LPE_PLL_LATCH_INPUT	BIT(14)
#define LUCID_5LPE_ENABLE_VOTE_RUN	BIT(21)

/*
 * LUCID EVO PLL specific settings and offsets
 */
#define LUCID_EVO_PCAL_NOT_DONE		BIT(8)
#define LUCID_EVO_ENABLE_VOTE_RUN	BIT(25)
#define LUCID_EVO_PLL_L_VAL_MASK	GENMASK(15, 0)
#define LUCID_EVO_PLL_CAL_L_VAL_SHIFT	16

/*
 * LUCID FAST N6RF PLL specific settings and offsets
 */
#define LUCID_FASTN6RF_PLL_CAL_L_VAL		0x37
#define LUCID_FASTN6RF_POST_DIV_EVEN_MASK	GENMASK(11, 8)
#define LUCID_FASTN6RF_POST_DIV_EVEN_SHIFT	8
#define LUCID_FASTN6RF_POST_DIV_ODD_MASK	GENMASK(15, 12)
#define LUCID_FASTN6RF_POST_DIV_ODD_SHIFT	12
#define LUCID_FASTN6RF_PRE_DIV_MASK		GENMASK(18, 16)
#define LUCID_FASTN6RF_PRE_DIV_SHIFT		16
#define LUCID_FASTN6RF_FRAC_FORMAT_SEL		BIT(15)
#define LUCID_FASTN6RF_FSM_LEGACY_MODE		BIT(24)

/*
 * ZONDA PLL specific
 */
#define ZONDA_PLL_OUT_MASK		0xf
#define ZONDA_STAY_IN_CFA		BIT(16)
#define ZONDA_PLL_FREQ_LOCK_DET		BIT(29)

#define pll_alpha_width(p)					\
		((PLL_ALPHA_VAL_U(p) - PLL_ALPHA_VAL(p) == 4) ?	\
				 ALPHA_REG_BITWIDTH : ALPHA_REG_16BIT_WIDTH)

#define pll_has_64bit_config(p)	((PLL_CONFIG_CTL_U(p) - PLL_CONFIG_CTL(p)) == 4)

/**
 * wait_for_pll() - Wait for the PLL to be enabled/disabled
 * @pll: Pointer to struct clk_alpha_pll
 * @mask: Mask of bits to check for
 * @inverse: Check for the inverse of mask
 * @action: String to print in case of timeout
 *
 * Return: 0 on success, negative errno otherwise
 */
static int wait_for_pll(struct clk_alpha_pll *pll, u32 mask, bool inverse,
			const char *action)
{
	u32 val;
	int count;

	if (!pll || !action) {
		pr_err("pll wait status: invalid arguments\n");
		return -EINVAL;
	}

	for (count = 200; count > 0; count--) {
		val = readl(PLL_MODE(pll));

		if (inverse && !(val & mask))
			return 0;
		else if ((val & mask) == mask)
			return 0;

		udelay(1);
	}

	printf("PLL failed to %s! PLL_MODE 0x%x\n", action, val);
	return -ETIMEDOUT;
}

#define wait_for_pll_enable_active(pll) \
	wait_for_pll(pll, PLL_ACTIVE_FLAG, 0, "enable")

#define wait_for_pll_enable_lock(pll) \
	wait_for_pll(pll, PLL_LOCK_DET, 0, "enable")

#define wait_for_zonda_pll_freq_lock(pll) \
	wait_for_pll(pll, ZONDA_PLL_FREQ_LOCK_DET, 0, "freq enable")

#define wait_for_pll_disable(pll) \
	wait_for_pll(pll, PLL_ACTIVE_FLAG, 1, "disable")

#define wait_for_pll_offline(pll) \
	wait_for_pll(pll, PLL_OFFLINE_ACK, 0, "offline")

#define wait_for_pll_update(pll) \
	wait_for_pll(pll, PLL_UPDATE, 1, "update")

#define wait_for_pll_update_ack_set(pll) \
	wait_for_pll(pll, ALPHA_PLL_ACK_LATCH, 0, "update_ack_set")

#define wait_for_pll_update_ack_clear(pll) \
	wait_for_pll(pll, ALPHA_PLL_ACK_LATCH, 1, "update_ack_clear")

/**
 * clkreg_update_bits() - Update register bits
 * @addr: address of the register
 * @mask: mask of bits to update
 * @val: value to update
 *
 * This function updates a register bitfield specified by @mask with value @val.
 * It is assumed that the register is protected by a lock.
 *
 * Return: None
 */
static void clkreg_update_bits(phys_addr_t addr, u32 mask, u32 val)
{
	u32 tmp;

	if (!addr) {
		pr_err("clk register update failed\n");
		return;
	}

	tmp = readl(addr);
	tmp &= ~mask;
	tmp |= (val & mask);
	writel(tmp, addr);
}

/**
 * clk_alpha_pll_write_config() - Write PLL configuration
 * @reg: register address
 * @val: value to write
 *
 * This function writes a value to a PLL configuration register.
 *
 * Return: None
 */
static void clk_alpha_pll_write_config(phys_addr_t reg,
					unsigned int val)
{
	if (!reg) {
		pr_err("clk register write failed\n");
		return;
	}

	writel(val, reg);
}

/**
 * qcom_pll_set_fsm_mode() - Set PLL FSM mode
 * @reg: register address
 * @bias_count: bias count value
 * @lock_count: lock count value
 *
 * This function sets the PLL FSM mode by programming the bias count and lock
 * count values in the register. It also enables the PLL FSM voting.
 *
 * Return: None
 */
static void qcom_pll_set_fsm_mode(phys_addr_t reg, u8 bias_count, u8 lock_count)
{
	u32 val, mask;

	if (!reg) {
		pr_err("pll fsm mode: Invalid register address\n");
		return;
	}

	/*
	 * Program bias count and lock count
	 */
	val = bias_count << PLL_BIAS_COUNT_SHIFT |
		lock_count << PLL_LOCK_COUNT_SHIFT;
	mask = PLL_BIAS_COUNT_MASK << PLL_BIAS_COUNT_SHIFT;
	mask |= PLL_LOCK_COUNT_MASK << PLL_LOCK_COUNT_SHIFT;
	clkreg_update_bits(reg, mask, val);

	/*
	 * Enable PLL FSM voting
	 */
	clkreg_update_bits(reg, PLL_VOTE_FSM_ENA, PLL_VOTE_FSM_ENA);
}

static void clk_alpha_pll_set_fsm_mode(struct clk_alpha_pll *pll)
{
	qcom_pll_set_fsm_mode(PLL_MODE(pll), 6, 0);
}

static void clk_huayra_pll_set_fsm_mode(struct clk_alpha_pll *pll)
{
	qcom_pll_set_fsm_mode(PLL_MODE(pll), 8, 0);
}

static void clk_lucid_fastn6rf_pll_set_fsm_mode(struct clk_alpha_pll *pll)
{
	/**
	 * Place holder for Lucid fastn6rf FSM mode
	 * TODO: need FSM bias and lock count
	 */
	//qcom_pll_set_fsm_mode(PLL_MODE(pll), 6, 0);
}

/**
 * clk_alpha_pll_regsettings() - Apply PLL register settings
 * @pll: pointer to struct clk_alpha_pll
 * @config: pointer to struct alpha_pll_config
 *
 * This function applies the register settings for a PLL based on the provided
 * configuration.
 *
 * Return: None
 */
static void clk_alpha_pll_regsettings(struct clk_alpha_pll *pll,
					const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll regsettings: invalid arguments\n");
		return;
	}

	/*
	 * Write the config ctl config
	 */
	clk_alpha_pll_write_config(PLL_CONFIG_CTL(pll),
				   config->config_ctl_val);

	/*
	 * Write the test ctl config
	 */
	clk_alpha_pll_write_config(PLL_TEST_CTL(pll),
				   config->test_ctl_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U(pll),
				   config->test_ctl_hi_val);

	/* Update the user ctl config,
	 * since the PLL out may be configured previously
	 */
	clkreg_update_bits(PLL_USER_CTL(pll),
			   config->user_ctl_val, config->user_ctl_val);

	clkreg_update_bits(PLL_USER_CTL_U(pll),
			   config->user_ctl_hi_val, config->user_ctl_hi_val);
}

static void clk_huayra_pll_regsettings(struct clk_alpha_pll *pll,
					const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll regsettings: invalid arguments\n");
		return;
	}

	/*
	 * Write the config ctl config
	 */
	clk_alpha_pll_write_config(PLL_CONFIG_CTL(pll),
				   config->config_ctl_val);

	/*
	 * Write the test ctl config
	 */
	clk_alpha_pll_write_config(PLL_TEST_CTL(pll),
				   config->test_ctl_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U(pll),
				   config->test_ctl_hi_val);

	/* Update the user ctl config,
	 * since the PLL out may be configured previously
	 */
	clkreg_update_bits(PLL_USER_CTL(pll),
			   config->user_ctl_val, config->user_ctl_val);
}

static void clk_huayra_pll_v2_regsettings(struct clk_alpha_pll *pll,
					const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll regsettings: invalid arguments\n");
		return;
	}

	/*
	 * Write the config ctl config
	 */
	clk_alpha_pll_write_config(PLL_CONFIG_CTL(pll),
				   config->config_ctl_val);

	clk_alpha_pll_write_config(PLL_CONFIG_CTL_U(pll),
				   config->config_ctl_hi_val);

	/*
	 * Write the test ctl config
	 */
	clk_alpha_pll_write_config(PLL_TEST_CTL(pll),
				   config->test_ctl_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U(pll),
				   config->test_ctl_hi_val);
}

static void clk_zonda_pll_regsettings(struct clk_alpha_pll *pll,
					  const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll regsettings: invalid arguments\n");
		return;
	}

	/*
	 * Write the config ctl config
	 */
	clk_alpha_pll_write_config(PLL_CONFIG_CTL(pll),
					config->config_ctl_val);

	clk_alpha_pll_write_config(PLL_CONFIG_CTL_U(pll),
					config->config_ctl_hi_val);

	clk_alpha_pll_write_config(PLL_CONFIG_CTL_U1(pll),
					config->config_ctl_hi1_val);

	/*
	 * Write the test ctl config
	 */
	clk_alpha_pll_write_config(PLL_TEST_CTL(pll),
					config->test_ctl_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U(pll),
					config->test_ctl_hi_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U1(pll),
					config->test_ctl_hi1_val);

	/* Update the user ctl config,
	 * since the PLL out may be configured previously
	 */
	clkreg_update_bits(PLL_USER_CTL(pll),
			   config->user_ctl_val, config->user_ctl_val);
}

static void
clk_lucid_fastn6rf_pll_regsettings(struct clk_alpha_pll *pll,
					const struct alpha_pll_config *config
)
{
	if (!pll || !config) {
		pr_err("pll regsettings: invalid arguments\n");
		return;
	}

	/*
	 * Write the config ctl config
	 */
	clk_alpha_pll_write_config(PLL_CONFIG_CTL(pll),
					config->config_ctl_val);

	clk_alpha_pll_write_config(PLL_CONFIG_CTL_U(pll),
					config->config_ctl_hi_val);

	clk_alpha_pll_write_config(PLL_CONFIG_CTL_U1(pll),
					config->config_ctl_hi1_val);

	/*
	 * Write the test ctl config
	 */
	clk_alpha_pll_write_config(PLL_TEST_CTL(pll),
					config->test_ctl_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U(pll),
					config->test_ctl_hi_val);

	clk_alpha_pll_write_config(PLL_TEST_CTL_U1(pll),
					config->test_ctl_hi1_val);

	/* Update the user ctl config,
	 * since the PLL out may be configured previously
	 */
	clkreg_update_bits(PLL_USER_CTL(pll),
			   config->user_ctl_val, config->user_ctl_val);

	clkreg_update_bits(PLL_USER_CTL_U(pll),
			   config->user_ctl_hi_val, config->user_ctl_hi_val);
}

/**
 * pll_is_enabled - Check if PLL is enabled
 * @pll: Pointer to struct clk_alpha_pll
 * @mask: Mask to check in the mode register
 *
 * Return: 1 if PLL is enabled, 0 otherwise
 */
static int pll_is_enabled(struct clk_alpha_pll *pll, u32 mask)
{
	u32 val;

	if (!pll) {
		pr_err("pll is enabled: invalid arguments\n");
		return 0;
	}

	val = readl(PLL_MODE(pll));

	return !!(val & mask);
}

static int clk_alpha_pll_is_enabled(struct clk_alpha_pll *pll)
{
	return pll_is_enabled(pll, PLL_OUTCTRL | PLL_BYPASSNL | PLL_RESET_N) ||
		pll_is_enabled(pll, PLL_LOCK_DET);
}

static int clk_lucid_fastn6rf_pll_is_enabled(struct clk_alpha_pll *pll)
{
	return pll_is_enabled(pll, PLL_LOCK_DET | PLL_RESET_N);
}

/**
 * clk_alpha_pll_prepare() - Prepare the PLL for configuration
 * @pll: Pointer to struct clk_alpha_pll
 * @config: Pointer to configuration data for the PLL
 *
 * This function prepares the PLL for configuration by enabling it and
 * setting up the necessary registers.
 *
 * Return: None
 */
void clk_alpha_pll_prepare(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config)
{
	u32 val, mask;

	if (!pll || !config) {
		pr_err("pll prepare: invalid arguments\n");
		return;
	}

	/*
	 * Enable any outputs for this PLL
	 */
	val = config->main_output_mask;
	val |= config->aux_output_mask;
	val |= config->aux2_output_mask;
	val |= config->early_output_mask;
	val |= config->test_output_mask;

	mask = config->main_output_mask;
	mask |= config->aux_output_mask;
	mask |= config->aux2_output_mask;
	mask |= config->early_output_mask;
	mask |= config->test_output_mask;

	clkreg_update_bits(PLL_USER_CTL(pll), mask, val);
}

void clk_lucid_fastn6rf_pll_prepare(struct clk_alpha_pll *pll,
					const struct alpha_pll_config *config)
{
	u32 val, mask;

	if (!pll || !config) {
		pr_err("pll prepare: invalid arguments\n");
		return;
	}

	/*
	 * Enable any outputs for this PLL
	 */
	val = config->main_output_mask;
	val |= config->even_output_mask;
	val |= config->odd_output_mask;
	val |= config->test_output_mask;

	mask = config->main_output_mask;
	mask |= config->even_output_mask;
	mask |= config->odd_output_mask;
	mask |= config->test_output_mask;

	clkreg_update_bits(PLL_USER_CTL(pll), mask, val);
}

/**
 * clk_alpha_pll_configure() - Configure the PLL with given settings
 * @pll: Pointer to struct clk_alpha_pll
 * @config: Pointer to configuration data for the PLL
 *
 * This function configures the PLL with the provided settings.
 *
 * Return: None
 */
static void clk_alpha_pll_configure(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll configure: invalid arguments\n");
		return;
	}

	/*
	 * Skip, if already enabled
	 */
	if (clk_alpha_pll_is_enabled(pll))
		return;

	/*
	 * Set register settings
	 */
	clk_alpha_pll_regsettings(pll, config);

	if (pll->flags & SUPPORTS_FSM_MODE)
		clk_alpha_pll_set_fsm_mode(pll);
}

static void clk_huayra_pll_configure(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll configure: invalid arguments\n");
		return;
	}

	/*
	 * Skip, if already enabled
	 */
	if (clk_alpha_pll_is_enabled(pll))
		return;

	/*
	 * Set register settings
	 */
	clk_huayra_pll_regsettings(pll, config);

	if (pll->flags & SUPPORTS_FSM_MODE)
		clk_huayra_pll_set_fsm_mode(pll);
}

static void clk_huayra_v2_pll_configure(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll configure: invalid arguments\n");
		return;
	}

	/*
	 * Skip, if already enabled
	 */
	if (clk_alpha_pll_is_enabled(pll))
		return;

	/*
	 * Set register settings
	 */
	clk_zonda_pll_regsettings(pll, config);

	if (pll->flags & SUPPORTS_FSM_MODE)
		clk_huayra_pll_set_fsm_mode(pll);
}

static void clk_huayra_v3_pll_configure(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll configure: invalid arguments\n");
		return;
	}

	/*
	 * Skip, if already enabled
	 */
	if (clk_alpha_pll_is_enabled(pll))
		return;

	/*
	 * Set register settings
	 */
	clk_huayra_pll_v2_regsettings(pll, config);
}

static void
clk_lucid_fastn6rf_pll_configure(struct clk_alpha_pll *pll,
				 const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll configure: invalid arguments\n");
		return;
	}

	/*
	 * Skip, if already enabled
	 */
	if (clk_lucid_fastn6rf_pll_is_enabled(pll))
		return;

	/*
	 * Set register settings
	 */
	clk_lucid_fastn6rf_pll_regsettings(pll, config);

	if (pll->flags & SUPPORTS_FSM_MODE)
		clk_lucid_fastn6rf_pll_set_fsm_mode(pll);
}

static void clk_zonda_pll_configure(struct clk_alpha_pll *pll,
					const struct alpha_pll_config *config)
{
	if (!pll || !config) {
		pr_err("pll configure: invalid arguments\n");
		return;
	}

	/*
	 * Skip, if already enabled
	 */
	if (clk_alpha_pll_is_enabled(pll))
		return;

	/*
	 * Set register settings
	 */
	clk_zonda_pll_regsettings(pll, config);

	if (pll->flags & SUPPORTS_FSM_MODE)
		clk_alpha_pll_set_fsm_mode(pll);
}

/**
 * clk_alpha_pll_disable() - Disable the PLL
 * @pll: Pointer to struct clk_alpha_pll
 *
 * This function disables the PLL by clearing the enable bits in the control
 * registers.
 *
 * Return: None
 */
static void clk_alpha_pll_disable(struct clk_alpha_pll *pll)
{
	u32 mask;
	bool is_pll_fsm_mode;

	if (!pll) {
		pr_err("pll disable: invalid arguments\n");
		return;
	}

	/*
	 * If in FSM mode, just unvote it
	 */
	is_pll_fsm_mode = pll_is_enabled(pll, PLL_VOTE_FSM_ENA);
	if (is_pll_fsm_mode) {
		if (pll->vote_addr)
			clkreg_update_bits(pll->vote_addr, pll->vote_mask, 0);

		return;
	}

	mask = PLL_OUTCTRL;
	clkreg_update_bits(PLL_MODE(pll), mask, 0);

	/*
	 * Delay of 2 output clock ticks required until output is disabled
	 */
	mb();
	udelay(1);

	mask = PLL_RESET_N | PLL_BYPASSNL;
	clkreg_update_bits(PLL_MODE(pll), mask, 0);
}

static void clk_lucid_fastn6rf_pll_disable(struct clk_alpha_pll *pll)
{
	bool is_pll_fsm_mode;

	if (!pll) {
		pr_err("pll disable: invalid arguments\n");
		return;
	}

	/*
	 * If in FSM mode, just unvote it
	 */
	is_pll_fsm_mode = pll_is_enabled(pll, PLL_VOTE_FSM_ENA);
	if (is_pll_fsm_mode) {
		if (pll->vote_addr)
			clkreg_update_bits(pll->vote_addr, pll->vote_mask, 0);

		return;
	}

	/*
	 * Disable the global PLL output
	 */
	clkreg_update_bits(PLL_MODE(pll), PLL_OUTCTRL, 0);

	/*
	 * Disable the PLL outputs
	 */
	clkreg_update_bits(PLL_USER_CTL(pll), PLL_OUT_MASK, 0);

	/*
	 * Set the PLL mode in STANDBY
	 */
	clkreg_update_bits(PLL_OPMODE(pll), PLL_STANDBY, PLL_STANDBY);

	/*
	 * De-assert the PLL reset
	 */
	clkreg_update_bits(PLL_MODE(pll), PLL_RESET_N, 0);
}

/**
 * clk_alpha_pll_enable() - Enable the PLL
 * @pll: Pointer to struct clk_alpha_pll
 *
 * This function enables the PLL by setting the enable bits in the control
 * registers.
 *
 * Return: 0 on success, negative error code on failure
 */
static int clk_alpha_pll_enable(struct clk_alpha_pll *pll)
{
	int ret;

	if (!pll) {
		pr_err("pll enable: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * If in FSM mode, just vote for it
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA)) {
		if (!pll->vote_addr)
			return 0;

		clkreg_update_bits(pll->vote_addr,
					pll->vote_mask, pll->vote_mask);

		return wait_for_pll_enable_active(pll);
	}

	clkreg_update_bits(PLL_USER_CTL_U(pll),
				PLL_LOCK_DET_EN, PLL_LOCK_DET_EN);

	clkreg_update_bits(PLL_MODE(pll), PLL_BYPASSNL, PLL_BYPASSNL);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	clkreg_update_bits(PLL_MODE(pll), PLL_RESET_N, PLL_RESET_N);

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	clkreg_update_bits(PLL_MODE(pll), PLL_OUTCTRL, PLL_OUTCTRL);

	/*
	 * Ensure that the write above goes through before returning.
	 */
	mb();

	return 0;
}

static int clk_huayra_pll_enable(struct clk_alpha_pll *pll)
{
	int ret;

	if (!pll) {
		pr_err("pll enable: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * If in FSM mode, just vote for it
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA)) {
		if (!pll->vote_addr)
			return 0;

		clkreg_update_bits(pll->vote_addr,
					pll->vote_mask, pll->vote_mask);

		return wait_for_pll_enable_active(pll);
	}

	clkreg_update_bits(PLL_MODE(pll), PLL_BYPASSNL, PLL_BYPASSNL);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	clkreg_update_bits(PLL_MODE(pll), PLL_RESET_N, PLL_RESET_N);

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	clkreg_update_bits(PLL_MODE(pll), PLL_OUTCTRL, PLL_OUTCTRL);

	/*
	 * Ensure that the write above goes through before returning.
	 */
	mb();

	return 0;
}

static int clk_lucid_fastn6rf_pll_enable(struct clk_alpha_pll *pll)
{
	int ret;

	if (!pll) {
		pr_err("pll enable: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * If in FSM mode, just vote for it
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA)) {
		if (!pll->vote_addr)
			return 0;

		clkreg_update_bits(pll->vote_addr,
					pll->vote_mask, pll->vote_mask);

		return wait_for_pll_enable_active(pll);
	}

	/**
	 * When 0, VOTE_FSM will put the PLL in STANDBY state,
	 * when there is no vote.
	 *
	 * When 1, VOTE_FSM will out the PLL in OFF state,
	 * when there is no vote.
	 */
	clkreg_update_bits(PLL_MODE(pll),
				LUCID_FASTN6RF_FSM_LEGACY_MODE,
				LUCID_FASTN6RF_FSM_LEGACY_MODE);

	clkreg_update_bits(PLL_OPMODE(pll), PLL_STANDBY, PLL_STANDBY);

	clkreg_update_bits(PLL_MODE(pll), PLL_RESET_N, PLL_RESET_N);

	clkreg_update_bits(PLL_OPMODE(pll), PLL_RUN, PLL_RUN);

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	clkreg_update_bits(PLL_MODE(pll), PLL_OUTCTRL, PLL_OUTCTRL);

	/*
	 * Ensure that the write above goes through before returning.
	 */
	mb();

	return 0;
}

static int clk_zonda_pll_enable(struct clk_alpha_pll *pll)
{
	int ret;
	u32 i, val;

	if (!pll) {
		pr_err("pll enable: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * If in FSM mode, just vote for it
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA)) {
		if (!pll->vote_addr)
			return 0;

		clkreg_update_bits(pll->vote_addr,
					pll->vote_mask, pll->vote_mask);

		return wait_for_pll_enable_active(pll);
	}

	clkreg_update_bits(PLL_MODE(pll), PLL_BYPASSNL, PLL_BYPASSNL);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	clkreg_update_bits(PLL_MODE(pll), PLL_RESET_N, PLL_RESET_N);

	/*
	 * Memory barrier
	 */
	mb();
	udelay(50);

	clkreg_update_bits(PLL_OPMODE(pll), PLL_RUN, PLL_RUN);

	val = readl(PLL_TEST_CTL(pll));

	/*
	 * If cfa mode then poll for freq lock
	 */
	if (val & ZONDA_STAY_IN_CFA)
		ret = wait_for_zonda_pll_freq_lock(pll);
	else {
		/**
		 * V1 Workaround for HW issue locking the PLL
		 * in less than 500us.
		 *
		 * Wait for prescribed time 500us before checking
		 * if the PLL is locked.
		 *
		 * Timeout after 5 tries.
		 */
		for (i = 0; i < 5; i++) {
			udelay(500);

			ret = wait_for_pll_enable_lock(pll);
			if (!ret)
				break;

			/**
			 * Reset the OPMODE to try again.
			 */
			clkreg_update_bits(PLL_OPMODE(pll),
						PLL_STANDBY,
						PLL_STANDBY);
			/*
			 * Memory barrier
			 */
			mb();
			udelay(50);

			clkreg_update_bits(PLL_OPMODE(pll), PLL_RUN, PLL_RUN);
			/*
			 * Memory barrier
			 */
			mb();
		}
	}

	if (ret)
		return ret;

	clkreg_update_bits(PLL_MODE(pll), PLL_OUTCTRL, PLL_OUTCTRL);

	/*
	 * Ensure that the write above goes through before returning.
	 */
	mb();

	return 0;
}

static const struct clk_div_table clk_alpha_post_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ 0xf, 16 },
	{ }
};

static const struct clk_div_table clk_alpha_pre_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ }
};

static const struct clk_div_table clk_alpha_aux_div_table[] = {
	{ 0x3, 3 },
	{ 0x5, 5 },
	{ 0x7, 7 },
	{ }
};

/**
 * clk_alpha_pll_set_rate() - Set the PLL rate
 * @pll: Pointer to struct clk_alpha_pll
 * @config: Pointer to struct clk_config
 * @rate: PLL Rate
 * @prate: Parent rate
 *
 * This function sets the PLL rate by updating the alpha/L values
 * in the PLL configuration registers.
 *
 * Return: 0 on success, negative error code on failure
 */
static int clk_alpha_pll_set_rate(struct clk_alpha_pll *pll,
				  const struct alpha_pll_config *config,
				  unsigned long rate,
				  unsigned long prate)
{
	u32 i, mask, val;

	if (!pll || !config) {
		pr_err("pll set rate: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * Disable the PLL
	 */
	if (clk_alpha_pll_is_enabled(pll))
		clk_alpha_pll_disable(pll);

	/*
	 * Disable FSM Mode
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA))
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_ENA, 0);

	/*
	 * Program L/Alpha/AlphaU values
	 */
	clk_alpha_pll_write_config(PLL_L_VAL(pll), config->l);
	clk_alpha_pll_write_config(PLL_ALPHA_VAL(pll), config->alpha);
	clk_alpha_pll_write_config(PLL_ALPHA_VAL_U(pll), config->alpha_hi);

	/*
	 * Configure Post div
	 */
	for (i = 0; i < ARRAY_SIZE(clk_alpha_post_div_table); i++) {
		if (clk_alpha_post_div_table[i].div == config->post_div_val) {
			val = clk_alpha_post_div_table[i].val;
			mask = val << PLL_POST_DIV_SHIFT;
			clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);
			break;
		}
	}

	/*
	 * Configure pre div
	 */
	for (i = 0; i < ARRAY_SIZE(clk_alpha_pre_div_table); i++) {
		if (clk_alpha_pre_div_table[i].div == config->pre_div_val) {
			val = clk_alpha_pre_div_table[i].val;
			mask = val << PLL_PRE_DIV_SHIFT;
			clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);
			break;
		}
	}

	/*
	 * Configure Aux Post div
	 */
	for (i = 0; i < ARRAY_SIZE(clk_alpha_aux_div_table); i++) {
		if (clk_alpha_aux_div_table[i].div == config->aux_post_div_val) {
			val = clk_alpha_aux_div_table[i].val;
			mask = val << PLL_AUX_POST_DIV_SHIFT;
			clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);
			break;
		}
	}

	if (config->alpha) {
		clkreg_update_bits(PLL_USER_CTL(pll),
				   PLL_ALPHA_EN, PLL_ALPHA_EN);

		clkreg_update_bits(PLL_USER_CTL(pll),
				   PLL_ALPHA_MODE, PLL_ALPHA_MODE);
	}

	/*
	 * Configure in FSM Mode if supported
	 */
	if (pll->flags & SUPPORTS_FSM_MODE) {
		/*
		 * Assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll),
					PLL_VOTE_FSM_RESET,
					PLL_VOTE_FSM_RESET);

		clk_alpha_pll_set_fsm_mode(pll);

		/*
		 * De-assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_RESET, 0);
	}

	return 0;
}

static const struct clk_div_table clk_huayra_post_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ 0xf, 16 },
	{ }
};

static const struct clk_div_table clk_huayra_pre_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ }
};

static int clk_huayra_pll_set_rate(struct clk_alpha_pll *pll,
				  const struct alpha_pll_config *config,
				  unsigned long rate,
				  unsigned long prate)
{
	u32 i, mask, val;

	if (!pll || !config) {
		pr_err("pll set rate: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * Disable the PLL
	 */
	if (clk_alpha_pll_is_enabled(pll))
		clk_alpha_pll_disable(pll);

	/*
	 * Disable FSM Mode
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA))
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_ENA, 0);

	/*
	 * Program L/Alpha/AlphaU values
	 */
	clk_alpha_pll_write_config(PLL_L_VAL(pll), config->l);
	clk_alpha_pll_write_config(PLL_ALPHA_VAL(pll), config->alpha);
	clk_alpha_pll_write_config(PLL_ALPHA_VAL_U(pll), config->alpha_hi);

	/*
	 * Configure Post div
	 */
	for (i = 0; i < ARRAY_SIZE(clk_huayra_post_div_table); i++) {
		if (clk_huayra_post_div_table[i].div == config->post_div_val) {
			val = clk_huayra_post_div_table[i].val;
			mask = val << PLL_POST_DIV_SHIFT;
			clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);
			break;
		}
	}

	/*
	 * Configure pre div
	 */
	for (i = 0; i < ARRAY_SIZE(clk_huayra_pre_div_table); i++) {
		if (clk_huayra_pre_div_table[i].div == config->pre_div_val) {
			val = clk_huayra_pre_div_table[i].val;
			mask = val << PLL_PRE_DIV_SHIFT;
			clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);
			break;
		}
	}

	if (config->alpha) {
		clkreg_update_bits(PLL_USER_CTL(pll),
				   PLL_ALPHA_EN, PLL_ALPHA_EN);

		clkreg_update_bits(PLL_USER_CTL(pll),
				   PLL_ALPHA_MODE, PLL_ALPHA_MODE);
	}

	/*
	 * Configure in FSM Mode if supported
	 */
	if (pll->flags & SUPPORTS_FSM_MODE) {
		/*
		 * Assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll),
					PLL_VOTE_FSM_RESET,
					PLL_VOTE_FSM_RESET);

		clk_huayra_pll_set_fsm_mode(pll);

		/*
		 * De-assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_RESET, 0);
	}

	return 0;
}

static const struct clk_div_table clk_lucid_fastn6rf_post_div_odd_table[] = {
	{ 0x0, 1 },
	{ 0x1, 3 },
	{ 0x3, 5 },
	{ 0x7, 7 },
	{ }
};

static const struct clk_div_table clk_lucid_fastn6rf_post_div_even_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ }
};

static const struct clk_div_table clk_lucid_fastn6rf_pre_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ }
};

static int
clk_lucid_fastn6rf_pll_set_rate(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config,
				unsigned long rate,
				unsigned long prate)
{
	u32 i, mask, val, div_table_sz;
	const struct clk_div_table *clk_div_table;

	if (!pll || !config) {
		pr_err("pll set rate: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * Disable the PLL
	 */
	if (clk_lucid_fastn6rf_pll_is_enabled(pll))
		clk_lucid_fastn6rf_pll_disable(pll);

	/*
	 * Disable FSM Mode
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA))
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_ENA, 0);

	/*
	 * Program L/Alpha/AlphaU values
	 */
	clk_alpha_pll_write_config(PLL_L_VAL(pll), config->l);
	clk_alpha_pll_write_config(PLL_CAL_L_VAL(pll),
					LUCID_FASTN6RF_PLL_CAL_L_VAL);

	clk_alpha_pll_write_config(PLL_ALPHA_VAL(pll), config->alpha);

	/*
	 * Configure Post div odd
	 */
	clk_div_table = clk_lucid_fastn6rf_post_div_odd_table;
	div_table_sz = ARRAY_SIZE(clk_lucid_fastn6rf_post_div_odd_table);
	val = 0;
	for (i = 0; i < div_table_sz; i++) {
		if (clk_div_table[i].div == config->post_div_odd_val) {
			val = clk_div_table[i].val;
			break;
		}
	}
	val <<= LUCID_FASTN6RF_POST_DIV_ODD_SHIFT;
	mask = LUCID_FASTN6RF_POST_DIV_ODD_MASK;
	clkreg_update_bits(PLL_USER_CTL(pll), mask, val);

	/*
	 * Configure Post div even
	 */
	clk_div_table = clk_lucid_fastn6rf_post_div_even_table;
	div_table_sz = ARRAY_SIZE(clk_lucid_fastn6rf_post_div_even_table);
	val = 0;
	for (i = 0; i < div_table_sz; i++) {
		if (clk_div_table[i].div == config->post_div_even_val) {
			val = clk_div_table[i].val;
			break;
		}
	}
	val <<= LUCID_FASTN6RF_POST_DIV_EVEN_SHIFT;
	mask = LUCID_FASTN6RF_POST_DIV_EVEN_MASK;
	clkreg_update_bits(PLL_USER_CTL(pll), mask, val);

	/*
	 * Configure pre div
	 */
	clk_div_table = clk_lucid_fastn6rf_pre_div_table;
	div_table_sz = ARRAY_SIZE(clk_lucid_fastn6rf_pre_div_table);
	val = 0;
	for (i = 0; i < div_table_sz; i++) {
		if (clk_div_table[i].div == config->pre_div_val) {
			val = clk_div_table[i].val;
			break;
		}
	}
	val <<= LUCID_FASTN6RF_PRE_DIV_SHIFT;
	mask = LUCID_FASTN6RF_PRE_DIV_MASK;
	clkreg_update_bits(PLL_USER_CTL(pll), mask, val);

	if (config->alpha)
		clkreg_update_bits(PLL_USER_CTL_U(pll),
				   LUCID_FASTN6RF_FRAC_FORMAT_SEL,
				   0);
	else
		clkreg_update_bits(PLL_USER_CTL_U(pll),
				   LUCID_FASTN6RF_FRAC_FORMAT_SEL,
				   LUCID_FASTN6RF_FRAC_FORMAT_SEL);

	/*
	 * Configure in FSM Mode if supported
	 */
	if (pll->flags & SUPPORTS_FSM_MODE) {
		/*
		 * Assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll),
					PLL_VOTE_FSM_RESET,
					PLL_VOTE_FSM_RESET);

		clk_lucid_fastn6rf_pll_set_fsm_mode(pll);

		/*
		 * De-assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_RESET, 0);
	}

	return 0;
}

static const struct clk_div_table clk_zonda_post_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x2, 4 },
	{ 0x3, 8 },
	{ }
};

static const struct clk_div_table clk_zonda_pre_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ }
};

static int clk_zonda_pll_set_rate(struct clk_alpha_pll *pll,
				  const struct alpha_pll_config *config,
				  unsigned long rate,
				  unsigned long prate)
{
	u32 i, mask, val;

	if (!pll || !config) {
		pr_err("pll set rate: invalid arguments\n");
		return -EINVAL;
	}

	/*
	 * Disable the PLL
	 */
	if (clk_alpha_pll_is_enabled(pll))
		clk_alpha_pll_disable(pll);

	/*
	 * Disable FSM Mode
	 */
	if (pll_is_enabled(pll, PLL_VOTE_FSM_ENA))
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_ENA, 0);

	/*
	 * Program L/Alpha/AlphaU values
	 */
	clk_alpha_pll_write_config(PLL_L_VAL(pll), config->l);
	clk_alpha_pll_write_config(PLL_ALPHA_VAL(pll), config->alpha);

	/*
	 * Configure Post div
	 */
	val = 0;
	for (i = 0; i < ARRAY_SIZE(clk_zonda_post_div_table); i++) {
		if (clk_zonda_post_div_table[i].div == config->post_div_val) {
			val = clk_zonda_post_div_table[i].val;
			break;
		}
	}
	mask = val << PLL_POST_DIV_SHIFT;
	clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);

	/*
	 * Configure pre div
	 */
	val = 0;
	for (i = 0; i < ARRAY_SIZE(clk_zonda_pre_div_table); i++) {
		if (clk_zonda_pre_div_table[i].div == config->pre_div_val) {
			val = clk_zonda_pre_div_table[i].val;
			break;
		}
	}
	mask = val << PLL_PRE_DIV_SHIFT;
	clkreg_update_bits(PLL_USER_CTL(pll), mask, mask);

	if (config->alpha) {
		clkreg_update_bits(PLL_USER_CTL(pll),
				   PLL_ALPHA_EN, PLL_ALPHA_EN);

		clkreg_update_bits(PLL_USER_CTL(pll),
				   PLL_ALPHA_MODE, PLL_ALPHA_MODE);
	}

	/*
	 * Configure in FSM Mode if supported
	 */
	if (pll->flags & SUPPORTS_FSM_MODE) {
		/*
		 * Assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll),
					PLL_VOTE_FSM_RESET,
					PLL_VOTE_FSM_RESET);

		clk_alpha_pll_set_fsm_mode(pll);

		/*
		 * De-assert reset to FSM
		 */
		clkreg_update_bits(PLL_MODE(pll), PLL_VOTE_FSM_RESET, 0);
	}

	return 0;
}

/**
 * clk_alpha_pll_ops - operations for alpha pll
 * @enable: enable the pll
 * @disable: disable the pll
 * @is_enabled: check if pll is enabled
 * @set_rate: set the pll rate
 * @prepare: prepare the pll
 */
const struct alpha_pll_ops clk_alpha_pll_ops = {
	.enable = clk_alpha_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.set_rate = clk_alpha_pll_set_rate,
	.prepare = clk_alpha_pll_prepare,
	.configure = clk_alpha_pll_configure,
};

const struct alpha_pll_ops clk_alpha_pll_huayra_ops = {
	.enable = clk_huayra_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.set_rate = clk_huayra_pll_set_rate,
	.prepare = clk_alpha_pll_prepare,
	.configure = clk_huayra_pll_configure,
};

const struct alpha_pll_ops clk_alpha_pll_huayra_v2_ops = {
	.enable = clk_huayra_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.set_rate = clk_huayra_pll_set_rate,
	.prepare = clk_alpha_pll_prepare,
	.configure = clk_huayra_v2_pll_configure,
};

const struct alpha_pll_ops clk_alpha_pll_huayra_v3_ops = {
	.enable = clk_huayra_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.set_rate = clk_huayra_pll_set_rate,
	.prepare = clk_alpha_pll_prepare,
	.configure = clk_huayra_v3_pll_configure,
};

const struct alpha_pll_ops clk_alpha_pll_lucid_fastn6rf_ops = {
	.enable = clk_lucid_fastn6rf_pll_enable,
	.disable = clk_lucid_fastn6rf_pll_disable,
	.is_enabled = clk_lucid_fastn6rf_pll_is_enabled,
	.set_rate = clk_lucid_fastn6rf_pll_set_rate,
	.prepare = clk_lucid_fastn6rf_pll_prepare,
	.configure = clk_lucid_fastn6rf_pll_configure,
};

const struct alpha_pll_ops clk_alpha_pll_zonda_ops = {
	.enable = clk_zonda_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.set_rate = clk_zonda_pll_set_rate,
	.prepare = clk_alpha_pll_prepare,
	.configure = clk_zonda_pll_configure,
};

/**
 * List of Target specific PLL offsets
 */
static const u8 ipq5424_pll_offsets[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_DEFAULT] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_TEST_CTL] = 0x10,
		[PLL_OFF_TEST_CTL_U] = 0x14,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_USER_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_HUAYRA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_TEST_CTL] = 0x0c,
		[PLL_OFF_TEST_CTL_U] = 0x10,
		[PLL_OFF_USER_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL_U] = 0x1c,
		[PLL_OFF_STATUS] = 0x20,
	},
};

/**
 * List of Target specific PLLs
 */
static struct clk_alpha_pll ipq5424_gpll0 = {
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_DEFAULT],
};

static struct clk_alpha_pll ipq5424_gpll2 = {
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_HUAYRA],
};

static struct clk_alpha_pll ipq5424_gpll4 = {
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_DEFAULT],
};

static struct clk_alpha_pll ipq5424_apss_pll = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
};

static struct clk_alpha_pll ipq5424_l3_pll = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
};

/*
 * APSS PLL offsets for IPQ5210 follow the APSS-Huayra layout from Linux.
 */
static const u8 ipq5210_apss_pll_offsets[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_HUAYRA] = {
		[PLL_OFF_L_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL] = 0x10,
		[PLL_OFF_USER_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL] = 0x20,
		[PLL_OFF_CONFIG_CTL_U] = 0x24,
		[PLL_OFF_STATUS] = 0x28,
		[PLL_OFF_TEST_CTL] = 0x30,
		[PLL_OFF_TEST_CTL_U] = 0x34,
	},
};

static struct clk_alpha_pll ipq5210_gpll0 = {
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_DEFAULT],
};

static struct clk_alpha_pll ipq5210_gpll2 = {
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_DEFAULT],
};

static struct clk_alpha_pll ipq5210_gpll4 = {
	.regs = ipq5424_pll_offsets[CLK_ALPHA_PLL_TYPE_DEFAULT],
};

static struct clk_alpha_pll ipq5210_apss_pll = {
	.regs = ipq5210_apss_pll_offsets[CLK_ALPHA_PLL_TYPE_HUAYRA],
};

static struct clk_alpha_pll ipq9650_gpll0 = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_FAST_N6RF],
};

static struct clk_alpha_pll ipq9650_gpll2 = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
};

static struct clk_alpha_pll ipq9650_gpll4 = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_FAST_N6RF],
};

static struct clk_alpha_pll ipq9650_apss_pll = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
};

static struct clk_alpha_pll ipq9650_l3_pll = {
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_ZONDA],
};

/**
 * List of Target specific PLL Configs
 */
static const struct alpha_pll_config ipq5424_gpll0_config = {
	.alpha = 0x55555555,
	.alpha_hi = 0x55,
	.l = 0x21,
	.config_ctl_val = 0x4001055b,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.user_ctl_val = 0x1200003,
	.user_ctl_hi_val = 0x4,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq5424_gpll2_config = {
	.alpha = 0x0,
	.l = 0x30,
	.config_ctl_val = 0x4001055b,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.user_ctl_val = 0x3,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
	.post_div_val = 0x2,
};

static const struct alpha_pll_config ipq5424_gpll4_config = {
	.alpha = 0x0,
	.l = 0x32,
	.config_ctl_val = 0x4001055b,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.user_ctl_val = 0x3,
	.user_ctl_hi_val = 0x4,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq5424_apss_pll_config = {
	.alpha = 0x0,
	.l = 0x3b,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0xF,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq5424_l3_pll_config = {
	.alpha = 0x0,
	.l = 0x29,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0xF,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq5210_gpll0_config = {
	.alpha = 0x55555555,
	.alpha_hi = 0x55,
	.l = 0x21,
	.config_ctl_val = 0x4001055b,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.user_ctl_val = 0x1200003,
	.user_ctl_hi_val = 0x4,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq5210_gpll2_config = {
	.alpha = 0x0,
	.l = 0x30,
	.config_ctl_val = 0x4001055b,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.user_ctl_val = 0x3,
	.user_ctl_hi_val = 0x4,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
	.post_div_val = 0x2,
};

static const struct alpha_pll_config ipq5210_gpll4_config = {
	.alpha = 0x0,
	.l = 0x32,
	.config_ctl_val = 0x4001055b,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.user_ctl_val = 0x3,
	.user_ctl_hi_val = 0x4,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq5210_apss_pll_config = {
	.alpha = 0x0,
	.l = 0x3B,
	.config_ctl_val = 0x4001075b,
	.config_ctl_hi_val = 0x6,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x400003,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq9650_gpll0_config = {
	.alpha = 0x5555,
	.l = 0x21,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0xB2923BBC,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x1,
	.user_ctl_hi_val = 0x805,
	.user_ctl_hi1_val = 0x0,
	.odd_output_mask = BIT(2),
	.even_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq9650_gpll2_config = {
	.alpha = 0x0,
	.l = 0x30,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x0,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x8,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
	.post_div_val = 0x2,
};

static const struct alpha_pll_config ipq9650_gpll4_config = {
	.alpha = 0x0,
	.l = 0x32,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00002261,
	.config_ctl_hi1_val = 0xB2923BBC,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x1,
	.user_ctl_hi_val = 0x805,
	.user_ctl_hi1_val = 0x0,
	.odd_output_mask = BIT(2),
	.even_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq9650_apss_pll_config = {
	.alpha = 0x0,
	.l = 0x39,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x01000009,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq9650_l3_pll_config = {
	.alpha = 0x0,
	.l = 0x31,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x01000009,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq9650_v2_apss_pll_config = {
	.alpha = 0x0,
	.l = 0x37,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x01000009,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

static const struct alpha_pll_config ipq9650_v2_l3_pll_config = {
	.alpha = 0x0,
	.l = 0x2f,
	.config_ctl_val = 0x08200920,
	.config_ctl_hi_val = 0x05008001,
	.config_ctl_hi1_val = 0x04000000,
	.test_ctl_val = 0x0,
	.test_ctl_hi_val = 0x0,
	.test_ctl_hi1_val = 0x0,
	.user_ctl_val = 0x01000009,
	.early_output_mask = BIT(3),
	.aux2_output_mask = BIT(2),
	.aux_output_mask = BIT(1),
	.main_output_mask = BIT(0),
};

/**
 * List of Target specific PLL descriptors
 */
static const struct clk_alpha_pll_desc ipq5424_plls[] = {
	{
		.name = "gpll0",
		.pll = &ipq5424_gpll0,
		.pll_config = &ipq5424_gpll0_config,
		.pll_ops = &clk_alpha_pll_ops,
	}, {
		.name = "gpll2",
		.pll = &ipq5424_gpll2,
		.pll_config = &ipq5424_gpll2_config,
		.pll_ops = &clk_alpha_pll_huayra_ops,
	}, {
		.name = "gpll4",
		.pll = &ipq5424_gpll4,
		.pll_config = &ipq5424_gpll4_config,
		.pll_ops = &clk_alpha_pll_ops,
	}, {
		.name = "apssll",
		.pll = &ipq5424_apss_pll,
		.pll_config = &ipq5424_apss_pll_config,
		.pll_ops = &clk_alpha_pll_huayra_v2_ops,
	}, {
		.name = "l3pll",
		.pll = &ipq5424_l3_pll,
		.pll_config = &ipq5424_l3_pll_config,
		.pll_ops = &clk_alpha_pll_huayra_v2_ops,
	}, {
		/**
		 * List Terminator
		 */
	}
};

static const struct clk_alpha_pll_desc ipq5210_plls[] = {
	{
		.name = "gpll0",
		.pll = &ipq5210_gpll0,
		.pll_config = &ipq5210_gpll0_config,
		.pll_ops = &clk_alpha_pll_ops,
	}, {
		.name = "gpll2",
		.pll = &ipq5210_gpll2,
		.pll_config = &ipq5210_gpll2_config,
		.pll_ops = &clk_alpha_pll_ops,
	}, {
		.name = "gpll4",
		.pll = &ipq5210_gpll4,
		.pll_config = &ipq5210_gpll4_config,
		.pll_ops = &clk_alpha_pll_ops,
	}, {
		.name = "apsspll",
		.pll = &ipq5210_apss_pll,
		.pll_config = &ipq5210_apss_pll_config,
		.pll_ops = &clk_alpha_pll_huayra_v3_ops,
	}, {
		/**
		 * List Terminator
		 */
	}
};

static const struct clk_alpha_pll_desc ipq9650_plls[] = {
	{
		.name = "gpll0",
		.pll = &ipq9650_gpll0,
		.pll_config = &ipq9650_gpll0_config,
		.pll_ops = &clk_alpha_pll_lucid_fastn6rf_ops,
	}, {
		.name = "gpll2",
		.pll = &ipq9650_gpll2,
		.pll_config = &ipq9650_gpll2_config,
		.pll_ops = &clk_alpha_pll_zonda_ops,
	}, {
		.name = "gpll4",
		.pll = &ipq9650_gpll4,
		.pll_config = &ipq9650_gpll4_config,
		.pll_ops = &clk_alpha_pll_lucid_fastn6rf_ops,
	}, {
		.name = "apsspll",
		.pll = &ipq9650_apss_pll,
		.pll_config = &ipq9650_apss_pll_config,
		.pll_ops = &clk_alpha_pll_zonda_ops,
	}, {
		.name = "l3pll",
		.pll = &ipq9650_l3_pll,
		.pll_config = &ipq9650_l3_pll_config,
		.pll_ops = &clk_alpha_pll_zonda_ops,
	}, {
		/**
		 * List Terminator
		 */
	}
};

static const struct clk_alpha_pll_desc ipq9650_v2_plls[] = {
	{
		.name = "apsspll",
		.pll = &ipq9650_apss_pll,
		.pll_config = &ipq9650_v2_apss_pll_config,
		.pll_ops = &clk_alpha_pll_zonda_ops,
	}, {
		.name = "l3pll",
		.pll = &ipq9650_l3_pll,
		.pll_config = &ipq9650_v2_l3_pll_config,
		.pll_ops = &clk_alpha_pll_zonda_ops,
	}, {
		/**
		 * List Terminator
		 */
	}
};

/**
 * List of Target specific PLL table information
 */
const struct clk_alpha_pll_tbl ipq5424_pll_tbl = {
	.pll_desc_base = ipq5424_plls,
	.num_plls = ARRAY_SIZE(ipq5424_plls),
};

const struct clk_alpha_pll_tbl ipq5210_pll_tbl = {
	.pll_desc_base = ipq5210_plls,
	.num_plls = ARRAY_SIZE(ipq5210_plls),
};

const struct clk_alpha_pll_tbl ipq9650_pll_tbl = {
	.pll_desc_base = ipq9650_plls,
	.num_plls = ARRAY_SIZE(ipq9650_plls),
};

const struct clk_alpha_pll_tbl ipq9650_v2_pll_tbl = {
	.pll_desc_base = ipq9650_v2_plls,
	.num_plls = ARRAY_SIZE(ipq9650_v2_plls),
};

/**
 * ipq_clk_pll_lookup() - lookup a PLL by name
 * @dev: device pointer
 * @name: name of the PLL to look up
 *
 * Return: pointer to PLL descriptor or NULL if not found
 */
static const struct
clk_alpha_pll_desc *ipq_clk_pll_lookup(struct udevice *dev, const char *name)
{
	const struct clk_alpha_pll_tbl *pll_tbl;
	const struct clk_alpha_pll_desc *pll_desc;
	u8 index;

	if (!name) {
		pr_err("pll lookup: invalid name\n");
		return NULL;
	}

	if (device_is_compatible(dev, "qcom,ipq9650-v2-clk-pll"))
		pll_tbl = &ipq9650_v2_pll_tbl;
	else
		pll_tbl = (const struct clk_alpha_pll_tbl *)dev_get_driver_data(dev);

	if (!pll_tbl || !pll_tbl->pll_desc_base || !pll_tbl->num_plls) {
		pr_err("pll lookup: invalid pll table\n");
		return NULL;
	}

	/**
	 * Walk through the list of PLLs available in the device data
	 */
	for (index = 0; index < pll_tbl->num_plls; index++) {
		pll_desc = &pll_tbl->pll_desc_base[index];

		if (!strcmp(pll_desc->name, name))
			return pll_desc;
	}

	pr_err("pll lookup: no matching pll for %s\n", name);

	return NULL;
}

/**
 * ipq_clk_pll_init - enable the PLL
 * @dev: device pointer
 *
 * Return: 0 on success, negative error code otherwise
 */
static int ipq_clk_pll_init(struct udevice *dev)
{
	int ret;
	const struct clk_alpha_pll_desc *pll_desc;
	struct clk_alpha_pll *pll;
	const struct alpha_pll_config *config;
	const struct alpha_pll_ops *ops;
	struct clk_alpha_pll_priv *priv = dev_get_priv(dev);
	bool is_pll_enabled;

	if (!priv) {
		pr_err("Invalid pll priv info\n");
		return -EINVAL;
	}

	pll_desc = priv->pll_desc;
	if (!pll_desc) {
		pr_err("Invalid pll descriptor\n");
		return -EINVAL;
	}

	pll = pll_desc->pll;
	config = pll_desc->pll_config;
	ops = pll_desc->pll_ops;

	if (!pll || !config || !ops) {
		pr_err("Invalid pll desc info\n");
		return -EINVAL;
	}

	if (!priv->base) {
		pr_err("Invalid pll base address\n");
		return -EINVAL;
	}

	pll->offset	= (phys_addr_t)priv->base;
	pll->vote_addr	= (phys_addr_t)priv->fsm_vote_addr;
	pll->vote_mask	= priv->fsm_vote_mask;

	/**
	 * If PLL has FSM vote address, enable the FSM support flag.
	 */
	if (pll->vote_addr)
		pll->flags = SUPPORTS_FSM_MODE;

	if (ops->prepare)
		ops->prepare(pll, config);

	if (ops->is_enabled)
		is_pll_enabled = ops->is_enabled(pll);
	else
		is_pll_enabled = false;

	if (!is_pll_enabled) {
		if (ops->configure)
			ops->configure(pll, config);

		if (ops->set_rate) {
			ret = ops->set_rate(pll, config, 0, 0);
			if (ret) {
				pr_err("Failed to set rate for %s\n",
					pll_desc->name);
				return ret;
			}
		}

		if (ops->enable) {
			ret = ops->enable(pll);
			if (ret) {
				pr_err("Failed to enable %s\n",
					pll_desc->name);
				return ret;
			}
		}
	}

	return 0;
}

/**
 * ipq_clk_pll_probe - probe function for the PLL driver
 * @pdev: platform device pointer
 *
 * Return: 0 on success, negative error code otherwise
 */
static int ipq_clk_pll_probe(struct udevice *dev)
{
	struct clk_alpha_pll_priv *priv = dev_get_priv(dev);
	phys_addr_t addr, fsm_vote_addr;
	u32 fsm_vote_mask;
	const char *outname;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->base = (void __iomem *)addr;

	/*
	 * vote-addr and vote-mask are optional
	 */
	fsm_vote_addr = (phys_addr_t)
				dev_read_u32_default(dev, "fsm-vote-addr", 0);
	fsm_vote_mask  = dev_read_u32_default(dev, "fsm-vote-mask", 0);

	if (fsm_vote_addr && fsm_vote_mask) {
		priv->fsm_vote_addr = (void __iomem *)(uintptr_t)fsm_vote_addr;
		priv->fsm_vote_mask = fsm_vote_mask;

	} else {
		priv->fsm_vote_addr = NULL;
		priv->fsm_vote_mask = 0;
	}

	/*
	 * Identify PLL instance by clock-output-names
	 */
	outname = dev_read_string(dev, "clock-output-names");
	if (!outname) {
		pr_err("missing clock-output-names\n");
		return -EINVAL;
	}

	/*
	 * Populate the PLL description based on the output name
	 * to the device priv
	 */
	priv->pll_desc = ipq_clk_pll_lookup(dev, outname);
	if (!priv->pll_desc) {
		pr_err("Failed to find PLL descriptor for '%s'\n", outname);
		return -EINVAL;
	}

	/*
	 * Validate PLL descriptor has required fields
	 */
	if (!priv->pll_desc->pll || !priv->pll_desc->pll_config ||
		!priv->pll_desc->pll_ops) {
		pr_err("Invalid PLL descriptor for '%s'\n", outname);
		return -EINVAL;
	}

	return ipq_clk_pll_init(dev);
}

static const struct udevice_id ipq_clk_pll_of_match[] = {
	{
		.compatible = "qcom,ipq-clk-pll"
	}, {
		.compatible = "qcom,ipq5424-clk-pll",
		.data = (ulong)&ipq5424_pll_tbl
	}, {
		.compatible = "qcom,ipq5210-clk-pll",
		.data = (ulong)&ipq5210_pll_tbl
	}, {
		.compatible = "qcom,ipq9650-clk-pll",
		.data = (ulong)&ipq9650_pll_tbl
	}, {
		.compatible = "qcom,ipq9650-v2-clk-pll",
		.data = (ulong)&ipq9650_v2_pll_tbl
	}, {
		/**
		 * List Terminator
		 */
	}
};

U_BOOT_DRIVER(ipq_clk_pll) = {
	.name		= "ipq-clk-pll",
	.id		= UCLASS_MISC,
	.of_match	= ipq_clk_pll_of_match,
	.probe		= ipq_clk_pll_probe,
	.priv_auto	= sizeof(struct clk_alpha_pll_priv),
};
