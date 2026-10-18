/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "adpt.h"
#include "ssdk_clk.h"
#include "jhppe_uniphy_reg.h"
#include "jhppe_uniphy.h"
#include "adpt_hppe_uniphy.h"
#include "hppe_reg_access.h"

#define ADPT_JHPPE_UNIPHY_POLLING_TIMEOUT	2000
#define ADPT_JHPPE_UNIPHY_POLLING_DELAY		1

/* CSR3 PMA settings registers */
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
#define QSERDES_RX_RX_MODE_RATE_0_1_B0_ADDRESS			0xc408
#define QSERDES_RX_RX_MODE_RATE_0_1_B1_ADDRESS			0xc40c
#define QSERDES_RX_RX_MODE_RATE_0_1_B2_ADDRESS			0xc410
#define QSERDES_RX_RX_MODE_RATE_0_1_B3_ADDRESS			0xc414
#define QSERDES_RX_RX_MODE_RATE_0_1_B4_ADDRESS			0xc418
#define QSERDES_RX_RX_MODE_RATE_0_1_B5_ADDRESS			0xc41c
#define QSERDES_RX_RX_MODE_RATE_0_1_B6_ADDRESS			0xc420
#define QSERDES_RX_RX_MODE_RATE_0_1_B7_ADDRESS			0xc424
#define QSERDES_RX_RX_MODE_RATE_0_1_B8_ADDRESS			0xc428
#define QSERDES_RX_RX_MODE_RATE2_B0_ADDRESS			0xc42c
#define QSERDES_RX_RX_MODE_RATE2_B1_ADDRESS			0xc430
#define QSERDES_RX_RX_MODE_RATE2_B2_ADDRESS			0xc434
#define QSERDES_RX_RX_MODE_RATE2_B3_ADDRESS			0xc438
#define QSERDES_RX_RX_MODE_RATE2_B4_ADDRESS			0xc43c
#define QSERDES_RX_RX_MODE_RATE2_B5_ADDRESS			0xc440
#define QSERDES_RX_RX_MODE_RATE2_B6_ADDRESS			0xc444
#define QSERDES_RX_RX_MODE_RATE2_B7_ADDRESS			0xc448
#define QSERDES_RX_RX_MODE_RATE2_B8_ADDRESS			0xc44c
#define QSERDES_RX_RX_MODE_RATE3_B0_ADDRESS			0xc450
#define QSERDES_RX_RX_MODE_RATE3_B1_ADDRESS			0xc454
#define QSERDES_RX_RX_MODE_RATE3_B2_ADDRESS			0xc458
#define QSERDES_RX_RX_MODE_RATE3_B3_ADDRESS			0xc45c
#define QSERDES_RX_RX_MODE_RATE3_B4_ADDRESS			0xc460
#define QSERDES_RX_RX_MODE_RATE3_B5_ADDRESS			0xc464
#define QSERDES_RX_RX_MODE_RATE3_B6_ADDRESS			0xc468
#define QSERDES_RX_RX_MODE_RATE3_B7_ADDRESS			0xc46c
#define QSERDES_RX_RX_MODE_RATE3_B8_ADDRESS			0xc470
#define QSERDES_RX_RX_MODE_RATE4_B0_ADDRESS			0xc474
#define QSERDES_RX_RX_MODE_RATE4_B1_ADDRESS			0xc478
#define QSERDES_RX_RX_MODE_RATE4_B2_ADDRESS			0xc47c
#define QSERDES_RX_RX_MODE_RATE4_B3_ADDRESS			0xc480
#define QSERDES_RX_RX_MODE_RATE4_B4_ADDRESS			0xc484
#define QSERDES_RX_RX_MODE_RATE4_B5_ADDRESS			0xc488
#define QSERDES_RX_RX_MODE_RATE4_B6_ADDRESS			0xc48c
#define QSERDES_RX_RX_MODE_RATE4_B7_ADDRESS			0xc490
#define QSERDES_RX_RX_MODE_RATE4_B8_ADDRESS			0xc494
#define QSERDES_RX_DFE_EN_TIMER_ADDRESS				0xc498
#define QSERDES_RX_DLL_CTRL1_ADDRESS				0xc4e0
#define QSERDES_RX_DLL_CTRL2_ADDRESS				0xc4e4
#define QSERDES_RX_DLL0_FTUNE_CTRL_ADDRESS			0xc4f8
#define QSERDES_RX_DLL_HR_DCC_ICLK_CTRL1_ADDRESS		0xc50c

/* ============================================================================
 * QSERDES_TX_EXT (Transmitter Extended) Register Addresses
 * Address Range: 0xc800 - 0xc988
 * ============================================================================ */
#define QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS		0xc818
#define QSERDES_TX_EXT_EMP_POST1_LVL_RATE4_ADDRESS		0xc82c
#define QSERDES_TX_EXT_EMP_POST1_LVL_RATE5_ADDRESS		0xc830
#define QSERDES_TX_EXT_EMP_POST1_LVL_RATE6_ADDRESS		0xc834
#define QSERDES_TX_EXT_PREEMPH_RATE6_ADDRESS			0xc850
#define QSERDES_TX_EXT_ADAPTOR_MODE_CTRL1_ADDRESS		0xc8f8
#define QSERDES_TX_EXT_ADAPTOR_MODE_CTRL3_ADDRESS		0xc900
#define QSERDES_TX_EXT_TAP1ADP_CTRL1_ADDRESS			0xc918
#define QSERDES_TX_EXT_TAP1CODE_MAN_VAL_ADDRESS			0xc920
#define QSERDES_TX_EXT_TAP1CODE_TARGET_VAL_ADDRESS		0xc924
#define QSERDES_TX_EXT_TAP1CODE_STEP_VAL_ADDRESS		0xc928
#define QSERDES_TX_EXT_TAP1CODE_STEP_TIME_ADDRESS		0xc92c
#define QSERDES_TX_EXT_DFE_TAP1_CODE_ADDRESS			0xc930
#define QSERDES_TX_EXT_LANE_MODE_BITS_RATE56_ADDRESS		0xc950
#define QSERDES_TX_EXT_RX_MODE_RATE1_B0_ADDRESS			0xc954
#define QSERDES_TX_EXT_RX_MODE_RATE1_B1_ADDRESS			0xc958
#define QSERDES_TX_EXT_RX_MODE_RATE1_B2_ADDRESS			0xc95c
#define QSERDES_TX_EXT_RX_MODE_RATE1_B3_ADDRESS			0xc960
#define QSERDES_TX_EXT_RX_MODE_RATE1_B4_ADDRESS			0xc964
#define QSERDES_TX_EXT_RX_MODE_RATE1_B5_ADDRESS			0xc968
#define QSERDES_TX_EXT_RX_MODE_RATE1_B6_ADDRESS			0xc96c
#define QSERDES_TX_EXT_RX_MODE_RATE1_B7_ADDRESS			0xc970
#define QSERDES_TX_EXT_RX_MODE_RATE1_B8_ADDRESS			0xc974

/* ============================================================================
 * QSERDES_RX_EXT (Receiver Extended) Register Addresses
 * Address Range: 0xca00 - 0xcdfc
 * ============================================================================ */
#define QSERDES_RX_EXT_UCDR_FASTLOCK_FO_GAIN_RATE6_ADDRESS	0xca08
#define QSERDES_RX_EXT_UCDR_FASTLOCK_SO_GAIN_RATE6_ADDRESS	0xca10
#define QSERDES_RX_EXT_UCDR_FASTLOCK_COUNT_HIGH_RATE6_ADDRESS	0xca20
#define QSERDES_RX_EXT_RX_TERM_BW_CTRL2_ADDRESS			0xca54
#define QSERDES_RX_EXT_UCDR_FO_GAIN_RATE6_ADDRESS		0xca5c
#define QSERDES_RX_EXT_UCDR_SO_GAIN_RATE5_ADDRESS		0xca60
#define QSERDES_RX_EXT_UCDR_SO_GAIN_RATE6_ADDRESS		0xca64
#define QSERDES_RX_EXT_AUXDATA_BIN_RATE56_ADDRESS		0xca6c
#define QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE5_ADDRESS	0xca74
#define QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE6_ADDRESS	0xca78
#define QSERDES_RX_EXT_RX_MODE_RATE5_B0_ADDRESS			0xca7c
#define QSERDES_RX_EXT_RX_MODE_RATE5_B1_ADDRESS			0xca80
#define QSERDES_RX_EXT_RX_MODE_RATE5_B2_ADDRESS			0xca84
#define QSERDES_RX_EXT_RX_MODE_RATE5_B3_ADDRESS			0xca88
#define QSERDES_RX_EXT_RX_MODE_RATE5_B4_ADDRESS			0xca8c
#define QSERDES_RX_EXT_RX_MODE_RATE5_B5_ADDRESS			0xca90
#define QSERDES_RX_EXT_RX_MODE_RATE5_B6_ADDRESS			0xca94
#define QSERDES_RX_EXT_RX_MODE_RATE5_B7_ADDRESS			0xca98
#define QSERDES_RX_EXT_RX_MODE_RATE5_B8_ADDRESS			0xca9c
#define QSERDES_RX_EXT_RX_MODE_RATE5_B9_ADDRESS			0xcaa0
#define QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS			0xcaa4
#define QSERDES_RX_EXT_RX_MODE_RATE6_B1_ADDRESS			0xcaa8
#define QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS			0xcaac
#define QSERDES_RX_EXT_RX_MODE_RATE6_B3_ADDRESS			0xcab0
#define QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS			0xcab4
#define QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS			0xcab8
#define QSERDES_RX_EXT_RX_MODE_RATE6_B6_ADDRESS			0xcabc
#define QSERDES_RX_EXT_RX_MODE_RATE6_B7_ADDRESS			0xcac0
#define QSERDES_RX_EXT_RX_MODE_RATE6_B8_ADDRESS			0xcac4
#define QSERDES_RX_EXT_RX_MODE_RATE6_B9_ADDRESS			0xcac8
#define QSERDES_RX_EXT_Q_PI_INTRINSIC_BIAS_RATE65_ADDRESS	0xcacc
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B0_ADDRESS		0xcad4
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B1_ADDRESS		0xcad8
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B0_ADDRESS		0xcadc
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B1_ADDRESS		0xcae0
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B0_ADDRESS		0xcae4
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B1_ADDRESS		0xcae8
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B0_ADDRESS		0xcaec
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B1_ADDRESS		0xcaf0
#define QSERDES_RX_EXT_DCC_CLK_EXTRA_DIV2_EN_RATE_CTRL0_ADDRESS	0xcaf4
#define QSERDES_RX_EXT_DLL_FTUNE_CAL_TIME_RATE456_ADDRESS	0xcba8
#define QSERDES_RX_EXT_RXEQ_CTRL3_ADDRESS			0xcb2c
#define QSERDES_RX_EXT_RXEQ_CTRL5_ADDRESS			0xcb34
#define QSERDES_RX_EXT_RXEQ_CTRL9_ADDRESS			0xcb44
#define QSERDES_RX_EXT_RX_EYEMON_CTRL0_ADDRESS			0xcb70
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL4_ADDRESS		0xcc74
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL5_ADDRESS		0xcc78
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL6_ADDRESS		0xcc7c
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL7_ADDRESS		0xcc80
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL8_ADDRESS		0xcc84
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL9_ADDRESS		0xcc88
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL10_ADDRESS	0xcc8c
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL11_ADDRESS	0xcc90
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL12_ADDRESS	0xcc94
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL13_ADDRESS	0xcc98
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL14_ADDRESS	0xcc9c
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL15_ADDRESS	0xcca0
#define QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL5_ADDRESS		0xcca4
#define QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL6_ADDRESS		0xcca8
#define QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS		0xccac
#define QSERDES_RX_EXT_VGA_CAL_MAN_VAL2_ADDRESS			0xccc8
#define QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS			0xcccc
#define QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS			0xccd0
#define QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE12_ADDRESS		0xcd60
#define QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE34_ADDRESS		0xcd64
#define QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE56_ADDRESS		0xcd68
#define QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE4_ADDRESS		0xcd78
#define QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE5_ADDRESS		0xcd7c
#define QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE6_ADDRESS		0xcd80
#define QSERDES_RX_EXT_RXEQ_CTRL26_ADDRESS			0xcdc8
#define QSERDES_RX_EXT_RXEQ_CTRL27_ADDRESS			0xcdcc
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B0_ADDRESS		0xcdd0
#define QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B1_ADDRESS		0xcdd4
#define QSERDES_RX_EXT_DFE_SUPPORTED_ADDRESS			0xcde8

/* ============================================================================
 * QSERDES_COM (Common PLL) Register Addresses
 * Address Range: 0xd000 - 0xd2fc
 * ============================================================================ */
#define QSERDES_COM_CP_CTRL_MODE1_ADDRESS			0xd010
#define QSERDES_COM_PLL_RCTRL_MODE1_ADDRESS			0xd014
#define QSERDES_COM_PLL_CCTRL_MODE1_ADDRESS			0xd018
#define QSERDES_COM_CORECLK_DIV_MODE1_ADDRESS			0xd01c
#define QSERDES_COM_LOCK_CMP1_MODE1_ADDRESS			0xd020
#define QSERDES_COM_LOCK_CMP2_MODE1_ADDRESS			0xd024
#define QSERDES_COM_DEC_START_MODE1_ADDRESS			0xd028
#define QSERDES_COM_DEC_START_MSB_MODE1_ADDRESS			0xd02c
#define QSERDES_COM_DIV_FRAC_START1_MODE1_ADDRESS		0xd030
#define QSERDES_COM_DIV_FRAC_START2_MODE1_ADDRESS		0xd034
#define QSERDES_COM_DIV_FRAC_START3_MODE1_ADDRESS		0xd038
#define QSERDES_COM_HSCLK_SEL_1_ADDRESS				0xd03c
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE1_ADDRESS		0xd040
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE1_ADDRESS		0xd044
#define QSERDES_COM_VCO_TUNE1_MODE1_ADDRESS			0xd048
#define QSERDES_COM_VCO_TUNE2_MODE1_ADDRESS			0xd04c
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE1_ADDRESS		0xd050
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE1_ADDRESS		0xd054
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0_ADDRESS		0xd058
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0_ADDRESS		0xd05c
#define QSERDES_COM_CP_CTRL_MODE0_ADDRESS			0xd070
#define QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS			0xd074
#define QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS			0xd078
#define QSERDES_COM_CORECLK_DIV_MODE0_ADDRESS			0xd07c
#define QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS			0xd080
#define QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS			0xd084
#define QSERDES_COM_DEC_START_MODE0_ADDRESS			0xd088
#define QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS			0xd08c
#define QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS		0xd090
#define QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS		0xd094
#define QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS		0xd098
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS		0xd0a0
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS		0xd0a4
#define QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS			0xd0a8
#define QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS			0xd0ac
#define QSERDES_COM_BG_TIMER_ADDRESS				0xd0bc
#define QSERDES_COM_SSC_EN_CENTER_ADDRESS			0xd0c0
#define QSERDES_COM_POST_DIV_MUX_ADDRESS			0xd0d8
#define QSERDES_COM_SYS_CLK_CTRL_ADDRESS			0xd0e4
#define QSERDES_COM_PLL_IVCO_ADDRESS				0xd0f4
#define QSERDES_COM_PLL_IVCO_MODE1_ADDRESS			0xd0f8
#define QSERDES_COM_PLL_CNTRL_ADDRESS				0xd108
#define QSERDES_COM_SYSCLK_EN_SEL_ADDRESS			0xd110
#define QSERDES_COM_LOCK_CMP_EN_ADDRESS				0xd120
#define QSERDES_COM_VCO_TUNE_INITVAL1_ADDRESS			0xd144
#define QSERDES_COM_VCO_TUNE_INITVAL2_ADDRESS			0xd148
#define QSERDES_COM_CORE_CLK_EN_ADDRESS				0xd170
#define QSERDES_COM_CMN_CONFIG_1_ADDRESS			0xd174
#define QSERDES_COM_SVS_MODE_CLK_SEL_ADDRESS			0xd17c
#define QSERDES_COM_CMN_MODE_ADDRESS				0xd188
#define QSERDES_COM_CMN_MODE_CONTD_ADDRESS			0xd18c
#define QSERDES_COM_CMN_MODE_CONTD1_ADDRESS			0xd190
#define QSERDES_COM_CMN_MODE_CONTD2_ADDRESS			0xd194
#define QSERDES_COM_VCO_DC_LEVEL_CTRL_ADDRESS			0xd198
#define QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_1_ADDRESS		0xd19c
#define QSERDES_COM_ADDITIONAL_MISC_2_ADDRESS			0xd1b8
#define QSERDES_COM_ADDITIONAL_MISC_3_ADDRESS			0xd1bc
#define QSERDES_COM_CP_CTRL_MODE2_ADDRESS			0xd218
#define QSERDES_COM_PLL_RCTRL_MODE2_ADDRESS			0xd21c
#define QSERDES_COM_PLL_CCTRL_MODE2_ADDRESS			0xd220
#define QSERDES_COM_CORECLK_DIV_MODE2_ADDRESS			0xd224
#define QSERDES_COM_LOCK_CMP1_MODE2_ADDRESS			0xd228
#define QSERDES_COM_LOCK_CMP2_MODE2_ADDRESS			0xd22c
#define QSERDES_COM_DEC_START_MODE2_ADDRESS			0xd230
#define QSERDES_COM_DEC_START_MSB_MODE2_ADDRESS			0xd234
#define QSERDES_COM_DIV_FRAC_START1_MODE2_ADDRESS		0xd238
#define QSERDES_COM_DIV_FRAC_START2_MODE2_ADDRESS		0xd23c
#define QSERDES_COM_DIV_FRAC_START3_MODE2_ADDRESS		0xd240
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE2_ADDRESS		0xd244
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE2_ADDRESS		0xd248
#define QSERDES_COM_VCO_TUNE1_MODE2_ADDRESS			0xd24c
#define QSERDES_COM_VCO_TUNE2_MODE2_ADDRESS			0xd250
#define QSERDES_COM_PLL_IVCO_MODE2_ADDRESS			0xd254
#define QSERDES_COM_HSCLK_SEL_2_ADDRESS				0xd258
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE2_ADDRESS		0xd25c
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE2_ADDRESS		0xd260
#define QSERDES_COM_CMN_CONFIG_2_ADDRESS			0xd268
#define QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_2_ADDRESS		0xd26c
#define QSERDES_COM_CP_CTRL_MODE3_ADDRESS			0xd270
#define QSERDES_COM_PLL_RCTRL_MODE3_ADDRESS			0xd274
#define QSERDES_COM_PLL_CCTRL_MODE3_ADDRESS			0xd278
#define QSERDES_COM_HSCLK_SEL_3_ADDRESS				0xd27c
#define QSERDES_COM_CMN_CONFIG_3_ADDRESS			0xd280
#define QSERDES_COM_DEC_START_MODE3_ADDRESS			0xd284
#define QSERDES_COM_DEC_START_MSB_MODE3_ADDRESS			0xd288
#define QSERDES_COM_DIV_FRAC_START1_MODE3_ADDRESS		0xd28c
#define QSERDES_COM_DIV_FRAC_START2_MODE3_ADDRESS		0xd290
#define QSERDES_COM_DIV_FRAC_START3_MODE3_ADDRESS		0xd294
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE3_ADDRESS		0xd2a4
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE3_ADDRESS		0xd2a8
#define QSERDES_COM_VCO_TUNE1_MODE3_ADDRESS			0xd2ac
#define QSERDES_COM_VCO_TUNE2_MODE3_ADDRESS			0xd2b0
#define QSERDES_COM_LOCK_CMP1_MODE3_ADDRESS			0xd2b4
#define QSERDES_COM_LOCK_CMP2_MODE3_ADDRESS			0xd2b8
#define QSERDES_COM_CORECLK_DIV_MODE3_ADDRESS			0xd2bc
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE3_ADDRESS		0xd2c4
#define QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE3_ADDRESS		0xd2c8
#define QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_3_ADDRESS		0xd2d0
#define QSERDES_COM_ADDITIONAL_CTRL_3_ADDRESS			0xd2d4
#define QSERDES_COM_ADDITIONAL_MISC_6_ADDRESS			0xd2d8
#define QSERDES_COM_PLL_IVCO_MODE3_ADDRESS			0xd2e0
#define QSERDES_COM_HSCLK_SEL_QP_ADDRESS			0xd2e4
#define QSERDES_COM_CORECLK_DIV_MODE0_QP_ADDRESS		0xd2e8
#define QSERDES_COM_CORECLK_DIV_MODE2_QP_ADDRESS		0xd2ec
#define QSERDES_COM_VCO_TUNE_INITVAL1_HI_FREQ_ADDRESS		0xd2f0
#define QSERDES_COM_VCO_TUNE_INITVAL2_HI_FREQ_ADDRESS		0xd2f4
#define QSERDES_COM_LOCK_CMP1_MODE4_ADDRESS			0xd2f8
#define QSERDES_COM_LOCK_CMP2_MODE4_ADDRESS			0xd2fc

/* PMA init setting with dfe hardware tuning */
static sw_error_t adpt_jhppe_uniphy_pma_dfe_hw_tuning_init(a_uint32_t dev_id,
		a_uint32_t uniphy_index, a_uint32_t uniphy_mode, a_bool_t is_long)
{
	/* Common settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS,
		uniphy_index, 0x01);

	/* QSERDES PLL Settings, PLL RefClk = 46.875MHz for 25GAUI, PLL RefClk = 31.25MHz
	 * for other Serdes modes.
	 */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE0_ADDRESS,
		uniphy_index, 0x17); /* vco_mode0 for SGMII & QSGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE0_ADDRESS,
		uniphy_index, 0xA0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE0_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS,
		uniphy_index, 0x68);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL2_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0_ADDRESS,
		uniphy_index, 0xE0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0_ADDRESS,
		uniphy_index, 0x12);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_DC_LEVEL_CTRL_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_1_ADDRESS,
		uniphy_index, 0x04); /* Multi-mode shared */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORE_CLK_EN_ADDRESS,
		uniphy_index, 0x82);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_POST_DIV_MUX_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SVS_MODE_CLK_SEL_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_1_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_1_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE2_ADDRESS,
		uniphy_index, 0x17); /* vco_mode2 for SGMII+ & PSGMII & 12.5GPHY */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE2_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE2_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE2_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE2_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_2_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE2_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE2_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE2_ADDRESS,
		uniphy_index, 0xF7);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL1_HI_FREQ_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL2_HI_FREQ_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_2_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE2_ADDRESS,
		uniphy_index, 0xBB);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE2_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE2_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_3_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE0_QP_ADDRESS,
		uniphy_index, 0x0A); /* For PSGMII & QSGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_QP_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE2_QP_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE1_ADDRESS,
		uniphy_index, 0x17); /* vco_mode1 for USXGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE1_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE1_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE1_ADDRESS,
		uniphy_index, 0xA5);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE1_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE1_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE1_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE1_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE1_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE1_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE1_ADDRESS,
		uniphy_index, 0x77);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE1_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_2_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE4_ADDRESS,
		uniphy_index, 0xFF); /* For 12.5GPHY only */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE4_ADDRESS,
		uniphy_index, 0x27); /* For 12.5GPHY only */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE3_ADDRESS,
		uniphy_index, 0x17); /* vco_mode3 for 25GPHY */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE3_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE3_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE3_ADDRESS,
		uniphy_index, 0x37);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE3_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_3_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_CTRL_3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE3_ADDRESS,
		uniphy_index, 0x7F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE3_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE3_ADDRESS,
		uniphy_index, 0x95);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_3_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE3_ADDRESS,
		uniphy_index, 0xD0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE3_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_6_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP_EN_ADDRESS,
		uniphy_index, 0x42); /* All mode shared */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SYS_CLK_CTRL_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SSC_EN_CENTER_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CNTRL_ADDRESS,
		uniphy_index, 0x23);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SYSCLK_EN_SEL_ADDRESS,
		uniphy_index, 0xDA);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BG_TIMER_ADDRESS,
		uniphy_index, 0x0B);

	/* QSERDES TX and TX Ext Settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_SEL_20B_10B_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_TX_BAND0_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_TX_HR_SEL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_OFFSET_TX_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_OFFSET_RX_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_TX_ADDRESS,
		uniphy_index, 0x56);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_RX_ADDRESS,
		uniphy_index, 0x4E);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_2_ADDRESS,
		uniphy_index, 0xD0);
	if (uniphy_mode == PORT_WRAPPER_25GBASE_R)
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_3_ADDRESS,
			uniphy_index, 0x50);
	else
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_3_ADDRESS,
			uniphy_index, 0x40);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_ADAPTOR_MODE_CTRL1_ADDRESS,
		uniphy_index, 0x2A);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_ADAPTOR_MODE_CTRL3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B0_ADDRESS,
		uniphy_index, 0xC1);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B1_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B4_ADDRESS,
		uniphy_index, 0x35);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B6_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE4_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE5_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_LANE_MODE_BITS_RATE56_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE6_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_PREEMPH_RATE6_ADDRESS,
		uniphy_index, 0x10);

	/* QSERDES RX and RX Ext Settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CTRL1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_TERM_BW_CTRL0_ADDRESS,
		uniphy_index, 0x55);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B0_ADDRESS,
		uniphy_index, 0xC1);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B1_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B3_ADDRESS,
		uniphy_index, 0x1A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B4_ADDRESS,
		uniphy_index, 0x35);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B6_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_BAND_CTRL0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_Q_EN_RATES_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SVS_MODE_CTRL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RXCLK_DIV2_CTRL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3_ADDRESS,
		uniphy_index, 0x48);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_FO_GAIN_RATE2_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B0_ADDRESS,
		uniphy_index, 0xD3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B4_ADDRESS,
		uniphy_index, 0xB5);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B6_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_FO_GAIN_RATE3_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B0_ADDRESS,
		uniphy_index, 0xD3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B4_ADDRESS,
		uniphy_index, 0xB5);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B6_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SB2_GAIN1_RATE3_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SB2_GAIN2_RATE3_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE4_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CTRL2_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_TERM_BW_CTRL1_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B0_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B1_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B2_ADDRESS,
		uniphy_index, 0x80); /* 0x80(S)|0x80(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B3_ADDRESS,
		uniphy_index, 0x1A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B4_ADDRESS,
		uniphy_index, 0x39); /* 0x39(S)|0x39(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B6_ADDRESS,
		uniphy_index, 0xEF);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_CTRL2_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_SATURATION_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CONTROLS_ADDRESS,
		uniphy_index, 0x06);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS,
			uniphy_index, 0xA9);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS,
			uniphy_index, 0x39);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4_ADDRESS,
		uniphy_index, 0x09); /* For rate0 eq adaptor manual value */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1_ADDRESS,
		uniphy_index, 0x14);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL3_ADDRESS,
		uniphy_index, 0x0E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL0_FTUNE_CTRL_ADDRESS,
		uniphy_index, 0x30);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_GM_CAL_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE0_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE01_ADDRESS,
		uniphy_index, 0x44);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE1_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE2_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE23_ADDRESS,
		uniphy_index, 0x44);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE3_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE4_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE4_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_ENABLES_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_CNTRL_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_DEGLITCH_CNTRL_ADDRESS,
		uniphy_index, 0x18);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_LVL_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DFE_EN_TIMER_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL3_ADDRESS,
		uniphy_index, 0x7F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL5_ADDRESS,
		uniphy_index, 0x16); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE12_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE34_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL26_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE4_ADDRESS,
		uniphy_index, 0x28); /* Set the dfe hw tune run time for USXGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_SO_GAIN_RATE5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_TERM_BW_CTRL2_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B0_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B1_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B2_ADDRESS,
		uniphy_index, 0x80); /* 0x80(S)|0x80(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B3_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B4_ADDRESS,
		uniphy_index, 0x39); /* 0x39(S)|0x39(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B6_ADDRESS,
		uniphy_index, 0xEF);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B8_ADDRESS,
		uniphy_index, 0xA8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B9_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL27_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE5_ADDRESS,
		uniphy_index, 0x28); /* Set the dfe hw tune run time for 12.5G, value may change
				      * per post-Si VI or bench test.
				      */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE56_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DLL_FTUNE_CAL_TIME_RATE456_ADDRESS,
		uniphy_index, 0x30);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_FO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_SO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x06);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_COUNT_HIGH_RATE6_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_SO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x00);

	/* RX_MODE_RATE6 settings with is_long condition */
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS,
			uniphy_index, 0xBF);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS,
			uniphy_index, 0xFF);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B1_ADDRESS,
		uniphy_index, 0x3F);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS,
			uniphy_index, 0x40);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS,
			uniphy_index, 0x00);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B3_ADDRESS,
		uniphy_index, 0x98);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS,
			uniphy_index, 0x58);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS,
			uniphy_index, 0x12);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS,
			uniphy_index, 0x7F);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS,
			uniphy_index, 0x00);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B6_ADDRESS,
		uniphy_index, 0xF3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B8_ADDRESS,
		uniphy_index, 0xFB);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B9_ADDRESS,
		uniphy_index, 0x07);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE6_ADDRESS,
		uniphy_index, 0x28); /* Set the dfe hw tune run time for 25GAUI */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DFE_SUPPORTED_ADDRESS,
		uniphy_index, 0x70);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DCC_CLK_EXTRA_DIV2_EN_RATE_CTRL0_ADDRESS,
		uniphy_index, 0x40);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B0_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL5_ADDRESS,
		uniphy_index, 0x99);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL2_ADDRESS,
		uniphy_index, 0x59);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL4_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL10_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL5_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL11_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL6_ADDRESS,
		uniphy_index, 0xE9); /* 0xE9(S)|0xE9(L)	S: Short channel, L: long channel */
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS,
			uniphy_index, 0x75);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS,
			uniphy_index, 0x35);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL6_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL12_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B0_ADDRESS,
		uniphy_index, 0xAA);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL7_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL13_ADDRESS,
		uniphy_index, 0x38);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS,
			uniphy_index, 0xBE);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS,
			uniphy_index, 0x07);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS,
			uniphy_index, 0xDE);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS,
			uniphy_index, 0x03);
	}

	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE5_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_AUXDATA_BIN_RATE56_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL8_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL14_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE6_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL9_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL15_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_Q_PI_INTRINSIC_BIAS_RATE65_ADDRESS,
		uniphy_index, 0x39);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_ADDRESS,
		uniphy_index, 0x04); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD1_ADDRESS,
		uniphy_index, 0x64); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD2_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_HR_DCC_ICLK_CTRL1_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_CTRL1_ADDRESS,
		uniphy_index, 0x10); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EYEMON_CTRL0_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */

	/* DFE HW tune settings enabled for USXGMII, 12.5GPHY, 25GAUI Serdes modes, for other
	 * Serdes modes, DFE functionality is disabled in the sequence.
	 */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_TAP1ADP_CTRL1_ADDRESS,
		uniphy_index, 0x04); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_TAP1CODE_MAN_VAL_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */

	if (uniphy_mode == PORT_WRAPPER_25GBASE_R) { /* 25GAUI mode */
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_TAP1CODE_TARGET_VAL_ADDRESS,
			uniphy_index, 0x0A);
	} else {/* 12.5GPHY or USXGMII mode */
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_TAP1CODE_TARGET_VAL_ADDRESS,
			uniphy_index, 0x05);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_TAP1CODE_STEP_VAL_ADDRESS,
		uniphy_index, 0x01); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_TAP1CODE_STEP_TIME_ADDRESS,
		uniphy_index, 0x0A); /* Placeholder */

	return SW_OK;
}

/* PMA init setting with dfe software tuning */
static sw_error_t adpt_jhppe_uniphy_pma_dfe_sw_tuning_init(a_uint32_t dev_id,
		a_uint32_t uniphy_index, a_uint32_t uniphy_mode, a_bool_t is_long)
{
	/* Common settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS,
			uniphy_index, 0x01);

	/* QSERDES PLL Settings, PLL RefClk = 46.875MHz for 25GAUI, PLL RefClk = 31.25MHz
	 * for other Serdes modes.
	 */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE0_ADDRESS,
		uniphy_index, 0x17); /* vco_mode0 for SGMII & QSGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE0_ADDRESS,
		uniphy_index, 0xA0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE0_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS,
		uniphy_index, 0x68);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL2_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0_ADDRESS,
		uniphy_index, 0xE0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0_ADDRESS,
		uniphy_index, 0x12);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_DC_LEVEL_CTRL_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_1_ADDRESS,
		uniphy_index, 0x04); /* Multi-mode shared */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORE_CLK_EN_ADDRESS,
		uniphy_index, 0x82);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_POST_DIV_MUX_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SVS_MODE_CLK_SEL_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_1_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_1_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE2_ADDRESS,
		uniphy_index, 0x17); /* vco_mode2 for SGMII+ & PSGMII & 12.5GPHY */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE2_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE2_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE2_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE2_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_2_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE2_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE2_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE2_ADDRESS,
		uniphy_index, 0xF7);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL1_HI_FREQ_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL2_HI_FREQ_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_2_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE2_ADDRESS,
		uniphy_index, 0xBB);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE2_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE2_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_3_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE0_QP_ADDRESS,
		uniphy_index, 0x0A); /* For PSGMII & QSGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_QP_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE2_QP_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE1_ADDRESS,
		uniphy_index, 0x17); /* vco_mode1 for USXGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE1_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE1_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE1_ADDRESS,
		uniphy_index, 0xA5);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE1_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE1_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE1_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE1_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE1_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE1_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE1_ADDRESS,
		uniphy_index, 0x77);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE1_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_2_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE4_ADDRESS,
		uniphy_index, 0xFF); /* For 12.5GPHY only */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE4_ADDRESS,
		uniphy_index, 0x27); /* For 12.5GPHY only */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE3_ADDRESS,
		uniphy_index, 0x17); /* vco_mode3 for 25GPHY */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE3_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE3_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE3_ADDRESS,
		uniphy_index, 0x37);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE3_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_3_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_CTRL_3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE3_ADDRESS,
		uniphy_index, 0x7F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE3_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE3_ADDRESS,
		uniphy_index, 0x95);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_3_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE3_ADDRESS,
		uniphy_index, 0xD0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE3_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_6_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP_EN_ADDRESS,
		uniphy_index, 0x42); /* All mode shared */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SYS_CLK_CTRL_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SSC_EN_CENTER_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CNTRL_ADDRESS,
		uniphy_index, 0x23);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SYSCLK_EN_SEL_ADDRESS,
		uniphy_index, 0xDA);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BG_TIMER_ADDRESS,
		uniphy_index, 0x0B);

	/* QSERDES TX and TX Ext Settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_SEL_20B_10B_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_TX_BAND0_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_TX_HR_SEL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_OFFSET_TX_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_OFFSET_RX_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_TX_ADDRESS,
		uniphy_index, 0x56);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_RX_ADDRESS,
		uniphy_index, 0x4E);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_2_ADDRESS,
		uniphy_index, 0xD0);
	if (uniphy_mode == PORT_WRAPPER_25GBASE_R)
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_3_ADDRESS,
			uniphy_index, 0x50);
	else
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_3_ADDRESS,
			uniphy_index, 0x40);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_ADAPTOR_MODE_CTRL1_ADDRESS,
		uniphy_index, 0x2A);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_ADAPTOR_MODE_CTRL3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B0_ADDRESS,
		uniphy_index, 0xC1);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B1_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B4_ADDRESS,
		uniphy_index, 0x35);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B6_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE4_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE5_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_LANE_MODE_BITS_RATE56_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE6_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_PREEMPH_RATE6_ADDRESS,
		uniphy_index, 0x10);

	/* QSERDES RX and RX Ext Settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CTRL1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_TERM_BW_CTRL0_ADDRESS,
		uniphy_index, 0x55);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B0_ADDRESS,
		uniphy_index, 0xC1);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B1_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B3_ADDRESS,
		uniphy_index, 0x1A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B4_ADDRESS,
		uniphy_index, 0x35);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B6_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_BAND_CTRL0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_Q_EN_RATES_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SVS_MODE_CTRL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RXCLK_DIV2_CTRL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3_ADDRESS,
		uniphy_index, 0x48);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_FO_GAIN_RATE2_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B0_ADDRESS,
		uniphy_index, 0xD3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B4_ADDRESS,
		uniphy_index, 0xB5);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B6_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_FO_GAIN_RATE3_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B0_ADDRESS,
		uniphy_index, 0xD3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B4_ADDRESS,
		uniphy_index, 0xB5);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B6_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SB2_GAIN1_RATE3_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SB2_GAIN2_RATE3_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE4_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CTRL2_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_TERM_BW_CTRL1_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B0_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B1_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B2_ADDRESS,
		uniphy_index, 0x80); /* 0x80(S)|0x80(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B3_ADDRESS,
		uniphy_index, 0x1A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B4_ADDRESS,
		uniphy_index, 0x39); /* 0x39(S)|0x39(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B6_ADDRESS,
		uniphy_index, 0xEF);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_CTRL2_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_SATURATION_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CONTROLS_ADDRESS,
		uniphy_index, 0x06);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS,
			uniphy_index, 0xA9);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS,
			uniphy_index, 0x39);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4_ADDRESS,
		uniphy_index, 0x09); /* For rate0 eq adaptor manual value */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1_ADDRESS,
		uniphy_index, 0x14);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL3_ADDRESS,
		uniphy_index, 0x0E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL0_FTUNE_CTRL_ADDRESS,
		uniphy_index, 0x30);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_GM_CAL_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE0_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE01_ADDRESS,
		uniphy_index, 0x44);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE1_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE2_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE23_ADDRESS,
		uniphy_index, 0x44);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE3_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE4_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE4_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_ENABLES_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_CNTRL_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_DEGLITCH_CNTRL_ADDRESS,
		uniphy_index, 0x18);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_LVL_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DFE_EN_TIMER_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL3_ADDRESS,
		uniphy_index, 0x7F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL5_ADDRESS,
		uniphy_index, 0x16); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE12_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE34_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */

	/* DIFFERENT: 0x40 for software tuning vs 0x80 for hardware tuning */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL26_ADDRESS,
		uniphy_index, 0x40);

	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_SO_GAIN_RATE5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_TERM_BW_CTRL2_ADDRESS,
		uniphy_index, 0x01);

	/* RX_MODE_RATE5 settings - all same regardless of is_long */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B0_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B1_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L) S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B2_ADDRESS,
		uniphy_index, 0x80); /* 0x80(S)|0x80(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B3_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B4_ADDRESS,
		uniphy_index, 0x39); /* 0x39(S)|0x39(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B6_ADDRESS,
		uniphy_index, 0xEF);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B8_ADDRESS,
		uniphy_index, 0xA8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B9_ADDRESS,
		uniphy_index, 0x02);

	/* DIFFERENT: 0x05 for software tuning vs 0x0A for hardware tuning */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL27_ADDRESS,
		uniphy_index, 0x05);

	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE56_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DLL_FTUNE_CAL_TIME_RATE456_ADDRESS,
		uniphy_index, 0x30);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_FO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_SO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x06);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_COUNT_HIGH_RATE6_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_SO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x00);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS,
			uniphy_index, 0xBF);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS,
			uniphy_index, 0xFF);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B1_ADDRESS,
		uniphy_index, 0x3F);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS,
			uniphy_index, 0x40);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS,
			uniphy_index, 0x00);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B3_ADDRESS,
		uniphy_index, 0x98);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS,
			uniphy_index, 0x58);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS,
			uniphy_index, 0x12);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS,
			uniphy_index, 0x7F);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS,
			uniphy_index, 0x00);
	}

	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B6_ADDRESS,
		uniphy_index, 0xF3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B8_ADDRESS,
		uniphy_index, 0xFB);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B9_ADDRESS,
		uniphy_index, 0x07);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DFE_SUPPORTED_ADDRESS,
		uniphy_index, 0x70);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DCC_CLK_EXTRA_DIV2_EN_RATE_CTRL0_ADDRESS,
		uniphy_index, 0x40);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B0_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL5_ADDRESS,
		uniphy_index, 0x99);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL2_ADDRESS,
		uniphy_index, 0x59);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL4_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL10_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL5_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL11_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL6_ADDRESS,
		uniphy_index, 0xE9); /* 0xE9(S)|0xE9(L)	S: Short channel, L: long channel */
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS,
			uniphy_index, 0x75);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS,
			uniphy_index, 0x35);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL6_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL12_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B0_ADDRESS,
		uniphy_index, 0xAA);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL7_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL13_ADDRESS,
		uniphy_index, 0x38);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS,
			uniphy_index, 0xBE);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS,
			uniphy_index, 0x07);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS,
			uniphy_index, 0xDE);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS,
			uniphy_index, 0x03);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE5_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_AUXDATA_BIN_RATE56_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL8_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL14_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE6_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL9_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL15_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_Q_PI_INTRINSIC_BIAS_RATE65_ADDRESS,
		uniphy_index, 0x39);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL9_ADDRESS,
		uniphy_index, 0x40); /* Mask RXEQ_ENGINE_DONE flag for software tuning */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_ADDRESS,
		uniphy_index, 0x04); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD1_ADDRESS,
		uniphy_index, 0x64); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD2_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_HR_DCC_ICLK_CTRL1_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_CTRL1_ADDRESS,
		uniphy_index, 0x10); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EYEMON_CTRL0_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */

	return SW_OK;
}

/* PMA init setting with dfe disabled */
static sw_error_t adpt_jhppe_uniphy_pma_dfe_disabled_init(a_uint32_t dev_id,
		a_uint32_t uniphy_index, a_uint32_t uniphy_mode, a_bool_t is_long)
{
	/* Common settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS,
		uniphy_index, 0x01);

	/* QSERDES PLL Settings, PLL RefClk = 46.875MHz for 25GAUI, PLL RefClk = 31.25MHz
	 * for other Serdes modes.
	 */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE0_ADDRESS,
		uniphy_index, 0x17); /* vco_mode0 for SGMII & QSGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE0_ADDRESS,
		uniphy_index, 0xA0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE0_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS,
		uniphy_index, 0x68);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL2_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0_ADDRESS,
		uniphy_index, 0xE0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0_ADDRESS,
		uniphy_index, 0x12);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_DC_LEVEL_CTRL_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_1_ADDRESS,
		uniphy_index, 0x04); /* Multi-mode shared */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORE_CLK_EN_ADDRESS,
		uniphy_index, 0x82);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_POST_DIV_MUX_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SVS_MODE_CLK_SEL_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_1_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_1_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE2_ADDRESS,
		uniphy_index, 0x17); /* vco_mode2 for SGMII+ & PSGMII & 12.5GPHY */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE2_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE2_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE2_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE2_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_2_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE2_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE2_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE2_ADDRESS,
		uniphy_index, 0xF7);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL1_HI_FREQ_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE_INITVAL2_HI_FREQ_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_2_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE2_ADDRESS,
		uniphy_index, 0xBB);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE2_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE2_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_3_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE0_QP_ADDRESS,
		uniphy_index, 0x0A); /* For PSGMII & QSGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_QP_ADDRESS,
		uniphy_index, 0x11);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE2_QP_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE1_ADDRESS,
		uniphy_index, 0x17); /* vco_mode1 for USXGMII */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE1_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE1_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE1_ADDRESS,
		uniphy_index, 0xA5);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE1_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE1_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE1_ADDRESS,
		uniphy_index, 0xFF);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE1_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE1_ADDRESS,
		uniphy_index, 0x19);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE1_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE1_ADDRESS,
		uniphy_index, 0x77);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE1_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_2_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE4_ADDRESS,
		uniphy_index, 0xFF); /* For 12.5GPHY only */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE4_ADDRESS,
		uniphy_index, 0x27); /* For 12.5GPHY only */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CP_CTRL_MODE3_ADDRESS,
		uniphy_index, 0x17); /* vco_mode3 for 25GPHY */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_RCTRL_MODE3_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CCTRL_MODE3_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MODE3_ADDRESS,
		uniphy_index, 0x37);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DEC_START_MSB_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START1_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START2_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_DIV_FRAC_START3_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN0_MODE3_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_INTEGLOOP_GAIN1_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CORECLK_DIV_MODE3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_HSCLK_SEL_3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_CONFIG_3_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_CTRL_3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP1_MODE3_ADDRESS,
		uniphy_index, 0x7F);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP2_MODE3_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE1_MODE3_ADDRESS,
		uniphy_index, 0x95);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_VCO_TUNE2_MODE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_3_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE3_ADDRESS,
		uniphy_index, 0xD0);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_IVCO_MODE3_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_ADDITIONAL_MISC_6_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_LOCK_CMP_EN_ADDRESS,
		uniphy_index, 0x42); /* All mode shared */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SYS_CLK_CTRL_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SSC_EN_CENTER_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_PLL_CNTRL_ADDRESS,
		uniphy_index, 0x23);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_SYSCLK_EN_SEL_ADDRESS,
		uniphy_index, 0xDA);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_BG_TIMER_ADDRESS,
		uniphy_index, 0x0B);

	/* QSERDES TX and TX Ext Settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_SEL_20B_10B_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_TX_BAND0_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_TX_HR_SEL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_OFFSET_TX_ADDRESS,
		uniphy_index, 0x03);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_OFFSET_RX_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_TX_ADDRESS,
		uniphy_index, 0x56);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_RES_CODE_LANE_RX_ADDRESS,
		uniphy_index, 0x4E);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_2_ADDRESS,
		uniphy_index, 0xD0);
	if (uniphy_mode == PORT_WRAPPER_25GBASE_R)
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_3_ADDRESS,
			uniphy_index, 0x50);
	else
		hppe_uniphy_reg_set(dev_id, QSERDES_TX_LANE_MODE_3_ADDRESS,
			uniphy_index, 0x40);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_ADAPTOR_MODE_CTRL1_ADDRESS,
		uniphy_index, 0x2A);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_ADAPTOR_MODE_CTRL3_ADDRESS,
		uniphy_index, 0x0A);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B0_ADDRESS,
		uniphy_index, 0xC1);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B1_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B4_ADDRESS,
		uniphy_index, 0x35);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B6_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_RX_MODE_RATE1_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE4_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE5_ADDRESS,
		uniphy_index, 0x0F);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_LANE_MODE_BITS_RATE56_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_EMP_POST1_LVL_RATE6_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_PREEMPH_RATE6_ADDRESS,
		uniphy_index, 0x10);

	/* QSERDES RX and RX Ext Settings */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CTRL1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_TERM_BW_CTRL0_ADDRESS,
		uniphy_index, 0x55);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B0_ADDRESS,
		uniphy_index, 0xC1);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B1_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B3_ADDRESS,
		uniphy_index, 0x1A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B4_ADDRESS,
		uniphy_index, 0x35);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B6_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE_0_1_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_BAND_CTRL0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_Q_EN_RATES_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SVS_MODE_CTRL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RXCLK_DIV2_CTRL_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3_ADDRESS,
		uniphy_index, 0x48);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE2_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_FO_GAIN_RATE2_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B0_ADDRESS,
		uniphy_index, 0xD3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B4_ADDRESS,
		uniphy_index, 0xB5);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B6_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE2_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE3_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_FO_GAIN_RATE3_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B0_ADDRESS,
		uniphy_index, 0xD3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B1_ADDRESS,
		uniphy_index, 0x13);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B2_ADDRESS,
		uniphy_index, 0xC8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B3_ADDRESS,
		uniphy_index, 0x1E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B4_ADDRESS,
		uniphy_index, 0xB5);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B5_ADDRESS,
		uniphy_index, 0x80);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B6_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE3_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SB2_GAIN1_RATE3_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SB2_GAIN2_RATE3_ADDRESS,
		uniphy_index, 0x05);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_GAIN_RATE4_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CTRL2_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_TERM_BW_CTRL1_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B0_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B1_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B2_ADDRESS,
		uniphy_index, 0x80); /* 0x80(S)|0x80(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B3_ADDRESS,
		uniphy_index, 0x1A);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B4_ADDRESS,
		uniphy_index, 0x39); /* 0x39(S)|0x39(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B6_ADDRESS,
		uniphy_index, 0xEF);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_MODE_RATE4_B8_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_CTRL2_ADDRESS,
		uniphy_index, 0x20);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_SO_SATURATION_ADDRESS,
		uniphy_index, 0x1F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_UCDR_PI_CONTROLS_ADDRESS,
		uniphy_index, 0x06);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS,
			uniphy_index, 0xA9);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS,
			uniphy_index, 0x39);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4_ADDRESS,
		uniphy_index, 0x09); /* For rate0 eq adaptor manual value */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1_ADDRESS,
		uniphy_index, 0x14);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL3_ADDRESS,
		uniphy_index, 0x0E);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL0_FTUNE_CTRL_ADDRESS,
		uniphy_index, 0x30);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_GM_CAL_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE0_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE01_ADDRESS,
		uniphy_index, 0x44);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE1_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE2_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE23_ADDRESS,
		uniphy_index, 0x44);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE3_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE4_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_AUXDATA_BIN_RATE4_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_ENABLES_ADDRESS,
		uniphy_index, 0x10);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_CNTRL_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_DEGLITCH_CNTRL_ADDRESS,
		uniphy_index, 0x18);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_SIGDET_LVL_ADDRESS,
		uniphy_index, 0x04);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DFE_EN_TIMER_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL3_ADDRESS,
		uniphy_index, 0x7F);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL5_ADDRESS,
		uniphy_index, 0x16); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE12_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE34_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL26_ADDRESS,
		uniphy_index, 0x40); /* Same as software tuning: 0x40 */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_SO_GAIN_RATE5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_TERM_BW_CTRL2_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B0_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B1_ADDRESS,
		uniphy_index, 0x15); /* 0x15(S)|0x15(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B2_ADDRESS,
		uniphy_index, 0x80); /* 0x80(S)|0x80(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B3_ADDRESS,
		uniphy_index, 0x1B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B4_ADDRESS,
		uniphy_index, 0x39); /* 0x39(S)|0x39(L)	S: Short channel, L: long channel */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B5_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B6_ADDRESS,
		uniphy_index, 0xEF);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B8_ADDRESS,
		uniphy_index, 0xA8);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE5_B9_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL27_ADDRESS,
		uniphy_index, 0x05); /* Same as software tuning: 0x05 */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE56_ADDRESS,
		uniphy_index, 0x11); /* Enable vga_dfe_gap_time */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DLL_FTUNE_CAL_TIME_RATE456_ADDRESS,
		uniphy_index, 0x30);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_FO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x1C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_SO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x06);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FASTLOCK_COUNT_HIGH_RATE6_ADDRESS,
		uniphy_index, 0x02);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_FO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x0C);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_UCDR_SO_GAIN_RATE6_ADDRESS,
		uniphy_index, 0x00);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS,
			uniphy_index, 0xBF);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS,
			uniphy_index, 0xFF);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B1_ADDRESS,
		uniphy_index, 0x3F);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS,
			uniphy_index, 0x40);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS,
			uniphy_index, 0x00);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B3_ADDRESS,
		uniphy_index, 0x98);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS,
			uniphy_index, 0x58);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS,
			uniphy_index, 0x12);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS,
			uniphy_index, 0x7F);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS,
			uniphy_index, 0x00);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B6_ADDRESS,
		uniphy_index, 0xF3);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B7_ADDRESS,
		uniphy_index, 0x60);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B8_ADDRESS,
		uniphy_index, 0xFB);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_RATE6_B9_ADDRESS,
		uniphy_index, 0x07);

	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_DCC_CLK_EXTRA_DIV2_EN_RATE_CTRL0_ADDRESS,
		uniphy_index, 0x40);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B0_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL5_ADDRESS,
		uniphy_index, 0x99);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL2_ADDRESS,
		uniphy_index, 0x59);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL4_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL10_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL5_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL11_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B0_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL6_ADDRESS,
		uniphy_index, 0xE9); /* 0xE9(S)|0xE9(L)	S: Short channel, L: long channel */
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS,
			uniphy_index, 0x75);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS,
			uniphy_index, 0x35);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL6_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL12_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B0_ADDRESS,
		uniphy_index, 0xAA);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B1_ADDRESS,
		uniphy_index, 0x00);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL7_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL13_ADDRESS,
		uniphy_index, 0x38);
	if (is_long) {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS,
			uniphy_index, 0xBE);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS,
			uniphy_index, 0x07);
	} else {
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS,
			uniphy_index, 0xDE);
		hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS,
			uniphy_index, 0x03);
	}
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE5_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_AUXDATA_BIN_RATE56_ADDRESS,
		uniphy_index, 0x24);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL8_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL14_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE6_ADDRESS,
		uniphy_index, 0x0B);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL9_ADDRESS,
		uniphy_index, 0x01);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL15_ADDRESS,
		uniphy_index, 0x38);
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_Q_PI_INTRINSIC_BIAS_RATE65_ADDRESS,
		uniphy_index, 0x39);
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_ADDRESS,
		uniphy_index, 0x04); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD1_ADDRESS,
		uniphy_index, 0x64); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_COM_CMN_MODE_CONTD2_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_HR_DCC_ICLK_CTRL1_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_DLL_CTRL1_ADDRESS,
		uniphy_index, 0x10); /* Placeholder */
	hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RX_EYEMON_CTRL0_ADDRESS,
		uniphy_index, 0x00); /* Placeholder */

	return SW_OK;
}

/* PMA initialization settings, dfe_mode: 0 for DFE hardware tuning, 1 for DFE software tuning,
 * 2 for DFE disabled. is_long: 0 for short channel, 1 for long channel.
 */
sw_error_t
adpt_jhppe_uniphy_pma_init_setting(a_uint32_t dev_id, a_uint32_t uniphy_index,
	a_uint32_t uniphy_mode, a_uint32_t dfe_mode, a_bool_t is_long)
{
	sw_error_t rv = SW_OK;

	ssdk_uniphy_reset(dev_id, UNIPHY0_SYS_RESET_E + uniphy_index, SSDK_RESET_ASSERT);
	switch (dfe_mode) {
	case 0:
		rv = adpt_jhppe_uniphy_pma_dfe_hw_tuning_init(dev_id, uniphy_index,
				uniphy_mode, is_long);
		break;
	case 1:
		rv = adpt_jhppe_uniphy_pma_dfe_sw_tuning_init(dev_id, uniphy_index,
				uniphy_mode, is_long);
		break;
	case 2:
		rv = adpt_jhppe_uniphy_pma_dfe_disabled_init(dev_id, uniphy_index,
				uniphy_mode, is_long);
		break;
	default:
		rv = SW_NOT_SUPPORTED;
		break;
	}
	ssdk_uniphy_reset(dev_id, UNIPHY0_SYS_RESET_E + uniphy_index, SSDK_RESET_DEASSERT);
	/* wait for uniphy sys reset to settle after deassert */
	msleep(1);
	return rv;
}

/* PMA DFE software tune sequence if PMA init settings choose DFE software tuning. */
sw_error_t
adpt_jhppe_uniphy_pma_dfe_sw_tune(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	a_uint32_t retries = ADPT_JHPPE_UNIPHY_POLLING_TIMEOUT;
	a_uint32_t tap1_code, vga_done = 0;
	sw_error_t ret;

	/* Poll QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS until bit[4] VGA_DONE is 0x1 */
	do {
		ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_vga_done_get(dev_id,
			uniphy_index, &vga_done);
		SW_RTN_ON_ERROR(ret);
		if (vga_done)
			break;
		mdelay(ADPT_JHPPE_UNIPHY_POLLING_DELAY);
		if (retries-- == 0) {
			SSDK_ERROR("JHPPE uniphy %d: VGA_DONE polling timeout!\n", uniphy_index);
			return SW_TIMEOUT;
		}
	} while (1);

	/* Write DFE TAP1 code sequence (0x80~0x8A) with 10us delays between each write */
	for (tap1_code = 0x80; tap1_code <= 0x8A; tap1_code++) {
		ret = hppe_uniphy_reg_set(dev_id, QSERDES_TX_EXT_DFE_TAP1_CODE_ADDRESS,
				uniphy_index, tap1_code);
		SW_RTN_ON_ERROR(ret);
		udelay(10);
	}

	/* Unmask RXEQ_ENGINE_DONE flag */
	return hppe_uniphy_reg_set(dev_id, QSERDES_RX_EXT_RXEQ_CTRL9_ADDRESS, uniphy_index, 0x00);
}

sw_error_t
adpt_jhppe_uniphy_xlgpcs_soft_reset(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t ret = SW_OK;
	a_uint32_t vr_rst;
	a_uint32_t timeout = ADPT_JHPPE_UNIPHY_POLLING_TIMEOUT;

	/* Program the VR_RST field (bit [15]) of the VR_PCS_DIG_CTRL1 register to 1 */
	ret = jhppe_uniphy_vr_pcs_dig_ctrl1_vr_rst_set(dev_id, uniphy_index, 1);
	SW_RTN_ON_ERROR(ret);

	/* Polling until VR_RST field (bit [15]) to be 0 */
	while (timeout > 0) {
		ret = jhppe_uniphy_vr_pcs_dig_ctrl1_vr_rst_get(dev_id, uniphy_index, &vr_rst);
		SW_RTN_ON_ERROR(ret);

		if (vr_rst == 0) {
			/* Reset completed */
			break;
		}

		mdelay(ADPT_JHPPE_UNIPHY_POLLING_DELAY);
		timeout--;
	}

	if (timeout == 0) {
		SSDK_ERROR("JHPPE UNIPHY %d: VR_RST polling timeout\n", uniphy_index);
		return SW_TIMEOUT;
	}

	return SW_OK;
}

sw_error_t
adpt_jhppe_uniphy_fec_set(a_uint32_t dev_id, a_uint32_t uniphy_index, a_uint32_t configured_fec)

{
	a_uint32_t am_cnt, cwn00, cwn01, cwn02, cwn03;
	a_bool_t rs_fec_en, baser_fec_en;
	sw_error_t ret = SW_OK;

	if (uniphy_index >= SSDK_MAX_UNIPHY_INSTANCE)
		return SW_BAD_PARAM;

	switch (configured_fec) {
	case ETHTOOL_FEC_RS:
		am_cnt = 0x400;
		cwn00 = 0x68C1;
		cwn01 = 0x3321;
		cwn02 = 0x973E;
		cwn03 = 0xCCDE;
		rs_fec_en = A_TRUE;
		baser_fec_en = A_FALSE;
		break;
	case ETHTOOL_FEC_BASER:
		am_cnt = 0x3FF;
		cwn00 = 0x7690;
		cwn01 = 0x3347;
		cwn02 = 0x896F;
		cwn03 = 0xCCB8;
		rs_fec_en = A_FALSE;
		baser_fec_en = A_TRUE;
		break;
	case ETHTOOL_FEC_OFF:
		am_cnt = 0x3FF;
		cwn00 = 0x7690;
		cwn01 = 0x3347;
		cwn02 = 0x896F;
		cwn03 = 0xCCB8;
		rs_fec_en = A_FALSE;
		baser_fec_en = A_FALSE;
		break;
	default:
		SSDK_ERROR("uniphy %d does not support fec mode 0x%x\n",
			   uniphy_index, configured_fec);
		return SW_NOT_SUPPORTED;
	}

	/* Set FEC configuration */
	ret = jhppe_uniphy_vr_pcs_am_cnt_pcs_am_cnt_set(dev_id, uniphy_index, am_cnt);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_uniphy_vr_pma_cwm00_cwm15_0_set(dev_id, uniphy_index, cwn00);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_uniphy_vr_pma_cwm01_cwm31_16_set(dev_id, uniphy_index, cwn01);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_uniphy_vr_pma_cwm02_cwm47_32_set(dev_id, uniphy_index, cwn02);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_uniphy_vr_pma_cwm03_cwm63_48_set(dev_id, uniphy_index, cwn03);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_uniphy_sr_pma_rs_fec_ctrl_rsfec_en_set(dev_id, uniphy_index, rs_fec_en);
	SW_RTN_ON_ERROR(ret);

	ret = jhppe_uniphy_sr_pma_kr_fec_ctrl_fec_en_set(dev_id, uniphy_index, baser_fec_en);
	SW_RTN_ON_ERROR(ret);

	/* Perform XLGPCS soft reset after FEC mode configuration */
	return adpt_jhppe_uniphy_xlgpcs_soft_reset(dev_id, uniphy_index);
}

static sw_error_t
adpt_jhppe_uniphy_25g_pcs_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	struct ssdk_port_priv *port_priv = NULL;
	struct qca_phy_priv *priv = NULL;
	sw_error_t ret = SW_OK;
	a_uint32_t port;

	/* Step 1: Write 4'b0101 to bits[5:2] (SS_5_2 field) of the SR_PCS_CTRL1 register */
	ret = jhppe_uniphy_sr_pcs_ctrl1_ss_5_2_set(dev_id, uniphy_index, 0x5);
	SW_RTN_ON_ERROR(ret);

	/* Step 2: Write 4'b0111 to bits[3:0] (PCS_TYPE_SEL field) of the SR_PCS_CTRL2 register
	 * to select 25GBASE-R PCS.
	 */
	ret = jhppe_uniphy_sr_pcs_ctrl2_pcs_type_sel_set(dev_id, uniphy_index, 0x7);
	SW_RTN_ON_ERROR(ret);

	/* Step 3: Program bits[6:0] (PMA_TYPE field) of the SR_PMA_CTRL2 register
	 * to 7'b0111001 (25GBASE-KR).
	 */
	ret = jhppe_uniphy_sr_pma_ctrl2_pma_type_set(dev_id, uniphy_index, 0x39);
	SW_RTN_ON_ERROR(ret);

	/* Step 4: Program bit[1] (EN_50G field) in the VR_PCS_DIG_CTRL3 register
	 * to 0 (to disable 50G).
	 */
	ret = jhppe_uniphy_vr_pcs_dig_ctrl3_en_50g_set(dev_id, uniphy_index, 0);
	SW_RTN_ON_ERROR(ret);

	/* Step 5: Program bit[0] (CNS_EN field) in the VR_PCS_DIG_CTRL3 register to 0 or 1 */
	ret = jhppe_uniphy_vr_pcs_dig_ctrl3_cns_en_set(dev_id, uniphy_index, 0);
	SW_RTN_ON_ERROR(ret);


	/* Step 6: Configure FEC mode and PCS soft reset */
	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	port = adpt_hppe_port_get_by_uniphy(dev_id, uniphy_index, SSDK_UNIPHY_CHANNEL0);
	if (port >= SW_MAX_NR_PORT)
		return SW_BAD_PARAM;

	port_priv = &priv->ports[port];

	if (port_priv->configured_fec) {
		ret = adpt_jhppe_uniphy_fec_set(dev_id, uniphy_index, port_priv->configured_fec);
		SW_RTN_ON_ERROR(ret);
	} else {
		/* For initialization, use RS FEC mode */
		port_priv->configured_fec = ETHTOOL_FEC_RS;
		ret = adpt_jhppe_uniphy_fec_set(dev_id, uniphy_index, ETHTOOL_FEC_RS);
		SW_RTN_ON_ERROR(ret);
	}

	SSDK_INFO("JHPPE UNIPHY %d: 25G PCS configuration completed successfully\n", uniphy_index);
	return SW_OK;
}

static int adpt_jhppe_uniphy_rxeq_engine_done_read(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t ret;
	a_uint32_t rxeq_engine_done = 0;

	ret = jhppe_uniphy_qserdes_rx_ext_ro_pmad_rxeq_status_rxeq_engine_done_get(dev_id,
			uniphy_index, &rxeq_engine_done);

	if (ret != SW_OK)
		return -EIO;  /* Return negative error code */

	return rxeq_engine_done;
}

sw_error_t
adpt_jhppe_uniphy_rxeq_status_check(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	int ret;
	int rxeq_engine_done;

	/* Polling QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS[1] (RXEQ_ENGINE_DONE) until 1
	 * Using read_poll_timeout with:
	 * - op: adpt_jhppe_uniphy_rxeq_engine_done_read
	 * - val: rxeq_engine_done
	 * - cond: rxeq_engine_done == 1 (RXEQ engine completed)
	 * - sleep_us: 1000 (1ms between polls)
	 * - timeout_us: 2000000 (2 second timeout)
	 * - sleep_before_read: false (don't sleep before first read)
	 * - args: dev_id, uniphy_index
	 */
	ret = read_poll_timeout(adpt_jhppe_uniphy_rxeq_engine_done_read, rxeq_engine_done,
			rxeq_engine_done == 1 || rxeq_engine_done < 0,
			1000, 2000000, false, dev_id, uniphy_index);

	if (ret == -ETIMEDOUT) {
		SSDK_ERROR("JHPPE UNIPHY %d: RXEQ engine done polling timeout\n", uniphy_index);
		return SW_TIMEOUT;
	} else if (rxeq_engine_done < 0) {
		SSDK_ERROR("JHPPE UNIPHY %d: RXEQ engine status read error\n", uniphy_index);
		return SW_FAIL;
	} else if (rxeq_engine_done == 1) {
		SSDK_INFO("JHPPE UNIPHY %d: RXEQ engine completed successfully\n", uniphy_index);
		return SW_OK;
	}

	return SW_FAIL;
}

static void
adpt_jhppe_gcc_uniphy_xlgpcs_reset(a_uint32_t dev_id, a_uint32_t uniphy_index, a_bool_t enable)
{
	enum ssdk_rst_action rst_action;
	enum uniphy_rst_type rst_type;

	switch (uniphy_index) {
	case SSDK_UNIPHY_INSTANCE1:
		rst_type = UNIPHY1_XLGPCS_RESET_E;
		break;
	case SSDK_UNIPHY_INSTANCE2:
		rst_type = UNIPHY2_XLGPCS_RESET_E;
		break;
	case SSDK_UNIPHY_INSTANCE0:
	default:
		SSDK_ERROR("UNIPHY %d not support XLGPCS\n", uniphy_index);
		return;
	}

	rst_action = (enable == A_TRUE) ? SSDK_RESET_ASSERT : SSDK_RESET_DEASSERT;

	ssdk_uniphy_reset(dev_id, rst_type, rst_action);

	return;
}

sw_error_t
adpt_jhppe_uniphy_calibrate(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	a_uint32_t reg_value = 0;
	a_uint32_t retries = ADPT_JHPPE_UNIPHY_POLLING_TIMEOUT;

	ADPT_DEV_ID_CHECK(dev_id);

	/* JHPPE calibration sequence:
	 * 1. Write 1'b1 to register CSR0 UNIPHY_OPTION_3[4](OFFSET 0x5AC/0X588): uniphy start
	 * 2. Wait for traffic state—read CSR3 R0_POWER_STATE[15:0](OFFSET 0XCDF4) = 0xf
	 */

	/* Step 1: Set UNIPHY_START bit to 1 */
	jhppe_uniphy_pcs_uniphy_option_3_uniphy_start_set(dev_id, uniphy_index, 1);

	/* Step 2: Wait for traffic state to be 0xf */
	while (reg_value != 0xf) {
		mdelay(ADPT_JHPPE_UNIPHY_POLLING_DELAY);
		if (retries-- == 0) {
			SSDK_ERROR("JHPPE uniphy %d calibration time out!\n", uniphy_index);
			return SW_TIMEOUT;
		}
		jhppe_uniphy_qserdes_rx_ext_ro_power_state_power_state_get(dev_id,
				uniphy_index, &reg_value);
	}

	return SW_OK;
}
sw_error_t
adpt_jhppe_uniphy_25g_r_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t rv;

	/* PMA init setting */
	adpt_jhppe_uniphy_pma_init_setting(dev_id, uniphy_index,
			PORT_WRAPPER_25GBASE_R, 2, A_FALSE);

	/* disable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index, 1, A_FALSE);

	/* keep xpcs to reset status */
	adpt_jhppe_gcc_uniphy_xlgpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* configure uniphy to 25G-R/XLGPCS mode */
	__adpt_hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index,
					 PORT_WRAPPER_25GBASE_R, SSDK_UNIPHY_CHANNEL0);
	/* configure uniphy gcc software reset */
	__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	/* Write 1'b1 to register CSR3 RXEQ_CTRL0[7] (0XCB20) */
	jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mask_set(dev_id, uniphy_index, 1);

	/* wait calibration done to uniphy */
	adpt_jhppe_uniphy_calibrate(dev_id, uniphy_index);

	/* enable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index, 1, A_TRUE);

	/* release xpcs reset status */
	adpt_jhppe_gcc_uniphy_xlgpcs_reset(dev_id, uniphy_index, A_FALSE);

	/* 25g xlgpcs setting */
	rv = adpt_jhppe_uniphy_25g_pcs_set(dev_id, uniphy_index);
	if (rv != SW_OK)
		SSDK_ERROR("uniphy %d 25g pcs set failed.\n", uniphy_index);

	/* Write 1'b0 to register CSR3 RXEQ_CTRL0[7] (0XCB20) */
	jhppe_uniphy_qserdes_rx_ext_rxeq_ctrl0_xpcs_rxeq_en_mask_set(dev_id, uniphy_index, 0);
#if 0
	/* software tuning if dfe mode choose software mode */
	adpt_jhppe_uniphy_pma_dfe_sw_tune(dev_id, uniphy_index);
#endif
	return SW_OK;
}

sw_error_t
adpt_jhppe_uniphy_25gr_status_check(a_uint32_t dev_id, a_uint32_t uniphy_index, a_bool_t *status)
{
	sw_error_t ret = SW_OK;
	a_uint32_t rlu = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(status);

	if (uniphy_index >= SSDK_MAX_UNIPHY_INSTANCE)
		return SW_BAD_PARAM;

	/* Read SR_PCS_STS1 BIT2 (RLU - Receive Link Up) */
	ret = jhppe_uniphy_sr_pcs_sts1_rlu_get(dev_id, uniphy_index, &rlu);
	if (ret != SW_OK) {
		SSDK_ERROR("JHPPE UNIPHY %d: Failed to read SR_PCS_STS1 RLU bit\n", uniphy_index);
		return ret;
	}

	/* Set status based on RLU bit value */
	*status = (rlu == 1) ? A_TRUE : A_FALSE;
	return SW_OK;
}
