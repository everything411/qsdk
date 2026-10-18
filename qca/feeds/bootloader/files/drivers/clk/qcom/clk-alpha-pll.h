/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015, 2018, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __QCOM_CLK_ALPHA_PLL_H__
#define __QCOM_CLK_ALPHA_PLL_H__

#include <clk.h>
#include <clk-uclass.h>

/*
 * Alpha PLL types
 */
enum {
	CLK_ALPHA_PLL_TYPE_DEFAULT,
	CLK_ALPHA_PLL_TYPE_HUAYRA,
	CLK_ALPHA_PLL_TYPE_BRAMMO,
	CLK_ALPHA_PLL_TYPE_FABIA,
	CLK_ALPHA_PLL_TYPE_TRION,
	CLK_ALPHA_PLL_TYPE_LUCID = CLK_ALPHA_PLL_TYPE_TRION,
	CLK_ALPHA_PLL_TYPE_AGERA,
	CLK_ALPHA_PLL_TYPE_ZONDA,
	CLK_ALPHA_PLL_TYPE_ZONDA_OLE,
	CLK_ALPHA_PLL_TYPE_LUCID_EVO,
	CLK_ALPHA_PLL_TYPE_LUCID_OLE,
	CLK_ALPHA_PLL_TYPE_LUCID_FAST_N6RF,
	CLK_ALPHA_PLL_TYPE_RIVIAN_EVO,
	CLK_ALPHA_PLL_TYPE_DEFAULT_EVO,
	CLK_ALPHA_PLL_TYPE_BRAMMO_EVO,
	CLK_ALPHA_PLL_TYPE_STROMER,
	CLK_ALPHA_PLL_TYPE_STROMER_PLUS,
	CLK_ALPHA_PLL_TYPE_NSS_HUAYRA,
	CLK_ALPHA_PLL_TYPE_MAX,
};

/*
 * Alpha PLL offsets
 */
enum {
	PLL_OFF_L_VAL,
	PLL_OFF_CAL_L_VAL,
	PLL_OFF_ALPHA_VAL,
	PLL_OFF_ALPHA_VAL_U,
	PLL_OFF_USER_CTL,
	PLL_OFF_USER_CTL_U,
	PLL_OFF_USER_CTL_U1,
	PLL_OFF_CONFIG_CTL,
	PLL_OFF_CONFIG_CTL_U,
	PLL_OFF_CONFIG_CTL_U1,
	PLL_OFF_CONFIG_CTL_U2,
	PLL_OFF_TEST_CTL,
	PLL_OFF_TEST_CTL_U,
	PLL_OFF_TEST_CTL_U1,
	PLL_OFF_TEST_CTL_U2,
	PLL_OFF_STATE,
	PLL_OFF_STATUS,
	PLL_OFF_OPMODE,
	PLL_OFF_FRAC,
	PLL_OFF_CAL_VAL,
	PLL_OFF_MAX_REGS
};

extern const u8 clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_MAX][PLL_OFF_MAX_REGS];

/**
 * struct pll_vco - VCO frequency range and value mapping
 * @min_freq: minimum frequency in KHz
 * @max_freq: maximum frequency in KHz
 * @val: register value for this VCO range
 */
struct pll_vco {
	unsigned long min_freq;
	unsigned long max_freq;
	u32 val;
};

#define VCO(a, b, c) { \
	.val = a,\
	.min_freq = b,\
	.max_freq = c,\
}

/**
 * struct clk_alpha_pll - phase locked loop (PLL)
 * @offset: base address of registers
 * @regs: register offsets from base offset
 * @vote_addr: address of the vote register
 * @vote_mask: mask of the vote register
 * @vco_table: table of VCO frequency ranges and values
 * @num_vco: number of entries in vco_table
 * @flags: flags to indicate special properties of PLL
 */
struct clk_alpha_pll {
	phys_addr_t offset;
	const u8 *regs;
	phys_addr_t vote_addr;
	u32 vote_mask;

	const struct pll_vco *vco_table;
	size_t num_vco;
#define SUPPORTS_OFFLINE_REQ		BIT(0)
#define SUPPORTS_FSM_MODE		BIT(2)
#define SUPPORTS_DYNAMIC_UPDATE		BIT(3)
#define SUPPORTS_FSM_LEGACY_MODE	BIT(4)
	u8 flags;
};

struct alpha_pll_config {
	u32 l;
	u32 alpha;
	u32 alpha_hi;
	u32 config_ctl_val;
	u32 config_ctl_hi_val;
	u32 config_ctl_hi1_val;
	u32 config_ctl_hi2_val;
	u32 user_ctl_val;
	u32 user_ctl_hi_val;
	u32 user_ctl_hi1_val;
	u32 test_ctl_val;
	u32 test_ctl_mask;
	u32 test_ctl_hi_val;
	u32 test_ctl_hi_mask;
	u32 test_ctl_hi1_val;
	u32 test_ctl_hi2_val;
	u32 main_output_mask;
	u32 aux_output_mask;
	u32 aux2_output_mask;
	u32 early_output_mask;
	u32 test_output_mask;
	u32 even_output_mask;
	u32 odd_output_mask;
	u32 alpha_en_mask;
	u32 alpha_mode_mask;
	u32 pre_div_val;
	u32 pre_div_mask;
	u32 post_div_val;
	u32 post_div_mask;
	u32 aux_post_div_val;
	u32 aux_post_div_mask;
	u32 post_div_odd_val;
	u32 post_div_even_val;
	u32 vco_val;
	u32 vco_mask;
	u32 status_val;
	u32 status_mask;
	u32 lock_det;
};

/**
 * struct alpha_pll_ops - PLL operations
 * @enable: enable the PLL
 * @disable: disable the PLL
 * @is_enabled: check if PLL is enabled
 * @set_rate: set the PLL rate
 * @prepare: prepare the PLL for use
 * @configure: configure the PLL with given parameters
 */
struct alpha_pll_ops {
	int (*enable)(struct clk_alpha_pll *pll);
	void (*disable)(struct clk_alpha_pll *pll);
	int (*is_enabled)(struct clk_alpha_pll *pll);
	int (*set_rate)(struct clk_alpha_pll *pll,
			const struct alpha_pll_config *config,
			unsigned long rate,
			unsigned long prate);
	void (*prepare)(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config);
	void (*configure)(struct clk_alpha_pll *pll,
				const struct alpha_pll_config *config);
};

/**
 * struct clk_alpha_pll_desc - PLL description
 * @name: name of the PLL
 * @pll: pointer to the PLL structure
 * @pll_config: pointer to the PLL configuration
 * @pll_ops: pointer to the PLL operations
 * @rate: rate of the PLL
 * @prate: parent rate of the PLL
 */
struct clk_alpha_pll_desc {
	char name[16];
	struct clk_alpha_pll		*pll;
	const struct alpha_pll_config	*pll_config;
	const struct alpha_pll_ops	*pll_ops;
	unsigned long			rate;
	unsigned long			prate;
};

/**
 * struct clk_alpha_pll_tbl - PLL table
 * @pll_desc_base: pointer to the PLL description table base
 * @num_plls: number of PLLs in the table
 */
struct clk_alpha_pll_tbl {
	const struct clk_alpha_pll_desc *pll_desc_base;
	u32 num_plls;
};

/**
 * struct clk_alpha_pll_priv - PLL driver private data
 * @pll_desc: pointer to the PLL description
 * @base: base address of the PLL registers in memory
 * @fsm_vote_addr: base address of the PLL vote registers in memory
 * @fsm_vote_mask: mask for the PLL vote register
 * @configured: flag to indicate if the PLL is configured
 */
struct clk_alpha_pll_priv {
	const struct clk_alpha_pll_desc *pll_desc;
	void __iomem *base;
	void __iomem *fsm_vote_addr;
	u32 fsm_vote_mask;
	bool configured;
};

#endif
