// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "nss-switch.h"

/* Polling timeout and delay constants */
#define UNIPHY_POLLING_TIMEOUT		2000
#define UNIPHY_POLLING_DELAY		1

/* ============================================================================
 * QSERDES_COM (Common) Register Addresses
 * Address Range: 0x0000 - 0x01fc
 * ============================================================================ */
#define QSERDES_COM_CP_CTRL_MODE0_ADDRESS			0x0078
#define QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS			0x0084
#define QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS			0x0088
#define QSERDES_COM_DEC_START_MODE0_ADDRESS			0x00bc
#define QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS			0x00c0
#define QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS		0x00c4
#define QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS		0x00c8
#define QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS		0x00cc
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS		0x00d8
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS		0x00dc
#define QSERDES_COM_VCO_TUNE_MAP_ADDRESS			0x0128
#define QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS			0x012c
#define QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS			0x0130
#define QSERDES_COM_CMN_STATUS_ADDRESS				0x0140
#define QSERDES_COM_RESET_SM_STATUS_ADDRESS			0x0144
#define QSERDES_COM_RESTRIM_CTRL_ADDRESS			0x0148
#define QSERDES_COM_RESCODE_DIV_NUM_ADDRESS			0x014c
#define QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS			0x0154
#define QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS			0x0158
#define QSERDES_COM_LOCK_CMP3_MODE0_ADDRESS			0x015c
#define QSERDES_COM_CORE_CLK_EN_ADDRESS				0x0174
#define QSERDES_COM_LOCK_CMP_CFG_ADDRESS			0x017c
#define QSERDES_COM_VCO_TUNE_CTRL_ADDRESS			0x0180
#define QSERDES_COM_SYSCLK_BUF_ENABLE_ADDRESS			0x019c
#define QSERDES_COM_SSC_EN_CENTER_ADDRESS			0x01a0
#define QSERDES_COM_SSC_ADJ_PER1_ADDRESS			0x01a4
#define QSERDES_COM_SSC_ADJ_PER2_ADDRESS			0x01a8
#define QSERDES_COM_SSC_PER1_ADDRESS				0x01ac
#define QSERDES_COM_SSC_PER2_ADDRESS				0x01b0
#define QSERDES_COM_SSC_STEP_SIZE1_MODE0_ADDRESS		0x01b4
#define QSERDES_COM_SSC_STEP_SIZE2_MODE0_ADDRESS		0x01b8

/* ============================================================================
 * QSERDES_TX (Transmitter) Register Addresses
 * Address Range: 0xc000 - 0xc130
 * ============================================================================ */
#define QSERDES_TX_RES_CODE_LANE_TX_ADDRESS			0xc028
#define QSERDES_TX_RES_CODE_LANE_RX_ADDRESS			0xc02c
#define QSERDES_TX_RES_CODE_LANE_OFFSET_TX_ADDRESS		0xc030
#define QSERDES_TX_RES_CODE_LANE_OFFSET_RX_ADDRESS		0xc034
#define QSERDES_TX_TX_HR_SEL_ADDRESS				0xc054
#define QSERDES_TX_LANE_MODE_1_ADDRESS				0xc07c
#define QSERDES_TX_LANE_MODE_2_ADDRESS				0xc080
#define QSERDES_TX_LANE_MODE_3_ADDRESS				0xc084
#define QSERDES_TX_TX_BAND0_ADDRESS				0xc0e0
#define QSERDES_TX_SEL_20B_10B_ADDRESS				0xc0f8

/* ============================================================================
 * QSERDES_RX (Receiver) Register Addresses
 * Address Range: 0xc200 - 0xc5d4
 * ============================================================================ */
#define QSERDES_RX_UCDR_SO_SATURATION_ADDRESS			0xc228
#define QSERDES_RX_UCDR_PI_CTRL1_ADDRESS			0xc258
#define QSERDES_RX_UCDR_PI_CTRL2_ADDRESS			0xc25c
#define QSERDES_RX_UCDR_SB2_GAIN1_RATE3_ADDRESS			0xc298
#define QSERDES_RX_UCDR_SB2_GAIN2_RATE3_ADDRESS			0xc2ac
#define QSERDES_RX_SVS_MODE_CTRL_ADDRESS			0xc2b4
#define QSERDES_RX_RXCLK_DIV2_CTRL_ADDRESS			0xc2b8
#define QSERDES_RX_RX_BAND_CTRL0_ADDRESS			0xc2bc
#define QSERDES_RX_RX_TERM_BW_CTRL0_ADDRESS			0xc2c4
#define QSERDES_RX_RX_TERM_BW_CTRL1_ADDRESS			0xc2c8
#define QSERDES_RX_UCDR_FO_GAIN_RATE2_ADDRESS			0xc2d4
#define QSERDES_RX_UCDR_FO_GAIN_RATE3_ADDRESS			0xc2d8
#define QSERDES_RX_UCDR_SO_GAIN_RATE0_ADDRESS			0xc2e0
#define QSERDES_RX_UCDR_SO_GAIN_RATE1_ADDRESS			0xc2e4
#define QSERDES_RX_UCDR_SO_GAIN_RATE2_ADDRESS			0xc2e8
#define QSERDES_RX_UCDR_SO_GAIN_RATE3_ADDRESS			0xc2ec
#define QSERDES_RX_UCDR_SO_GAIN_RATE4_ADDRESS			0xc2f0
#define QSERDES_RX_UCDR_PI_CONTROLS_ADDRESS			0xc2f4
#define QSERDES_RX_AUXDATA_BIN_RATE01_ADDRESS			0xc304
#define QSERDES_RX_AUXDATA_BIN_RATE23_ADDRESS			0xc308
#define QSERDES_RX_AUXDATA_BIN_RATE4_ADDRESS			0xc30c
#define QSERDES_RX_RX_Q_EN_RATES_ADDRESS			0xc340
#define QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS			0xc378
#define QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE0_ADDRESS		0xc384
#define QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE1_ADDRESS		0xc388
#define QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE2_ADDRESS		0xc38c
#define QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE3_ADDRESS		0xc390
#define QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE4_ADDRESS		0xc394
#define QSERDES_RX_GM_CAL_ADDRESS				0xc398
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3_ADDRESS		0xc3a8
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4_ADDRESS		0xc3ac
#define QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1_ADDRESS		0xc3bc
#define QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL3_ADDRESS		0xc3c4
#define QSERDES_RX_SIGDET_ENABLES_ADDRESS			0xc3c8
#define QSERDES_RX_SIGDET_CNTRL_ADDRESS				0xc3cc
#define QSERDES_RX_SIGDET_LVL_ADDRESS				0xc3d0
#define QSERDES_RX_SIGDET_DEGLITCH_CNTRL_ADDRESS		0xc3d4

/* ============================================================================
 * QSERDES_TX_EXT (Transmitter Extended) Register Addresses
 * Address Range: 0xc800 - 0xc988
 * ============================================================================ */
#define QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS		0xc818
#define QSERDES_TX_EXT_DFE_TAP1_CODE_ADDRESS			0xc930

/* ============================================================================
 * QSERDES_RX_EXT (Receiver Extended) Register Addresses
 * Address Range: 0xca00 - 0xcdfc
 * ============================================================================ */
#define QSERDES_RX_EXT_RXEQ_CTRL0_ADDRESS			0xcb20
#define QSERDES_RX_EXT_RO_POWER_STATE_ADDRESS			0xcdf4
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS		0xcdfc

/* Forward declaration */
struct port_info;

/* UNIPHY 25G Register Address Configuration */
struct uniphy_25g_reg_addrs {
	u32 pcs_uniphy_option_3;
	u32 qserdes_rx_ext_rxeq_ctrl0;
	u32 qserdes_rx_ext_ro_power_state;
	u32 qserdes_rx_ext_ro_pmad_rxeq_status;
	u32 sr_pma_ctrl2;
	u32 sr_pma_kr_fec_ctrl;
	u32 sr_pma_rs_fec_ctrl;
	u32 vr_pma_cwm00;
	u32 vr_pma_cwm01;
	u32 vr_pma_cwm02;
	u32 vr_pma_cwm03;
	u32 sr_pcs_ctrl1;
	u32 sr_pcs_ctrl2;
	u32 vr_pcs_dig_ctrl1;
	u32 vr_pcs_dig_ctrl3;
	u32 vr_pcs_am_cnt;
	u32 uniphy_mode_ctrl;
};

/* Default register addresses for standard UNIPHY 25G configuration */
static const struct uniphy_25g_reg_addrs uniphy_25g_regs_default = {
	.pcs_uniphy_option_3			= 0x588,
	.qserdes_rx_ext_rxeq_ctrl0		= QSERDES_RX_EXT_RXEQ_CTRL0_ADDRESS,
	.qserdes_rx_ext_ro_power_state		= QSERDES_RX_EXT_RO_POWER_STATE_ADDRESS,
	.qserdes_rx_ext_ro_pmad_rxeq_status	= QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS,
	.sr_pma_ctrl2				= 0x10007,
	.sr_pma_kr_fec_ctrl			= 0x100ab,
	.sr_pma_rs_fec_ctrl			= 0x100c8,
	.vr_pma_cwm00				= 0x180a3,
	.vr_pma_cwm01				= 0x180a4,
	.vr_pma_cwm02				= 0x180a5,
	.vr_pma_cwm03				= 0x180a6,
	.sr_pcs_ctrl1				= 0x30000,
	.sr_pcs_ctrl2				= 0x30007,
	.vr_pcs_dig_ctrl1			= 0x38000,
	.vr_pcs_dig_ctrl3			= 0x38003,
	.vr_pcs_am_cnt				= 0x38018,
	.uniphy_mode_ctrl			= 0x46c,
};

/* VR_PCS_DIG_CTRL1 - Vendor PCS Digital Control 1 */
union vr_pcs_dig_ctrl1_u {
	u32 val;
	struct {
		u32 dskbyp:1;		/* [0] Deskew bypass */
		u32 byp_pwrup:1;	/* [1] Bypass power-up */
		u32 en_2_5g_mode:1;	/* [2] Enable 2.5G mode */
		u32 cr_cjn:1;		/* [3] Clock recovery */
		u32 dtxlaned_0:1;	/* [4] Disable TX lane 0 */
		u32 dtxlaned_3_1:3;	/* [7:5] Disable TX lanes 3:1 */
		u32 init:1;		/* [8] Initialize */
		u32 usxg_en:1;		/* [9] USXGMII enable */
		u32 usra_rst:1;		/* [10] USRA reset */
		u32 pwrsv:1;		/* [11] Power save */
		u32 cl37_bp:1;		/* [12] Clause 37 bypass */
		u32 en_vsmmd1:1;	/* [13] Enable vendor-specific MMD1 */
		u32 r2tlbe:1;		/* [14] R2TL bypass enable */
		u32 vr_rst:1;		/* [15] Vendor reset */
		u32 _reserved0:16;	/* [31:16] Reserved */
	} bf;
};

/* SR_PCS_CTRL1 - Standard PCS Control 1 */
union sr_pcs_ctrl1_u {
	u32 val;
	struct {
		u32 _reserved0:2;	/* [1:0] */
		u32 ss_5_2:4;		/* [5:2] Speed selection bits */
		u32 ss6:1;		/* [6] Speed selection bit 6 */
		u32 _reserved1:25;	/* [31:7] */
	} bf;
};

/* SR_PCS_CTRL2 - Standard PCS Control 2 */
union sr_pcs_ctrl2_u {
	u32 val;
	struct {
		u32 pcs_type_sel:4;	/* [3:0] PCS type selection */
		u32 _reserved0:28;	/* [31:4] */
	} bf;
};

/* SR_PMA_CTRL2 - Standard PMA Control 2 */
union sr_pma_ctrl2_u {
	u32 val;
	struct {
		u32 pma_type:7;	/* [6:0] PMA type */
		u32 _reserved0:25;	/* [31:7] */
	} bf;
};

/* VR_PCS_DIG_CTRL3 - Vendor PCS Digital Control 3 */
union vr_pcs_dig_ctrl3_u {
	u32 val;
	struct {
		u32 cns_en:1;		/* [0] CNS enable */
		u32 en_50g:1;		/* [1] Enable 50G */
		u32 _reserved0:30;	/* [31:2] */
	} bf;
};

/* SR_PMA_RS_FEC_CTRL - RS-FEC Control */
union sr_pma_rs_fec_ctrl_u {
	u32 val;
	struct {
		u32 _reserved0:2;	/* [1:0] */
		u32 rsfec_en:1;		/* [2] RS-FEC enable */
		u32 _reserved1:29;	/* [31:3] */
	} bf;
};

/* SR_PMA_KR_FEC_CTRL - BASE-R FEC Control */
union sr_pma_kr_fec_ctrl_u {
	u32 val;
	struct {
		u32 fec_en:1;		/* [0] FEC enable */
		u32 _reserved0:31;	/* [31:1] */
	} bf;
};

/* QSERDES_RX_EXT_RXEQ_CTRL0 - RXEQ Control */
union qserdes_rx_ext_rxeq_ctrl0_u {
	u32 val;
	struct {
		u32 _reserved0:7;		/* [6:0] */
		u32 xpcs_rxeq_en_mask:1;	/* [7] XPCS RXEQ enable mask */
		u32 _reserved1:24;		/* [31:8] */
	} bf;
};

/* PCS_UNIPHY_OPTION_3 - UNIPHY Option 3 */
union pcs_uniphy_option_3_u {
	u32 val;
	struct {
		u32 _reserved0:4;	/* [3:0] */
		u32 uniphy_start:1;	/* [4] UNIPHY start */
		u32 _reserved1:27;	/* [31:5] */
	} bf;
};

/**
 * uniphy_pma_init_hw_tuning() - Initialize PMA with hardware DFE tuning
 * @base: UNIPHY base address
 * @uniphy_index: UNIPHY instance index
 *
 * Configures PMA registers for hardware DFE tuning mode.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_init_hw_tuning(phys_addr_t base, int uniphy_index)
{
	debug("UNIPHY %d: Initializing PMA with HW DFE tuning\n", uniphy_index);

	/* Common settings */
	writel(0x01, base + QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS);

	/* QSERDES PLL Settings for 25GAUI (PLL RefClk = 46.875MHz) */
	writel(0x17, base + QSERDES_COM_CP_CTRL_MODE0_ADDRESS);
	writel(0x19, base + QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS);
	writel(0x0C, base + QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS);
	writel(0xA0, base + QSERDES_COM_DEC_START_MODE0_ADDRESS);
	writel(0x00, base + QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS);
	writel(0x00, base + QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS);
	writel(0x00, base + QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS);
	writel(0x00, base + QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS);
	writel(0x3E, base + QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS);
	writel(0x00, base + QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS);
	writel(0x02, base + QSERDES_COM_VCO_TUNE_MAP_ADDRESS);
	writel(0x24, base + QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS);
	writel(0x02, base + QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS);
	writel(0x2B, base + QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS);
	writel(0x68, base + QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS);
	writel(0x7C, base + QSERDES_COM_LOCK_CMP3_MODE0_ADDRESS);
	writel(0x18, base + QSERDES_COM_CORE_CLK_EN_ADDRESS);
	writel(0x00, base + QSERDES_COM_LOCK_CMP_CFG_ADDRESS);
	writel(0x00, base + QSERDES_COM_VCO_TUNE_CTRL_ADDRESS);
	writel(0x0C, base + QSERDES_COM_SYSCLK_BUF_ENABLE_ADDRESS);
	writel(0x02, base + QSERDES_COM_SSC_EN_CENTER_ADDRESS);
	writel(0x31, base + QSERDES_COM_SSC_ADJ_PER1_ADDRESS);
	writel(0x00, base + QSERDES_COM_SSC_ADJ_PER2_ADDRESS);
	writel(0x31, base + QSERDES_COM_SSC_PER1_ADDRESS);
	writel(0x01, base + QSERDES_COM_SSC_PER2_ADDRESS);
	writel(0x85, base + QSERDES_COM_SSC_STEP_SIZE1_MODE0_ADDRESS);
	writel(0x07, base + QSERDES_COM_SSC_STEP_SIZE2_MODE0_ADDRESS);

	/* TX Settings */
	writel(0x05, base + QSERDES_TX_RES_CODE_LANE_TX_ADDRESS);
	writel(0x07, base + QSERDES_TX_RES_CODE_LANE_RX_ADDRESS);
	writel(0x00, base + QSERDES_TX_RES_CODE_LANE_OFFSET_TX_ADDRESS);
	writel(0x00, base + QSERDES_TX_RES_CODE_LANE_OFFSET_RX_ADDRESS);
	writel(0x0C, base + QSERDES_TX_TX_HR_SEL_ADDRESS);
	writel(0x05, base + QSERDES_TX_LANE_MODE_1_ADDRESS);
	writel(0xC2, base + QSERDES_TX_LANE_MODE_2_ADDRESS);
	writel(0x07, base + QSERDES_TX_LANE_MODE_3_ADDRESS);
	writel(0x18, base + QSERDES_TX_TX_BAND0_ADDRESS);
	writel(0x01, base + QSERDES_TX_SEL_20B_10B_ADDRESS);

	/* RX Settings */
	writel(0x7F, base + QSERDES_RX_UCDR_SO_SATURATION_ADDRESS);
	writel(0x16, base + QSERDES_RX_UCDR_PI_CTRL1_ADDRESS);
	writel(0x7F, base + QSERDES_RX_UCDR_PI_CTRL2_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SB2_GAIN1_RATE3_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SB2_GAIN2_RATE3_ADDRESS);
	writel(0x04, base + QSERDES_RX_SVS_MODE_CTRL_ADDRESS);
	writel(0x02, base + QSERDES_RX_RXCLK_DIV2_CTRL_ADDRESS);
	writel(0x1B, base + QSERDES_RX_RX_BAND_CTRL0_ADDRESS);
	writel(0x4F, base + QSERDES_RX_RX_TERM_BW_CTRL0_ADDRESS);
	writel(0x9F, base + QSERDES_RX_RX_TERM_BW_CTRL1_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_FO_GAIN_RATE2_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_FO_GAIN_RATE3_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SO_GAIN_RATE0_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SO_GAIN_RATE1_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SO_GAIN_RATE2_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SO_GAIN_RATE3_ADDRESS);
	writel(0x04, base + QSERDES_RX_UCDR_SO_GAIN_RATE4_ADDRESS);
	writel(0x81, base + QSERDES_RX_UCDR_PI_CONTROLS_ADDRESS);
	writel(0x09, base + QSERDES_RX_AUXDATA_BIN_RATE01_ADDRESS);
	writel(0x09, base + QSERDES_RX_AUXDATA_BIN_RATE23_ADDRESS);
	writel(0x09, base + QSERDES_RX_AUXDATA_BIN_RATE4_ADDRESS);
	writel(0x02, base + QSERDES_RX_RX_Q_EN_RATES_ADDRESS);
	writel(0x0C, base + QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS);
	writel(0x0E, base + QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE0_ADDRESS);
	writel(0x0E, base + QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE1_ADDRESS);
	writel(0x0E, base + QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE2_ADDRESS);
	writel(0x0E, base + QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE3_ADDRESS);
	writel(0x0E, base + QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE4_ADDRESS);
	writel(0x42, base + QSERDES_RX_GM_CAL_ADDRESS);
	writel(0x0A, base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3_ADDRESS);
	writel(0x0A, base + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4_ADDRESS);
	writel(0x80, base + QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1_ADDRESS);
	writel(0x00, base + QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL3_ADDRESS);
	writel(0x0E, base + QSERDES_RX_SIGDET_ENABLES_ADDRESS);
	writel(0x03, base + QSERDES_RX_SIGDET_CNTRL_ADDRESS);
	writel(0x2B, base + QSERDES_RX_SIGDET_LVL_ADDRESS);
	writel(0x08, base + QSERDES_RX_SIGDET_DEGLITCH_CNTRL_ADDRESS);

	debug("UNIPHY %d: PMA HW tuning initialization completed\n", uniphy_index);
	return 0;
}

/**
 * uniphy_pma_dfe_sw_tune() - Perform PMA DFE software tuning
 * @base: UNIPHY base address
 * @uniphy_index: UNIPHY instance index
 *
 * Performs software DFE tuning sequence if PMA was initialized with SW tuning mode.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_dfe_sw_tune(phys_addr_t base, int uniphy_index)
{
	u32 timeout = UNIPHY_POLLING_TIMEOUT;
	u32 reg_val, tap1_code;

	debug("UNIPHY %d: Starting PMA DFE software tuning\n", uniphy_index);

	/* Poll QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS until bit[4] VGA_DONE is 0x1 */
	while (timeout > 0) {
		reg_val = readl(base + QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS);
		if (reg_val & (1 << 4)) {
			debug("VGA_DONE detected\n");
			break;
		}
		mdelay(UNIPHY_POLLING_DELAY);
		timeout--;
	}

	if (timeout == 0) {
		printf("ERROR: VGA_DONE polling timeout\n");
		return -ETIMEDOUT;
	}

	/* Write DFE TAP1 code sequence (0x80~0x8A) with 10us delays between each write */
	for (tap1_code = 0x80; tap1_code <= 0x8A; tap1_code++) {
		writel(tap1_code, base + QSERDES_TX_EXT_DFE_TAP1_CODE_ADDRESS);
		udelay(10);
	}

	debug("UNIPHY %d: PMA DFE software tuning completed\n", uniphy_index);
	return 0;
}

/**
 * uniphy_25g_pcs_set() - Configure 25G PCS
 * @base: UNIPHY base address
 * @regs: Register address configuration
 *
 * Configures the 25GBASE-R PCS with RS-FEC.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_25g_pcs_set(phys_addr_t base, int uniphy_index, const struct uniphy_25g_reg_addrs *regs)
{
	union sr_pcs_ctrl1_u pcs_ctrl1;
	union sr_pcs_ctrl2_u pcs_ctrl2;
	union sr_pma_ctrl2_u pma_ctrl2;
	union vr_pcs_dig_ctrl3_u dig_ctrl3;
	union vr_pcs_dig_ctrl1_u dig_ctrl1;
	union sr_pma_rs_fec_ctrl_u rs_fec;
	union sr_pma_kr_fec_ctrl_u kr_fec;
	u32 timeout = UNIPHY_POLLING_TIMEOUT;
	u32 am_cnt;

	debug("Configuring 25G PCS\n");

	/* Configure SR_PCS_CTRL1 - Speed selection for 25G */
	pcs_ctrl1.val = csr_read(uniphy_index, CSR2_ADDR(regs->sr_pcs_ctrl1));
	pcs_ctrl1.bf.ss_5_2 = 0x5; /* 4'b0101 for 25G */
	csr_write(uniphy_index, CSR2_ADDR(regs->sr_pcs_ctrl1), pcs_ctrl1.val);

	/* Configure SR_PCS_CTRL2 - PCS type for 25GBASE-R */
	pcs_ctrl2.val = csr_read(uniphy_index, CSR2_ADDR(regs->sr_pcs_ctrl2));
	pcs_ctrl2.bf.pcs_type_sel = 0x7; /* 4'b0111 for 25GBASE-R */
	csr_write(uniphy_index, CSR2_ADDR(regs->sr_pcs_ctrl2), pcs_ctrl2.val);

	/* Configure SR_PMA_CTRL2 - PMA type for 25GBASE-KR */
	pma_ctrl2.val = csr_read(uniphy_index, CSR2_ADDR(regs->sr_pma_ctrl2));
	pma_ctrl2.bf.pma_type = 0x39; /* 7'b0111001 for 25GBASE-KR */
	csr_write(uniphy_index, CSR2_ADDR(regs->sr_pma_ctrl2), pma_ctrl2.val);

	/* Configure VR_PCS_DIG_CTRL3 - Disable 50G, enable CNS */
	dig_ctrl3.val = csr_read(uniphy_index, CSR2_ADDR(regs->vr_pcs_dig_ctrl3));
	dig_ctrl3.bf.en_50g = 0;  /* Disable 50G mode */
	dig_ctrl3.bf.cns_en = 0;  /* CNS enable (set to 0 as per SSDK) */
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pcs_dig_ctrl3), dig_ctrl3.val);

	/* Enable RS-FEC */

	/* Program AM interval period in VR_PCS_AM_CNT to 14'h400 (1024) */
	am_cnt = csr_read(uniphy_index, CSR2_ADDR(regs->vr_pcs_am_cnt));
	am_cnt = (am_cnt & ~0x3FFF) | 0x400; /* PCS_AM_CNT = 14'h400 */
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pcs_am_cnt), am_cnt);

	/* Program RS-FEC Code Word Markers */
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pma_cwm00), 0x68C1);
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pma_cwm01), 0x3321);
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pma_cwm02), 0x973E);
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pma_cwm03), 0xCCDE);

	/* Enable RS-FEC */
	rs_fec.val = csr_read(uniphy_index, CSR2_ADDR(regs->sr_pma_rs_fec_ctrl));
	rs_fec.bf.rsfec_en = 1; /* Enable RS-FEC */
	csr_write(uniphy_index, CSR2_ADDR(regs->sr_pma_rs_fec_ctrl), rs_fec.val);

	/* Disable BASE-R FEC (alternative to RS-FEC) */
	kr_fec.val = csr_read(uniphy_index, CSR2_ADDR(regs->sr_pma_kr_fec_ctrl));
	kr_fec.bf.fec_en = 0; /* Disable BASE-R FEC */
	csr_write(uniphy_index, CSR2_ADDR(regs->sr_pma_kr_fec_ctrl), kr_fec.val);

	/* Assert vendor reset */
	dig_ctrl1.val = csr_read(uniphy_index, CSR2_ADDR(regs->vr_pcs_dig_ctrl1));
	dig_ctrl1.bf.vr_rst = 1; /* Assert VR_RST */
	csr_write(uniphy_index, CSR2_ADDR(regs->vr_pcs_dig_ctrl1), dig_ctrl1.val);

	/* Poll until VR_RST self-clears */
	while (timeout > 0) {
		dig_ctrl1.val = csr_read(uniphy_index, CSR2_ADDR(regs->vr_pcs_dig_ctrl1));
		if (dig_ctrl1.bf.vr_rst == 0)
			break;

		mdelay(UNIPHY_POLLING_DELAY);
		timeout--;
	}

	if (timeout == 0) {
		printf("ERROR: VR_RST polling timeout\n");
		return -ETIMEDOUT;
	}

	debug("25G PCS configuration completed\n");
	return 0;
}

/**
 * uniphy_rxeq_status_check() - Check RXEQ status by polling
 * @base: UNIPHY base address
 * @regs: Register address configuration
 *
 * Polls the RXEQ_ENGINE_DONE bit to verify RXEQ engine completion.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_rxeq_status_check(phys_addr_t base, const struct uniphy_25g_reg_addrs *regs)
{
	u32 reg_val;
	u32 timeout = UNIPHY_POLLING_TIMEOUT;

	debug("Starting RXEQ status check\n");

	/* Poll RXEQ_ENGINE_DONE bit[1] until it becomes 1 */
	while (timeout > 0) {
		reg_val = readl(base + regs->qserdes_rx_ext_ro_pmad_rxeq_status);
		if (reg_val & (1 << 1)) {
			debug("RXEQ engine completed\n");
			return 0;
		}

		mdelay(UNIPHY_POLLING_DELAY);
		timeout--;
	}

	printf("ERROR: RXEQ engine done polling timeout\n");
	return -ETIMEDOUT;
}

/**
 * uniphy_calibrate() - UNIPHY calibration function
 * @base: UNIPHY base address
 * @regs: Register address configuration
 *
 * Performs UNIPHY calibration sequence.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_calibrate(phys_addr_t base, const struct uniphy_25g_reg_addrs *regs)
{
	union pcs_uniphy_option_3_u option3;
	u32 reg_val;
	u32 retries = UNIPHY_POLLING_TIMEOUT;

	debug("Starting calibration\n");

	/* Set UNIPHY_START to trigger calibration */
	option3.val = readl(base + regs->pcs_uniphy_option_3);
	option3.bf.uniphy_start = 1;
	writel(option3.val, base + regs->pcs_uniphy_option_3);

	/* Wait for traffic state - read QSERDES_RX_EXT_RO_POWER_STATE[15:0] = 0xf */
	reg_val = 0;
	while ((reg_val & 0xF) != 0xF) {
		mdelay(UNIPHY_POLLING_DELAY);
		if (retries-- == 0) {
			printf("ERROR: 25G calibration timeout\n");
			return -ETIMEDOUT;
		}
		reg_val = readl(base + regs->qserdes_rx_ext_ro_power_state);
	}

	debug("25G Calibration completed successfully\n");
	return 0;
}

/**
 * uniphy_xlgpcs_reset() - Assert or deassert the XLGPCS reset for a UNIPHY instance
 * @port: Port information structure containing uniphy_id and dev
 * @enable: true to assert reset (keep XPCS in reset), false to deassert (release reset)
 *
 * Uses the GCC reset control named "uniphy%d_xlgpcs_rst" to manage the
 * XLGPCS reset line. Only UNIPHY instances 1 and 2 support XLGPCS.
 */
static void uniphy_xlgpcs_reset(struct port_info *port, bool enable)
{
	struct reset_ctl rst;
	char name[32];
	int ret;

	if (!port || !port->dev)
		return;

	if (port->uniphy_id == 0) {
		printf("ERROR: UNIPHY 0 does not support XLGPCS reset\n");
		return;
	}

	snprintf(name, sizeof(name), "uniphy%d_xlgpcs_rst", port->uniphy_id);

	ret = reset_get_by_name(port->dev, name, &rst);
	if (ret) {
		printf("ERROR: Failed to get reset '%s': %d\n", name, ret);
		return;
	}

	if (enable)
		reset_assert(&rst);
	else
		reset_deassert(&rst);
}

/**
 * uniphy_25g_r_mode_set() - Configure UNIPHY for 25G-R mode
 * @port: Port information structure containing uniphy_base and uniphy_id
 *
 * Configures UNIPHY for 25GBASE-R operation with XLGPCS.
 * This is the main entry point for 25G link setup.
 * UNIPHY 0 is not supported for XLGPCS.
 *
 * Return: 0 on success, negative error code on failure
 */
int uniphy_25g_r_mode_set(struct port_info *port)
{
	const struct uniphy_25g_reg_addrs *regs = &uniphy_25g_regs_default;
	union uniphy_mode_ctrl_u mode_ctrl;
	union qserdes_rx_ext_rxeq_ctrl0_u rxeq_ctrl;
	phys_addr_t base = port->uniphy_base;
	int uniphy_index = port->uniphy_id;
	int ret;

	if (uniphy_index == 0) {
		printf("ERROR: UNIPHY 0 does not support XLGPCS\n");
		return -EINVAL;
	}

	debug("UNIPHY %d: Configuring 25G-R mode\n", uniphy_index);

	/* Initialize PMA settings with hardware DFE tuning */
	ret = uniphy_pma_init_hw_tuning(base, uniphy_index);
	if (ret) {
		printf("ERROR: UNIPHY %d PMA initialization failed\n", uniphy_index);
		return ret;
	}

	/* Disable instance clock */
	clk_disable(&port->rx_clk);
	clk_disable(&port->tx_clk);

	/* Keep XPCS in reset */
	uniphy_xlgpcs_reset(port, true);

	/* Configure UNIPHY mode control for XLGPCS */
	mode_ctrl.val = readl(base + regs->uniphy_mode_ctrl);

	/* Configure for XLGPCS mode - clear all mode bits, enable only XLGPCS */
	mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii = 0;  /* Disable SGMII */
	mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii = 0; /* Disable PSGMII/QSGMII */
	mode_ctrl.bf.newaddedfromhere_sg_mode = 0;           /* Disable SG mode */
	mode_ctrl.bf.newaddedfromhere_sgplus_mode = 0;       /* Disable SGMII+ */
	mode_ctrl.bf.newaddedfromhere_xpcs_mode = 0;         /* Disable XPCS */
	mode_ctrl.bf.newaddedfromhere_usxg_en = 0;           /* Disable USXG */
	mode_ctrl.bf.newaddedfromhere_xlgpcs_en = 1;         /* Enable XLGPCS for 25G */

	writel(mode_ctrl.val, base + regs->uniphy_mode_ctrl);

	/*
	 * Note: GCC software reset should be handled by platform code
	 */

	/* Enable XPCS RXEQ mask */
	rxeq_ctrl.val = readl(base + regs->qserdes_rx_ext_rxeq_ctrl0);
	rxeq_ctrl.bf.xpcs_rxeq_en_mask = 1;
	writel(rxeq_ctrl.val, base + regs->qserdes_rx_ext_rxeq_ctrl0);

	/* Wait for calibration to complete */
	ret = uniphy_calibrate(base, regs);
	if (ret) {
		printf("ERROR: UNIPHY %d calibration failed\n", uniphy_index);
		return ret;
	}

	/* Enable instance clock */
	clk_enable(&port->rx_clk);
	clk_enable(&port->tx_clk);

	/* Release XPCS reset */
	uniphy_xlgpcs_reset(port, false);

	/* Configure 25G XLGPCS settings */
	ret = uniphy_25g_pcs_set(base, uniphy_index, regs);
	if (ret) {
		printf("ERROR: UNIPHY %d 25G PCS configuration failed\n", uniphy_index);
		return ret;
	}

	/* Check RXEQ status for JHPPE */
	ret = uniphy_rxeq_status_check(base, regs);
	if (ret) {
		printf("WARNING: UNIPHY %d RXEQ status check failed\n", uniphy_index);
		/* Continue anyway as this might not be critical */
	}

	/* Disable XPCS RXEQ mask */
	rxeq_ctrl.val = readl(base + regs->qserdes_rx_ext_rxeq_ctrl0);
	rxeq_ctrl.bf.xpcs_rxeq_en_mask = 0;
	writel(rxeq_ctrl.val, base + regs->qserdes_rx_ext_rxeq_ctrl0);

	/* Optional: Perform software DFE tuning if needed */
#if 0
	ret = uniphy_pma_dfe_sw_tune(base, uniphy_index);
	if (ret) {
		printf("WARNING: UNIPHY %d PMA DFE SW tuning failed\n", uniphy_index);
		/* Continue anyway as this is optional */
	}
#endif

	printf("UNIPHY %d: 25G-R mode configuration completed\n", uniphy_index);
	return 0;
}
