// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include "nss-switch.h"

/* DFE mode definitions */
#define UNIPHY_PMA_DFE_HW_TUNING	0	/* DFE hardware tuning */
#define UNIPHY_PMA_DFE_SW_TUNING	1	/* DFE software tuning */
#define UNIPHY_PMA_DFE_DISABLED		2	/* DFE disabled */

/*
 * Hardware Configuration
 * - 24 TX Descriptor Rings (0-23)
 * - 20 TX Completion Rings (0-19)
 * - 20 RX Fill Rings (0-19)
 * - 24 RX Descriptor Rings (0-23)
 * - Mixed stride: 4KB for TX, 256B for RX (memory efficient)
 * - DDRQ, GRO, TSO control, pass-through features
 * - 40-port flow control
 * - 11 interrupt types
 */

/* Hardware configuration constants */
#define TX_DESC_RINGS_COUNT		24
#define TX_CMPL_RINGS_COUNT		20
#define RX_FILL_RINGS_COUNT		20
#define RX_DESC_RINGS_COUNT		24
#define TX_RING_STRIDE			0x1000	/* 4KB */
#define RX_RING_STRIDE			0x100	/* 256B */
#define INTERRUPT_TYPES_COUNT		11
#define PORT_COUNT			9
#define ETH_PORT_START			1
#define ETH_PORT_END			6
#define CPU_PORT			0
#define LOOPBACK_PORT			7
#define EIP_PORT			8

/* Buffer size constants */
#define RX_BUFFER_SIZE			2048
#define TX_BUFFER_SIZE			2048
#define RXFILL_DESC_SIZE		16
#define RXDESC_DESC_SIZE		32
#define TXDESC_DESC_SIZE		32
#define TXDESC_SEC_DESC_SIZE		32
#define TXCMPL_DESC_SIZE		16

/* EDMA configuration constants */
#define EDMA_SW_VERSION			EDMA_SW_VER_3_ID
#define MAX_TXCMPL_RINGS		20
#define MAX_TXDESC_RINGS		24
#define MAX_RXDESC_RINGS		24
#define MAX_RXFILL_RINGS		20

/* PPE buffer management constants */
#define PPE_GROUP_BUF_SIZE		3550
#define PPE_AC_GROUP_TOTAL_BUF		4000

/* Polling timeout and delay constants */
#define UNIPHY_POLLING_TIMEOUT		2000
#define UNIPHY_POLLING_DELAY		1

/* ============================================================================
 * QSERDES_TX (Transmitter) Register Addresses
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
 * ============================================================================ */
#define QSERDES_RX_EXT_UCDR_FASTLOCK_FO_GAIN_RATE6_ADDRESS	0xca08
#define QSERDES_RX_EXT_UCDR_FASTLOCK_SO_GAIN_RATE6_ADDRESS	0xca10
#define QSERDES_RX_EXT_UCDR_FASTLOCK_COUNT_HIGH_RATE6_ADDRESS	0xca20
#define QSERDES_RX_EXT_RX_TERM_BW_CTRL2_ADDRESS		0xca54
#define QSERDES_RX_EXT_UCDR_FO_GAIN_RATE6_ADDRESS		0xca5c
#define QSERDES_RX_EXT_UCDR_SO_GAIN_RATE5_ADDRESS		0xca60
#define QSERDES_RX_EXT_UCDR_SO_GAIN_RATE6_ADDRESS		0xca64
#define QSERDES_RX_EXT_AUXDATA_BIN_RATE56_ADDRESS		0xca6c
#define QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE5_ADDRESS	0xca74
#define QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE6_ADDRESS	0xca78
#define QSERDES_RX_EXT_RX_MODE_RATE5_B0_ADDRESS		0xca7c
#define QSERDES_RX_EXT_RX_MODE_RATE5_B1_ADDRESS		0xca80
#define QSERDES_RX_EXT_RX_MODE_RATE5_B2_ADDRESS		0xca84
#define QSERDES_RX_EXT_RX_MODE_RATE5_B3_ADDRESS		0xca88
#define QSERDES_RX_EXT_RX_MODE_RATE5_B4_ADDRESS		0xca8c
#define QSERDES_RX_EXT_RX_MODE_RATE5_B5_ADDRESS		0xca90
#define QSERDES_RX_EXT_RX_MODE_RATE5_B6_ADDRESS		0xca94
#define QSERDES_RX_EXT_RX_MODE_RATE5_B7_ADDRESS		0xca98
#define QSERDES_RX_EXT_RX_MODE_RATE5_B8_ADDRESS		0xca9c
#define QSERDES_RX_EXT_RX_MODE_RATE5_B9_ADDRESS		0xcaa0
#define QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS		0xcaa4
#define QSERDES_RX_EXT_RX_MODE_RATE6_B1_ADDRESS		0xcaa8
#define QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS		0xcaac
#define QSERDES_RX_EXT_RX_MODE_RATE6_B3_ADDRESS		0xcab0
#define QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS		0xcab4
#define QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS		0xcab8
#define QSERDES_RX_EXT_RX_MODE_RATE6_B6_ADDRESS		0xcabc
#define QSERDES_RX_EXT_RX_MODE_RATE6_B7_ADDRESS		0xcac0
#define QSERDES_RX_EXT_RX_MODE_RATE6_B8_ADDRESS		0xcac4
#define QSERDES_RX_EXT_RX_MODE_RATE6_B9_ADDRESS		0xcac8
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
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL4_ADDRESS	0xcc74
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL5_ADDRESS	0xcc78
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL6_ADDRESS	0xcc7c
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL7_ADDRESS	0xcc80
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL8_ADDRESS	0xcc84
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL9_ADDRESS	0xcc88
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL10_ADDRESS	0xcc8c
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL11_ADDRESS	0xcc90
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL12_ADDRESS	0xcc94
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL13_ADDRESS	0xcc98
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL14_ADDRESS	0xcc9c
#define QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL15_ADDRESS	0xcca0
#define QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL5_ADDRESS		0xcca4
#define QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL6_ADDRESS		0xcca8
#define QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS		0xccac
#define QSERDES_RX_EXT_VGA_CAL_MAN_VAL2_ADDRESS		0xccc8
#define QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS		0xcccc
#define QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS		0xccd0
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
#define QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS		0xcdfc

/* ============================================================================
 * QSERDES_COM (Common PLL) Register Addresses
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


/*
 * Hardware configuration structure
 */
static struct edma_hw_cfg ipq9650_hw_cfg = {
	/* Global register configuration */
	.global = {
		.mas_ctrl = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.port_ctrl = {
			.offset			= 0x04,
			.mask			= 0x221F9,	/* Includes DDRQ fields */
			.shift			= 0,
		},
		.rxdesc2fill_map_0 = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,	/* 7-bit fields */
			.shift			= 0,
		},
		.rxdesc2fill_map_1 = {
			.offset			= 0x18,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.rxdesc2fill_map_2 = {
			.offset			= 0x1C,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.dmar_ctrl = {
			.offset			= 0x48,
			.mask			= 0x1DDF0,	/* Expanded fields */
			.shift			= 0,
		},
		.misc_int_stat = {
			.offset			= 0x5C,
			.mask			= 0x7FF,	/* 11 interrupt types */
			.shift			= 0,
		},
		.misc_int_mask = {
			.offset			= 0x60,
			.mask			= 0x7FF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_0 = {
			.offset			= 0x8C,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_1 = {
			.offset			= 0x90,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_2 = {
			.offset			= 0x94,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_3 = {
			.offset			= 0x98,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
	},

	/* TX Descriptor Ring Configuration - 24 rings */
	.txdesc = {
		.base_offset			= 0x1000,
		.ring_increment			= TX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x18,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x1C,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x0C,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ctrl = {
			.offset			= 0x10,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x10,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0x28,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0x2c,
			.mask			= 0x0,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x30,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x34,
			.mask			= 0x0,
			.shift			= 0,
		},

		/* TXDESC-specific masks - v3 descriptor */
		.tx_en				= 0x1,
		.buf_hi_add_mask		= 0xFF,
		.data_offset_mask		= 0xFFF,
		.data_offset_shift		= 0,
		.data_length_mask		= 0xFFFF,	/* v3: 16-bit (bits 0-15) */
		.data_length_shift		= 0,
	},

	/* TX Completion Ring Configuration - 20 rings */
	.txcmpl = {
		.base_offset			= 0x79000,
		.ring_increment			= TX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x1C,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x04,
			.mask			= 0x0,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x1c,
			.mask			= 0x0,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x0C,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ctrl = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x14,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0x20000,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0x20004,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x2000c,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x34,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},

		/* TXCMPL-specific masks */
		.ring_int_status_mask		= 0x3,
	},

	/* RX Fill Ring Configuration - 20 rings */
	.rxfill = {
		.base_offset			= 0x29000,
		.ring_increment			= RX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x28,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x1c,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x10,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.buf_size = {
			.offset			= 0x10,
			.mask			= 0xFFFF0000,
			.shift			= 16,
		},
		.ctrl = {
			.offset			= 0x10,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x1C,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0xd000,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0xd004,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x30,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},

		/* RXFILL-specific masks */
		.buf_hi_add_mask		= 0xFF,
		.ring_int_status_mask		= 0x1,
	},

	/* RX Descriptor Ring Configuration - 24 rings */
	.rxdesc = {
		.base_offset			= 0x4A000,
		.ring_increment			= RX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x2C,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x28,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x30,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x0c,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.pl_offset = {
			.offset			= 0x10,
			.mask			= 0xFF800000,
			.shift			= 23,
		},
		.ctrl = {
			.offset			= 0x18,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x18,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0x18000,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0x18004,
			.mask			= 0x0,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x1800c,
			.mask			= 0x0,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x10,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},

		/* RXDESC-specific masks */
		.rx_en				= 0x1,
		.srcinfo_type_mask		= 0xF000,
		.pkt_size_mask			= 0xFFFF,	/* v3: 16-bit field (bits 0-15) */
		.pkt_size_shift			= 0,
		.ring_int_status_mask		= 0x3,
		.portnum_bits			= 0x0FFF,
	},

	/* QID2RID configuration */
	.qid2rid_base_offset		= 0xB9000,
	.qid2rid_increment		= 0x4,

	/* Interrupt control configuration */
	.int_ctrl_base_offset		= 0x62000,
	.int_ctrl_ring_increment	= 0x1000,
	.int_ctrl_reg_offset		= 0xc,

	/* Hardware-specific parameters */
	.ring_dma_mask			= 0xFFFFFFFF,
	.rx_ring_size			= IS_ENABLED(CONFIG_ETH_LOW_MEM) ? 32 : 128,
	.tx_ring_size			= IS_ENABLED(CONFIG_ETH_LOW_MEM) ? 32 : 128,
	.rx_buff_size			= RX_BUFFER_SIZE,
	.tx_buff_size			= TX_BUFFER_SIZE,
	.rxfill_desc_size		= RXFILL_DESC_SIZE,
	.rxdesc_desc_size		= RXDESC_DESC_SIZE,
	.txdesc_desc_size		= TXDESC_DESC_SIZE,
	.txdesc_sec_desc_size		= TXDESC_SEC_DESC_SIZE,
	.txcmpl_desc_size		= TXCMPL_DESC_SIZE,

	/* Interrupt masks */
	.rxfill_int_mask		= 0x1,
	.rxdesc_int_mask		= 0x1,
	.txcmpl_int_mask		= 0x1,
	.misc_intr_mask			= 0x7FF,
	.rx_payload_offset		= 0x0,

	/* Common masks used across multiple rings */
	.tx_int_mask			= 0x3,
	.rx_ne_int_en			= 0x2,
	.tx_ne_int_en			= 0x2,

	/* Destination port configuration */
	.dst_port_type			= 2,
	.dst_port_type_shift		= 28,
	.dst_port_type_mask		= 0xf0000000,
	.dst_port_id_shift		= 16,
	.dst_port_id_mask		= 0x0fff0000,

	/* Register initialization values */
	.port_ctrl_init_val		= 0x6,	/* EDMA v3: PAD_MODE(0x2) | EDMA_EN(0x4) */

	/* DMAR_CTRL configurable values - EDMA v3 expanded limits */
	.dmar_txdata_outstanding_num	= 31,	/* 6-bit field (max 63) */
	.dmar_txdesc_outstanding_num	= 7,	/* 4-bit field (max 15) */
	.dmar_rxfill_outstanding_num	= 7,	/* 4-bit field (max 15) */

	/* DMAR_CTRL bit field configuration - EDMA v3 */
	.dmar_txdata_mask		= 0x3f,		/* 6 bits (0-63) */
	.dmar_txdata_shift		= 4,		/* Bits [9:4] */
	.dmar_txdesc_mask		= 0xf,		/* 4 bits (0-15) */
	.dmar_txdesc_shift		= 10,		/* Bits [13:10] */
	.dmar_rxfill_mask		= 0xf,		/* 4 bits (0-15) */
	.dmar_rxfill_shift		= 14,		/* Bits [17:14] */
};

/*
 * Port Configuration
 * Includes all PHY types from reference designs:
 *
 * - SFP10G_PHY_TYPE: 10G SFP with 10GBASE_R mode
 * - QCA81xx_PHY_TYPE: QCA81xx PHY with USXGMII mode
 * - QCA8081_PHY_TYPE: QCA8081 PHY with SGMII0_RGMII4 mode
 * - QCA8337_SWITCH_TYPE: QCA8337 switch with SGMII0_RGMII4 mode
 * - QCA8033_PHY_TYPE: QCA8033 PHY with SGMII0_RGMII4 mode
 * - QCA8075_PHY_TYPE: QCA8075 PHY with PSGMII mode
 *
 * Each configuration includes:
 * - .id: PHY type identifier
 * - .clk_rate[]: Clock rates for different speeds {10, 100, 1000, 10000, 2500, 5000}
 * - .mac_mode[]: MAC modes for different speeds
 * - .modes[]: Port wrapper modes for different speeds
 */
static struct ipq_eth_port_config ipq9650_port_config[] = {
	{
		SFP10G_PHY_TYPE,
		{
			CLK_312_5_MHZ,			/* 10M */
			CLK_312_5_MHZ,			/* 100M */
			CLK_312_5_MHZ,			/* 1000M */
			CLK_312_5_MHZ,			/* 10000M */
			CLK_312_5_MHZ,			/* 2500M */
			CLK_312_5_MHZ			/* 5000M */
		},
		{
			XGMAC,				/* 10M */
			XGMAC,				/* 100M */
			XGMAC,				/* 1000M */
			XGMAC,				/* 10000M */
			XGMAC,				/* 2500M */
			XGMAC				/* 5000M */
		},
		{
			PORT_WRAPPER_10GBASE_R,		/* 10M */
			PORT_WRAPPER_10GBASE_R,		/* 100M */
			PORT_WRAPPER_10GBASE_R,		/* 1000M */
			PORT_WRAPPER_10GBASE_R,		/* 10000M */
			PORT_WRAPPER_10GBASE_R,		/* 2500M */
			PORT_WRAPPER_10GBASE_R		/* 5000M */
		},
	},
	/* QCA81xx PHY Type */
	{
		QCA81xx_PHY_TYPE,
		{
			-1,				/* 10M - not supported */
			CLK_12_5_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			CLK_312_5_MHZ,			/* 10000M */
			CLK_78_125_MHZ,			/* 2500M */
			CLK_156_25_MHZ			/* 5000M */
		},
		{
			-1,				/* 10M - not supported */
			XGMAC,				/* 100M */
			XGMAC,				/* 1000M */
			XGMAC,				/* 10000M */
			XGMAC,				/* 2500M */
			XGMAC				/* 5000M */
		},
		{
			-1,				/* 10M - not supported */
			PORT_WRAPPER_USXGMII,		/* 100M */
			PORT_WRAPPER_USXGMII,		/* 1000M */
			PORT_WRAPPER_USXGMII,		/* 10000M */
			PORT_WRAPPER_USXGMII,		/* 2500M */
			PORT_WRAPPER_USXGMII		/* 5000M */
		},
	},
	/* QCA8081 PHY Type */
	{
		QCA8081_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			-1,				/* 10000M - not supported */
			CLK_312_5_MHZ			/* 2500M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC,				/* 1000M */
			-1,				/* 10000M - not supported */
			XGMAC				/* 2500M */
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,	/* 10M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 100M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 1000M */
			-1,				/* 10000M - not supported */
			PORT_WRAPPER_SGMII_PLUS		/* 2500M */
		},
	},
	/* QCA8337 Switch Type */
	{
		QCA8337_SWITCH_TYPE,
		{
			CLK_125_MHZ,			/* 10M */
			CLK_125_MHZ,			/* 100M */
			CLK_125_MHZ			/* 1000M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC				/* 1000M */
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,	/* 10M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 100M */
			PORT_WRAPPER_SGMII0_RGMII4	/* 1000M */
		},
	},
	/* QCA8033 PHY Type */
	{
		QCA8033_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			-1,				/* 10000M - not supported */
			-1				/* 2500M - not supported */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC,				/* 1000M */
			-1,				/* 10000M - not supported */
			-1				/* 2500M - not supported */
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,	/* 10M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 100M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 1000M */
			-1,				/* 10000M - not supported */
			-1				/* 2500M - not supported */
		},
	},
	/* QCA8075 PHY Type */
	{
		QCA8075_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ			/* 1000M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC				/* 1000M */
		},
		{
			PORT_WRAPPER_PSGMII,		/* 10M */
			PORT_WRAPPER_PSGMII,		/* 100M */
			PORT_WRAPPER_PSGMII		/* 1000M */
		},
	},
	/* QCE1204 PHY Type */
	{
		QCE1204_PHY_TYPE,
		{
			CLK_1_25_MHZ,
			CLK_12_5_MHZ,
			CLK_125_MHZ,
			-1,
			CLK_78_125_MHZ,			/* 2500M */
		},
		{
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC
		},
		{
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
		},
	},
	/* Unused PHY Type - terminator */
	{
		UNUSED_PHY_TYPE,
	},
};

/*
 * VSI configuration array - CONFIG_ETH_MAX_MAC = 6
 */
u32 nb_vsi_config[CONFIG_ETH_MAX_MAC] = {
	0x03, 0x05, 0x09, 0x11, 0x21, 0x41
};

/*
 * PPE Table Address Configuration
 *
 * Example usage for VSI table:
 * Address = IPE_L2_BASE + VSI_TBL_OFFSET + (vsi_id * entry_size)
 * Address = 0x540000 + 0x3800 + (vsi_id * 0x10)
 */
static struct ppe_table_addr_config ipq9650_ppe_table_addrs = {
	.vsi_tbl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x3800,	/* VSI_TBL_OFFSET */
		.increment		= 0x10,		/* VSI_TBL_INC (16 bytes per entry) */
	},
	.vp_port_tbl = {
		.base_addr		= 0x200000,	/* PPE_IPE_L3_BASE_ADDR */
		.offset			= 0x4000,	/* VP_PORT_TBL_OFFSET */
		.increment		= 0x10,		/* VP_PORT_TBL_INC (16 bytes per entry) */
	},
	.port_fc_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x1000,	/* PORT_FC_CFG_OFFSET */
		.increment		= 0x10,		/* PORT_FC_CFG_INC (16 bytes per entry) */
	},
	.mru_mtu_ctrl_tbl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x5000,	/* MRU_MTU_CTRL_TBL_OFFSET */
		.increment		= 0x10,		/* MRU_MTU_CTRL_TBL_INC (16 bytes) */
	},
	.mc_mtu_ctrl_tbl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0xa00,	/* MC_MTU_CTRL_TBL_OFFSET */
		.increment		= 0x4,		/* MC_MTU_CTRL_TBL_INC (4 bytes) */
	},
	.port_bridge_ctrl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x300,	/* PORT_BRIDGE_CTRL_OFFSET */
		.increment		= 0x4,		/* PORT_BRIDGE_CTRL_INC (4 bytes) */
	},
	.cst_state = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x100,	/* CST_STATE_ADDRESS */
		.increment		= 0x4,		/* CST_STATE_INC (4 bytes per entry) */
	},
	.ipo_action = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x8000,	/* IPO_ACTION_OFFSET */
		.increment		= 0x20,		/* IPO_ACTION_INC (32 bytes per entry) */
	},
	.port_eg_vlan_tbl = {
		.base_addr		= 0x600000,	/* NSS_PTX_CSR_BASE_ADDR */
		.offset			= 0x40,		/* PORT_EG_VLAN_TBL_OFFSET */
		.increment		= 0x4,		/* PORT_EG_VLAN_TBL_INC (16 bytes) */
	},
	.eg_bridge_config = {
		.base_addr		= 0x600000,	/* NSS_PTX_CSR_BASE_ADDR */
		.offset			= 0x84,		/* EG_BRIDGE_CONFIG_ADDRESS */
		.increment		= 0x4,		/* Single 32-bit register */
	},
	.ipo_rule_reg = {
		.base_addr		= 0x0b0000,	/* IPO_CSR_BASE_ADDR */
		.offset			= 0x0,		/* IPO_RULE_REG_OFFSET */
		.increment		= 0x10,		/* IPO_RULE_REG_INC (16 bytes per entry) */
	},
	.ipo_mask_reg = {
		.base_addr		= 0x0b0000,	/* IPO_CSR_BASE_ADDR */
		.offset			= 0x2000,	/* IPO_MASK_REG_OFFSET */
		.increment		= 0x10,		/* IPO_MASK_REG_INC (16 bytes per entry) */
	},
	.tl_port_vp_tbl = {
		.base_addr		= 0x200000,	/* PPE_IPE_L3_BASE_ADDR */
		.offset			= 0x2000,	/* TL_PORT_VP_TBL_OFFSET */
		.increment		= 0x10,		/* TL_PORT_VP_TBL_INC (16 bytes) */
	},
	.ac_uni_queue_cfg_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x4e000,	/* AC_UNI_QUEUE_CFG_TBL_OFFSET */
		.increment		= 0x20,		/* AC_UNI_QUEUE_CFG_TBL_INC (32 bytes) */
	},
	.ac_mul_queue_cfg_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x50000,	/* AC_MUL_QUEUE_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* AC_MUL_QUEUE_CFG_TBL_INC (16 bytes) */
	},
	.ac_grp_cfg_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x51000,	/* AC_GRP_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* AC_GRP_CFG_TBL_INC (16 bytes) */
	},
	.ucast_queue_map_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x10000,	/* UCAST_QUEUE_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* UCAST_QUEUE_MAP_TBL_INC (16 bytes) */
	},
	.ucast_priority_map_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x30000,	/* UCAST_PRIORITY_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* UCAST_PRIORITY_MAP_TBL_INC (16 bytes) */
	},
	.l0_flow_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x2000,	/* L0_FLOW_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L0_FLOW_MAP_TBL_INC (16 bytes) */
	},
	.l0_flow_port_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x8000,	/* L0_FLOW_PORT_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L0_FLOW_PORT_MAP_TBL_INC (16 bytes) */
	},
	.l1_flow_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x40000,	/* L1_FLOW_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L1_FLOW_MAP_TBL_INC (16 bytes) */
	},
	.l1_flow_port_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x46000,	/* L1_FLOW_PORT_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L1_FLOW_PORT_MAP_TBL_INC (16 bytes) */
	},
	.psch_tdm_cfg_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x7a000,	/* PSCH_TDM_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* PSCH_TDM_CFG_TBL_INC (16 bytes) */
	},
	.port_bufgrp_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x460,	/* PORT_BUFGRP_CFG_OFFSET */
		.increment		= 0x4,		/* PORT_BUFGRP_CFG_INC (4 bytes) */
	},
	.port_shp_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x240,	/* PORT_SHP_CFG_OFFSET */
		.increment		= 0x4,		/* PORT_SHP_CFG_INC (4 bytes per entry) */
	},
	.port_cnt_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x100,	/* PORT_CNT_CFG_OFFSET */
		.increment		= 0x4,		/* PORT_CNT_CFG_INC (4 bytes per entry) */
	},
	.l0_comp_cfg_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x28000,	/* L0_COMP_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* L0_COMP_CFG_TBL_INC (16 bytes) */
	},
	.l1_comp_cfg_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x6a000,	/* L1_COMP_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* L1_COMP_CFG_TBL_INC (16 bytes) */
	},
	.ac_mcast_queue_en_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x4a000,	/* AC_MCAST_QUEUE_EN_TBL_OFFSET */
		.increment		= 0x10,		/* AC_MCAST_QUEUE_EN_TBL_INC (16 bytes) */
	},
	.l2_global_conf = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x38,		/* L2_GLOBAL_CONF_OFFSET */
		.increment		= 0x4,		/* Single 32-bit register */
	},
};

/*
 * Port Configuration
 * 8 ports (0-7):
 * - Port 0: CPU port
 * - Ports 1-6: Ethernet ports
 * - Port 7: Loopback/EIP port
 */
static struct ppe_port_config ipq9650_port_cfg = {
	.port_cpu		= CPU_PORT,
	.port_eth_start		= ETH_PORT_START,
	.port_eth_end		= ETH_PORT_END,
	.port_loopback		= LOOPBACK_PORT,
	.port_eip		= EIP_PORT,
	.num_ports		= PORT_COUNT,
	.reserved		= 0,
};

/*
 * EDMA Configuration
 * Architecture with 24 TX rings, 20 RX fill rings
 */
struct edma_config ipq_edma_config = {
	.sw_version		= EDMA_SW_VERSION,
	.txdesc_ring_start	= 0,
	.txdesc_rings		= 1,
	.txdesc_ring_end	= 1,
	.txcmpl_ring_start	= 0,
	.txcmpl_rings		= 1,
	.txcmpl_ring_end	= 1,
	.rxfill_ring_start	= 0,
	.rxfill_rings		= 1,
	.rxfill_ring_end	= 1,
	.rxdesc_ring_start	= 0,
	.rxdesc_rings		= 1,
	.rxdesc_ring_end	= 1,
	.tx_map			= 4,
	.rx_map			= 3,
	.max_txcmpl_rings	= MAX_TXCMPL_RINGS,
	.max_txdesc_rings	= MAX_TXDESC_RINGS,
	.max_rxdesc_rings	= MAX_RXDESC_RINGS,
	.max_rxfill_rings	= MAX_RXFILL_RINGS,
	.iports			= 9,
	.ports			= 6,
	.start_ports		= 1,
	.vsi			= 0x1FF,
	.ipo_action		= 6,
	.tdm_ctrl_val		= 0x800000A0,
	.hw_cfg			= &ipq9650_hw_cfg,
};

/*
 * TDM Configurations
 * Multiple TDM configurations for different board variants
 * Board selects config via device tree tdm_mode property (array index)
 */
static struct ipq_tdm_config ipq9650_tdm_config[] = {
	{
		.val = {
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x00 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x01 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),    /* 0x02 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),     /* 0x03 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x04 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x05 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 1, 1),    /* 0x06 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 1, 1),     /* 0x07 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x08 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x09 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 1, 1),    /* 0x0A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 1, 1),     /* 0x0B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x0C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x0D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x0E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x0F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 1, 1),    /* 0x10 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 1, 1),     /* 0x11 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x12 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x13 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x14 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x15 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),    /* 0x16 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),     /* 0x17 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x18 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x19 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),    /* 0x1A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),     /* 0x1B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x1C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x1D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 8, 1, 1),    /* 0x1E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 8, 1, 1),     /* 0x1F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x20 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x21 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x22 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x23 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),    /* 0x24 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),     /* 0x25 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x26 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x27 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x28 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x29 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 8, 1, 7),    /* 0x2A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 8, 1, 7),     /* 0x2B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x2C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x2D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x2E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x2F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 7),    /* 0x30 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 7),     /* 0x31 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x32 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x33 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),    /* 0x34 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),     /* 0x35 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 0),    /* 0x36 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 0),     /* 0x37 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x38 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x39 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),    /* 0x3A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),     /* 0x3B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 1, 1),    /* 0x3C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 1, 1),     /* 0x3D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x3E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x3F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x40 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x41 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 1, 1),    /* 0x42 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 1, 1),     /* 0x43 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 7),    /* 0x44 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 7),     /* 0x45 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x46 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x47 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 1, 1),    /* 0x48 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 1, 1),     /* 0x49 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x4A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x4B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x4C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x4D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),    /* 0x4E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),     /* 0x4F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x50 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x51 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x52 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x53 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 8, 1, 1),    /* 0x54 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 8, 1, 1),     /* 0x55 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 7),    /* 0x56 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 7),     /* 0x57 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x58 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x59 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 8),    /* 0x5A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 8),     /* 0x5B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x5C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x5D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x5E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x5F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 7),    /* 0x60 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 7),     /* 0x61 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 1),    /* 0x62 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 1),     /* 0x63 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x64 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x65 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 7),    /* 0x66 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 7),     /* 0x67 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 1),    /* 0x68 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 1),     /* 0x69 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x6A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x6B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 7),    /* 0x6C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 7),     /* 0x6D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x6E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x6F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x70 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x71 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 8),    /* 0x72 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 8),     /* 0x73 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x74 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x75 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 1, 1),    /* 0x76 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 1, 1),     /* 0x77 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x78 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x79 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 1, 1),    /* 0x7A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 1, 1),     /* 0x7B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x7C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x7D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x7E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x7F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 1, 1),    /* 0x80 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 1, 1),     /* 0x81 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x82 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x83 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x84 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x85 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 7),    /* 0x86 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 7),     /* 0x87 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 1),    /* 0x88 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 1),     /* 0x89 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 8, 1, 7),    /* 0x8A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 8, 1, 7),     /* 0x8B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 1),    /* 0x8C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 1),     /* 0x8D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 8),    /* 0x8E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 8),     /* 0x8F */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 1),    /* 0x90 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 1),     /* 0x91 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),    /* 0x92 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),     /* 0x93 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 8),    /* 0x94 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 8),     /* 0x95 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),    /* 0x96 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),     /* 0x97 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 8),    /* 0x98 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 8),     /* 0x99 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),    /* 0x9A */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),     /* 0x9B */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 8),    /* 0x9C */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 8),     /* 0x9D */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),    /* 0x9E */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),     /* 0x9F */
		},
		.depth		= 160,
	},
	/* Add more configurations here as needed for different board variants */
};

/*
 * Scheduler configuration table
 */
static struct ipq_sch_config ipq9650_ppe_port_scheduler0_tbl[] = {
	{
		.val = {
			PSCH(0x9f, 0x7, 0x5, 0x0, 0x0),    /* 0: ENS=7, DES=5 */
			PSCH(0x19e, 0x8, 0x6, 0x0, 0x0),   /* 1: ENS=8, DES=6 */
			PSCH(0x1be, 0x5, 0x0, 0x0, 0x0),   /* 2: ENS=5, DES=0 */
			PSCH(0x19e, 0x1, 0x6, 0x0, 0x0),   /* 3: ENS=1, DES=6 */
			PSCH(0x19d, 0x0, 0x5, 0x0, 0x0),   /* 4: ENS=0, DES=5 */
			PSCH(0x1dd, 0x6, 0x1, 0x0, 0x0),   /* 5: ENS=6, DES=1 */
			PSCH(0x19c, 0xf, 0x5, 0x0, 0x0),   /* 6: ENS=15, DES=5 */
			PSCH(0x198, 0xf, 0x6, 0x0, 0x1),   /* 7: ENS=15, DES=6 */
			PSCH(0x1b8, 0x5, 0x2, 0x1, 0x1),   /* 8: ENS=5, DES=2 */
			PSCH(0x199, 0xf, 0x6, 0x0, 0x0),   /* 9: ENS=15, DES=6 */
			PSCH(0x19e, 0x2, 0x5, 0x0, 0x0),   /* 10: ENS=2, DES=5 */
			PSCH(0x1de, 0x6, 0x0, 0x0, 0x0),   /* 11: ENS=6, DES=0 */
			PSCH(0x19e, 0xf, 0x5, 0x0, 0x0),   /* 12: ENS=15, DES=5 */
			PSCH(0x194, 0xf, 0x6, 0x0, 0x1),   /* 13: ENS=15, DES=6 */
			PSCH(0x1b4, 0x5, 0x3, 0x1, 0x1),   /* 14: ENS=5, DES=3 */
			PSCH(0x194, 0xf, 0x6, 0x0, 0x0),   /* 15: ENS=15, DES=6 */
			PSCH(0x18c, 0x3, 0x5, 0x0, 0x1),   /* 16: ENS=3, DES=5 */
			PSCH(0x1cc, 0x6, 0x4, 0x1, 0x1),   /* 17: ENS=6, DES=4 */
			PSCH(0x18d, 0xf, 0x5, 0x0, 0x0),   /* 18: ENS=15, DES=5 */
			PSCH(0x19e, 0x4, 0x6, 0x0, 0x0),   /* 19: ENS=4, DES=6 */
			PSCH(0x1be, 0x5, 0x0, 0x0, 0x0),   /* 20: ENS=5, DES=0 */
			PSCH(0x19e, 0xf, 0x6, 0x0, 0x0),   /* 21: ENS=15, DES=6 */
			PSCH(0x11d, 0x0, 0x5, 0x0, 0x0),   /* 22: ENS=0, DES=5 */
			PSCH(0x15c, 0x6, 0x7, 0x1, 0x1),   /* 23: ENS=6, DES=7 */
			PSCH(0x11c, 0xf, 0x5, 0x0, 0x1),   /* 24: ENS=15, DES=5 */
			PSCH(0x9e, 0x7, 0x6, 0x0, 0x0),    /* 25: ENS=7, DES=6 */
			PSCH(0xbe, 0x5, 0x8, 0x0, 0x0),    /* 26: ENS=5, DES=8 */
			PSCH(0x9e, 0xf, 0x6, 0x0, 0x1),    /* 27: ENS=15, DES=6 */
			PSCH(0x19e, 0x8, 0x5, 0x0, 0x0),   /* 28: ENS=8, DES=5 */
			PSCH(0x1de, 0x6, 0x0, 0x0, 0x0),   /* 29: ENS=6, DES=0 */
			PSCH(0x19e, 0x1, 0x5, 0x0, 0x0),   /* 30: ENS=1, DES=5 */
			PSCH(0x19d, 0x0, 0x6, 0x0, 0x0),   /* 31: ENS=0, DES=6 */
			PSCH(0x1bd, 0x5, 0x1, 0x0, 0x0),   /* 32: ENS=5, DES=1 */
			PSCH(0x19c, 0xf, 0x6, 0x0, 0x0),   /* 33: ENS=15, DES=6 */
			PSCH(0x198, 0xf, 0x5, 0x0, 0x1),   /* 34: ENS=15, DES=5 */
			PSCH(0x1d8, 0x6, 0x2, 0x1, 0x1),   /* 35: ENS=6, DES=2 */
			PSCH(0x199, 0xf, 0x5, 0x0, 0x0),   /* 36: ENS=15, DES=5 */
			PSCH(0x19e, 0x2, 0x6, 0x0, 0x0),   /* 37: ENS=2, DES=6 */
			PSCH(0x1be, 0x5, 0x0, 0x0, 0x0),   /* 38: ENS=5, DES=0 */
			PSCH(0x19e, 0xf, 0x6, 0x0, 0x0),   /* 39: ENS=15, DES=6 */
			PSCH(0x195, 0xf, 0x5, 0x0, 0x0),   /* 40: ENS=15, DES=5 */
			PSCH(0x1d5, 0x6, 0x3, 0x1, 0x1),   /* 41: ENS=6, DES=3 */
			PSCH(0x195, 0xf, 0x5, 0x0, 0x0),   /* 42: ENS=15, DES=5 */
			PSCH(0x18d, 0x3, 0x6, 0x0, 0x0),   /* 43: ENS=3, DES=6 */
			PSCH(0x1ad, 0x5, 0x4, 0x1, 0x1),   /* 44: ENS=5, DES=4 */
			PSCH(0x18d, 0xf, 0x6, 0x0, 0x0),   /* 45: ENS=15, DES=6 */
			PSCH(0x19e, 0x4, 0x5, 0x0, 0x0),   /* 46: ENS=4, DES=5 */
			PSCH(0x1de, 0x6, 0x0, 0x0, 0x0),   /* 47: ENS=6, DES=0 */
			PSCH(0x19e, 0xf, 0x5, 0x0, 0x0),   /* 48: ENS=15, DES=5 */
			PSCH(0x11d, 0x0, 0x6, 0x0, 0x0),   /* 49: ENS=0, DES=6 */
			PSCH(0x13d, 0x5, 0x7, 0x1, 0x1),   /* 50: ENS=5, DES=7 */
			PSCH(0x11c, 0xf, 0x6, 0x0, 0x0),   /* 51: ENS=15, DES=6 */
			PSCH(0x9e, 0x7, 0x5, 0x0, 0x1),    /* 52: ENS=7, DES=5 */
			PSCH(0xde, 0x6, 0x8, 0x0, 0x0),    /* 53: ENS=6, DES=8 */
			PSCH(0x9e, 0xf, 0x5, 0x0, 0x1),    /* 54: ENS=15, DES=5 */
			PSCH(0x19e, 0x8, 0x6, 0x0, 0x0),   /* 55: ENS=8, DES=6 */
			PSCH(0x1be, 0x5, 0x0, 0x0, 0x0),   /* 56: ENS=5, DES=0 */
			PSCH(0x19e, 0x1, 0x6, 0x0, 0x0),   /* 57: ENS=1, DES=6 */
			PSCH(0x19d, 0x0, 0x5, 0x0, 0x0),   /* 58: ENS=0, DES=5 */
			PSCH(0x1dd, 0x6, 0x1, 0x0, 0x0),   /* 59: ENS=6, DES=1 */
			PSCH(0x19d, 0xf, 0x5, 0x0, 0x0),   /* 60: ENS=15, DES=5 */
			PSCH(0x199, 0xf, 0x6, 0x0, 0x0),   /* 61: ENS=15, DES=6 */
			PSCH(0x1b9, 0x5, 0x2, 0x1, 0x1),   /* 62: ENS=5, DES=2 */
			PSCH(0x199, 0xf, 0x6, 0x0, 0x0),   /* 63: ENS=15, DES=6 */
			PSCH(0x19e, 0x2, 0x5, 0x0, 0x0),   /* 64: ENS=2, DES=5 */
			PSCH(0x1de, 0x6, 0x0, 0x0, 0x0),   /* 65: ENS=6, DES=0 */
			PSCH(0x19e, 0xf, 0x5, 0x0, 0x0),   /* 66: ENS=15, DES=5 */
			PSCH(0x196, 0xf, 0x6, 0x0, 0x0),   /* 67: ENS=15, DES=6 */
			PSCH(0x1b6, 0x5, 0x3, 0x0, 0x1),   /* 68: ENS=5, DES=3 */
			PSCH(0x196, 0xf, 0x6, 0x0, 0x0),   /* 69: ENS=15, DES=6 */
			PSCH(0x18e, 0x3, 0x5, 0x0, 0x0),   /* 70: ENS=3, DES=5 */
			PSCH(0x1ce, 0x6, 0x4, 0x0, 0x1),   /* 71: ENS=6, DES=4 */
			PSCH(0x18e, 0xf, 0x5, 0x0, 0x0),   /* 72: ENS=15, DES=5 */
			PSCH(0x19e, 0x4, 0x6, 0x0, 0x0),   /* 73: ENS=4, DES=6 */
			PSCH(0x13c, 0xf, 0x0, 0x0, 0x0),   /* 74: ENS=15, DES=0 */
		},
		.depth		= 75,
	},
};

/*
 * TDM Address Configuration
 * Shared across all TDM configurations - same for all boards
 */
static struct ipq_tdm_addr_config ipq9650_tdm_addr_config = {
	.tdm_addr = {
		.base_addr		= 0x0,		/* Relative to PPE base */
		.offset			= 0xc000,	/* TDM offset from PPE base */
		.increment		= 0x10,		/* 16 bytes per TDM entry */
	},
	.tdm_ctrl_offset		= 0xb000,	/* TDM control register offset */
};

/* Global exports required by nss-switch.c */
struct ipq_eth_port_config	*port_config		= ipq9650_port_config;
struct ipq_eth_sku		*ipq_uniphy;
struct ppe_table_addr_config	*ppe_table_addrs	= &ipq9650_ppe_table_addrs;
struct ppe_port_config		*ppe_port_cfg		= &ipq9650_port_cfg;

/* Export pointers - nss-switch.c will index tdm_config based on tdm_mode from DTS */
struct ipq_tdm_config		*tdm_config		= ipq9650_tdm_config;
struct ipq_tdm_addr_config	*tdm_addr_config	= &ipq9650_tdm_addr_config;
struct ipq_sch_config		*sch_config		= ipq9650_ppe_port_scheduler0_tbl;

static struct ipq_eth_sku ipq9650_uniphy[CONFIG_ETH_MAX_UNIPHY] = {
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

struct ipq_eth_sku *ipq_uniphy = ipq9650_uniphy;

/*
 * Common clock configuration function - dummy implementation
 */
void ipq_config_cmn_clock(void)
{
	writel(0x157, 0x9b41c);
}

/**
 * port_init() - One-time port initialization (SoC-specific implementation)
 * @port: Port info structure
 *
 * Sets SoC-specific function pointers on the port_info structure.
 * Called once per port during probe, before any mode configuration.
 * Each SoC's port_config.c provides its own implementation of this function.
 */
void port_init(struct port_info *port)
{
	/* IPQ9650 uses QSERDES-based SerDes: use QSERDES calibration */
	port->calibrate = ppe_uniphy_serdes_calibration;
}

/**
 * ipq_get_group_buf() - Get the buffer group size for the current chip
 *
 * Returns the configured buffer group size value for PPE buffer management.
 * This value is chip-specific:
 *
 * Return: Buffer group size value
 */
u16 ipq_get_group_buf(void)
{
	return PPE_GROUP_BUF_SIZE;
}

/**
 * ipq_get_ac_group_total_buf() - Get the AC group total buffer size
 *
 * Returns the configured total buffer value for AC group configuration.
 * This value is chip-specific:
 *
 * Return: Total buffer value for AC group
 */
u16 ipq_get_ac_group_total_buf(void)
{
	return PPE_AC_GROUP_TOTAL_BUF;
}

enum csr_version uniphy_get_csr_version(void)
{
	return CSR_VERSION_V2;
}
/**
 * uniphy_pma_init_internal() - PMA init setting (internal, all DFE modes)
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 * @uniphy_mode: port wrapper mode (PORT_WRAPPER_25GBASE_R or other)
 * @is_long: true for long channel, false for short channel
 * @dfe_mode: 0=HW tuning, 1=SW tuning, 2=DFE disabled
 *
 * Single implementation for all three PMA DFE modes.
 * Differences per mode:
 *   RXEQ_CTRL26:      0x80 (HW) | 0x40 (SW/disabled)
 *   DFE_TRAIN_TIME_RATE4/5/6: present (HW) | absent (SW/disabled)
 *   RXEQ_CTRL27:      0x0A (HW) | 0x05 (SW/disabled)
 *   DFE_SUPPORTED:    present (HW+SW) | absent (disabled)
 *   RXEQ_CTRL9:       present (SW) | absent (HW/disabled)
 *   TAP1 settings:    present (HW) | absent (SW/disabled)
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_init_internal(int uniphy_index, u32 uniphy_mode,
				    bool is_long, int dfe_mode)
{
	debug("UNIPHY %d: Initializing PMA dfe_mode=%d\n", uniphy_index, dfe_mode);

	/* Common settings */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_POWER_DOWN_CONTROL_ADDRESS), 0x01);

	/* QSERDES PLL Settings - vco_mode0 for SGMII & QSGMII */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CP_CTRL_MODE0_ADDRESS), 0x17);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_RCTRL_MODE0_ADDRESS), 0x19);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_CCTRL_MODE0_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MODE0_ADDRESS), 0xA0);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MSB_MODE0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START1_MODE0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START2_MODE0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START3_MODE0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN0_MODE0_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN1_MODE0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORECLK_DIV_MODE0_ADDRESS), 0x05);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP1_MODE0_ADDRESS), 0xFF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP2_MODE0_ADDRESS), 0x0F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE1_MODE0_ADDRESS), 0x68);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE2_MODE0_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE_INITVAL1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE_INITVAL2_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE0_ADDRESS), 0xE0);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE0_ADDRESS), 0x12);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_IVCO_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_DC_LEVEL_CTRL_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_HSCLK_SEL_1_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORE_CLK_EN_ADDRESS), 0x82);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_POST_DIV_MUX_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_SVS_MODE_CLK_SEL_ADDRESS), 0x11);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_CONFIG_1_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_1_ADDRESS), 0x11);

	/* vco_mode2 for SGMII+ & PSGMII & 12.5GPHY */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CP_CTRL_MODE2_ADDRESS), 0x17);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_RCTRL_MODE2_ADDRESS), 0x1B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_CCTRL_MODE2_ADDRESS), 0x19);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MODE2_ADDRESS), 0xC8);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MSB_MODE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START1_MODE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START2_MODE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START3_MODE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN0_MODE2_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN1_MODE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORECLK_DIV_MODE2_ADDRESS), 0x05);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_HSCLK_SEL_2_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_CONFIG_2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP1_MODE2_ADDRESS), 0xFF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP2_MODE2_ADDRESS), 0x13);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE1_MODE2_ADDRESS), 0xF7);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE2_MODE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE_INITVAL1_HI_FREQ_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE_INITVAL2_HI_FREQ_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_2_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE2_ADDRESS), 0xBB);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE2_ADDRESS), 0x0F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_IVCO_MODE2_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_ADDITIONAL_MISC_3_ADDRESS), 0x04);
	/* For PSGMII & QSGMII */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORECLK_DIV_MODE0_QP_ADDRESS), 0x0A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_HSCLK_SEL_QP_ADDRESS), 0x11);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORECLK_DIV_MODE2_QP_ADDRESS), 0x0A);

	/* vco_mode1 for USXGMII */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CP_CTRL_MODE1_ADDRESS), 0x17);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_RCTRL_MODE1_ADDRESS), 0x19);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_CCTRL_MODE1_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MODE1_ADDRESS), 0xA5);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MSB_MODE1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START1_MODE1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START2_MODE1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START3_MODE1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN0_MODE1_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN1_MODE1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORECLK_DIV_MODE1_ADDRESS), 0x0A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP1_MODE1_ADDRESS), 0xFF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP2_MODE1_ADDRESS), 0x20);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE1_MODE1_ADDRESS), 0x19);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE2_MODE1_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE1_ADDRESS), 0x77);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE1_ADDRESS), 0x13);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_IVCO_MODE1_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_ADDITIONAL_MISC_2_ADDRESS), 0x04);
	/* For 12.5GPHY only */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP1_MODE4_ADDRESS), 0xFF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP2_MODE4_ADDRESS), 0x27);

	/* vco_mode3 for 25GPHY */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CP_CTRL_MODE3_ADDRESS), 0x17);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_RCTRL_MODE3_ADDRESS), 0x1C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_CCTRL_MODE3_ADDRESS), 0x24);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MODE3_ADDRESS), 0x37);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DEC_START_MSB_MODE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START1_MODE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START2_MODE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_DIV_FRAC_START3_MODE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN0_MODE3_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_INTEGLOOP_GAIN1_MODE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CORECLK_DIV_MODE3_ADDRESS), 0x0A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_HSCLK_SEL_3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_CONFIG_3_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_ADDITIONAL_CTRL_3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP1_MODE3_ADDRESS), 0x7F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP2_MODE3_ADDRESS), 0x1B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE1_MODE3_ADDRESS), 0x95);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_VCO_TUNE2_MODE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_HSCLK_SEL_3_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE1_MODE3_ADDRESS), 0xD0);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BIN_VCOCAL_CMP_CODE2_MODE3_ADDRESS), 0x0A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_IVCO_MODE3_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_ADDITIONAL_MISC_6_ADDRESS), 0x10);

	/* All mode shared settings */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_LOCK_CMP_EN_ADDRESS), 0x42);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_SYS_CLK_CTRL_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_SSC_EN_CENTER_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_PLL_CNTRL_ADDRESS), 0x23);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_SYSCLK_EN_SEL_ADDRESS), 0xDA);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_BG_TIMER_ADDRESS), 0x0B);

	/* QSERDES TX and TX Ext Settings */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_SEL_20B_10B_ADDRESS), 0x1C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_TX_BAND0_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_TX_HR_SEL_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_RES_CODE_LANE_OFFSET_TX_ADDRESS), 0x03);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_RES_CODE_LANE_OFFSET_RX_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_LANE_MODE_1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_RES_CODE_LANE_TX_ADDRESS), 0x56);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_RES_CODE_LANE_RX_ADDRESS), 0x4E);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_LANE_MODE_2_ADDRESS), 0xD0);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_LANE_MODE_3_ADDRESS), 0x40);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_ADAPTOR_MODE_CTRL1_ADDRESS), 0x2A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_ADAPTOR_MODE_CTRL3_ADDRESS), 0x0A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B0_ADDRESS), 0xC1);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B1_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B2_ADDRESS), 0xC8);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B3_ADDRESS), 0x1E);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B4_ADDRESS), 0x35);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B5_ADDRESS), 0x80);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B6_ADDRESS), 0x24);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_RX_MODE_RATE1_B8_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_EMP_POST1_LVL_RATE4_ADDRESS), 0x0F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_EMP_POST1_LVL_RATE5_ADDRESS), 0x0F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_LANE_MODE_BITS_RATE56_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_EMP_POST1_LVL_RATE6_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_PREEMPH_RATE6_ADDRESS), 0x10);

	/* QSERDES RX and RX Ext Settings */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SO_GAIN_RATE0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_PI_CTRL1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_TERM_BW_CTRL0_ADDRESS), 0x55);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B0_ADDRESS), 0xC1);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B1_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B2_ADDRESS), 0xC8);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B3_ADDRESS), 0x1A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B4_ADDRESS), 0x35);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B5_ADDRESS), 0x80);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B6_ADDRESS), 0x20);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE_0_1_B8_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_BAND_CTRL0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_Q_EN_RATES_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_SVS_MODE_CTRL_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RXCLK_DIV2_CTRL_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3_ADDRESS), 0x48);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SO_GAIN_RATE1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SO_GAIN_RATE2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_FO_GAIN_RATE2_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B0_ADDRESS), 0xD3);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B1_ADDRESS), 0x13);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B2_ADDRESS), 0xC8);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B3_ADDRESS), 0x1E);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B4_ADDRESS), 0xB5);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B5_ADDRESS), 0x80);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B6_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE2_B8_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SO_GAIN_RATE3_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_FO_GAIN_RATE3_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B0_ADDRESS), 0xD3);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B1_ADDRESS), 0x13);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B2_ADDRESS), 0xC8);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B3_ADDRESS), 0x1E);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B4_ADDRESS), 0xB5);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B5_ADDRESS), 0x80);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B6_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE3_B8_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SB2_GAIN1_RATE3_ADDRESS), 0x05);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SB2_GAIN2_RATE3_ADDRESS), 0x05);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SO_GAIN_RATE4_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_PI_CTRL2_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_TERM_BW_CTRL1_ADDRESS), 0x01);
	/* RX_MODE_RATE4: same values for short and long channel */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B0_ADDRESS), 0x15);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B1_ADDRESS), 0x15);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B2_ADDRESS), 0x80);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B3_ADDRESS), 0x1A);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B4_ADDRESS), 0x39);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B5_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B6_ADDRESS), 0xEF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_MODE_RATE4_B8_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_DLL_CTRL2_ADDRESS), 0x20);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_SO_SATURATION_ADDRESS), 0x1F);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_UCDR_PI_CONTROLS_ADDRESS), 0x06);
	if (is_long)
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS), 0xA9);
	else
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VGA_CAL_MAN_VAL_ADDRESS), 0x39);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4_ADDRESS), 0x09);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1_ADDRESS), 0x14);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL3_ADDRESS), 0x0E);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_DLL0_FTUNE_CTRL_ADDRESS), 0x30);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_GM_CAL_ADDRESS), 0x10);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE0_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_AUXDATA_BIN_RATE01_ADDRESS), 0x44);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE1_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE2_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_AUXDATA_BIN_RATE23_ADDRESS), 0x44);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE3_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_VTHRESH_CAL_MAN_VAL_RATE4_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_AUXDATA_BIN_RATE4_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_SIGDET_ENABLES_ADDRESS), 0x10);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_SIGDET_CNTRL_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_SIGDET_DEGLITCH_CNTRL_ADDRESS), 0x18);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_SIGDET_LVL_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_DFE_EN_TIMER_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RXEQ_CTRL3_ADDRESS), 0x7F);
	/* Enable vga_dfe_gap_time */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RXEQ_CTRL5_ADDRESS), 0x16);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE12_ADDRESS), 0x11);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE34_ADDRESS), 0x11);
	/* RXEQ_CTRL26: 0x80 for HW tuning, 0x40 for SW tuning and DFE disabled */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RXEQ_CTRL26_ADDRESS),
		  dfe_mode == 0 ? 0x80 : 0x40);
	/* DFE_TRAIN_TIME_RATE4: HW tuning only */
	if (dfe_mode == 0)
		csr_write(uniphy_index,
			  CSR0_ADDR(QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE4_ADDRESS), 0x28);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_UCDR_SO_GAIN_RATE5_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_TERM_BW_CTRL2_ADDRESS), 0x05);
	/* RX_MODE_RATE5: same values for short and long channel */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B0_ADDRESS), 0x15);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B1_ADDRESS), 0x15);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B2_ADDRESS), 0x80);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B3_ADDRESS), 0x1B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B4_ADDRESS), 0x39);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B5_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B6_ADDRESS), 0xEF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B8_ADDRESS), 0xA8);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE5_B9_ADDRESS), 0x02);
	/* RXEQ_CTRL27: 0x0A for HW tuning, 0x05 for SW tuning and DFE disabled */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RXEQ_CTRL27_ADDRESS),
		  dfe_mode == 0 ? 0x0A : 0x05);
	/* DFE_TRAIN_TIME_RATE5: HW tuning only */
	if (dfe_mode == 0)
		csr_write(uniphy_index,
			  CSR0_ADDR(QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE5_ADDRESS), 0x28);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_DFE_GAP_TIME_RATE56_ADDRESS), 0x11);
	csr_write(uniphy_index,
		  CSR0_ADDR(QSERDES_RX_EXT_DLL_FTUNE_CAL_TIME_RATE456_ADDRESS), 0x30);
	csr_write(uniphy_index,
		  CSR0_ADDR(QSERDES_RX_EXT_UCDR_FASTLOCK_FO_GAIN_RATE6_ADDRESS), 0x1C);
	csr_write(uniphy_index,
		  CSR0_ADDR(QSERDES_RX_EXT_UCDR_FASTLOCK_SO_GAIN_RATE6_ADDRESS), 0x06);
	csr_write(uniphy_index,
		  CSR0_ADDR(QSERDES_RX_EXT_UCDR_FASTLOCK_COUNT_HIGH_RATE6_ADDRESS), 0x02);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_UCDR_FO_GAIN_RATE6_ADDRESS), 0x0C);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_UCDR_SO_GAIN_RATE6_ADDRESS), 0x00);

	/* RX_MODE_RATE6 settings - long/short channel dependent */
	if (is_long)
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS), 0xBF);
	else
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B0_ADDRESS), 0xFF);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B1_ADDRESS), 0x3F);
	if (is_long)
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS), 0x40);
	else
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B3_ADDRESS), 0x98);
	if (is_long) {
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS), 0x58);
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS), 0x12);
	} else {
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B4_ADDRESS), 0x4A);
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B5_ADDRESS), 0x00);
	}
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B6_ADDRESS), 0xF3);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B7_ADDRESS), 0x60);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B8_ADDRESS), 0xFB);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_RATE6_B9_ADDRESS), 0x07);
	/* DFE_TRAIN_TIME_RATE6: HW tuning only */
	if (dfe_mode == 0)
		csr_write(uniphy_index,
			  CSR0_ADDR(QSERDES_RX_EXT_DFE_TRAIN_TIME_RATE6_ADDRESS), 0x28);
	/* DFE_SUPPORTED: HW tuning and SW tuning, NOT DFE disabled */
	if (dfe_mode <= 1)
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_DFE_SUPPORTED_ADDRESS), 0x70);
	csr_write(uniphy_index,
		  CSR0_ADDR(QSERDES_RX_EXT_DCC_CLK_EXTRA_DIV2_EN_RATE_CTRL0_ADDRESS), 0x40);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B0_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE0_B1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE1_B1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL5_ADDRESS), 0x99);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_CAL_MAN_VAL2_ADDRESS), 0x59);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL4_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL10_ADDRESS), 0x38);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE2_B1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL5_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL11_ADDRESS), 0x38);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B0_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE3_B1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL6_ADDRESS), 0xE9);
	if (is_long)
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS), 0x75);
	else
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_CAL_MAN_VAL3_ADDRESS), 0x35);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL6_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL12_ADDRESS), 0x38);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B0_ADDRESS), 0xAA);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_MODE_HIGH_RATE4_B1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL7_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL13_ADDRESS), 0x38);
	if (is_long) {
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS), 0xBE);
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS), 0x07);
	} else {
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_EQU_ADAPTOR_CNTRL7_ADDRESS), 0xDE);
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VGA_CAL_MAN_VAL4_ADDRESS), 0x03);
	}
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE5_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_AUXDATA_BIN_RATE56_ADDRESS), 0x24);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL8_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL14_ADDRESS), 0x38);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_VTHRESH_CAL_MAN_VAL_RATE6_ADDRESS), 0x0B);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL9_ADDRESS), 0x01);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_OFFSET_ADAPTOR_CNTRL15_ADDRESS), 0x38);

	/* RXEQ_CTRL9: SW tuning only - mask RXEQ_ENGINE_DONE flag */
	if (dfe_mode == 1)
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RXEQ_CTRL9_ADDRESS), 0x40);

	/* Placeholder registers */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_MODE_ADDRESS), 0x04);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_MODE_CONTD_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_MODE_CONTD1_ADDRESS), 0x64);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_COM_CMN_MODE_CONTD2_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_DLL_HR_DCC_ICLK_CTRL1_ADDRESS), 0x00);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_DLL_CTRL1_ADDRESS), 0x10);
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RX_EYEMON_CTRL0_ADDRESS), 0x00);

	/* TAP1 settings: HW tuning only (USXGMII, 12.5GPHY, 25GAUI Serdes modes) */
	if (dfe_mode == 0) {
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_TAP1ADP_CTRL1_ADDRESS), 0x04);
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_TAP1CODE_MAN_VAL_ADDRESS), 0x00);
		if (uniphy_mode == PORT_WRAPPER_25GBASE_R) /* 25GAUI mode */
			csr_write(uniphy_index,
				  CSR0_ADDR(QSERDES_TX_EXT_TAP1CODE_TARGET_VAL_ADDRESS), 0x0A);
		else /* 12.5GPHY or USXGMII mode */
			csr_write(uniphy_index,
				  CSR0_ADDR(QSERDES_TX_EXT_TAP1CODE_TARGET_VAL_ADDRESS), 0x05);
		csr_write(uniphy_index, CSR0_ADDR(QSERDES_TX_EXT_TAP1CODE_STEP_VAL_ADDRESS), 0x01);
		csr_write(uniphy_index,
			  CSR0_ADDR(QSERDES_TX_EXT_TAP1CODE_STEP_TIME_ADDRESS), 0x0A);
	}

	debug("UNIPHY %d: PMA init dfe_mode=%d completed\n", uniphy_index, dfe_mode);
	return 0;
}

/**
 * uniphy_pma_dfe_hw_tuning_init() - PMA init setting with DFE hardware tuning
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 * @uniphy_mode: port wrapper mode (PORT_WRAPPER_25GBASE_R or other)
 * @is_long: true for long channel, false for short channel
 *
 * PMA DFE HW tuning initialization.
 * RXEQ_CTRL26=0x80, RXEQ_CTRL27=0x0A, DFE_TRAIN_TIME_RATE4/5/6=0x28,
 * DFE_SUPPORTED=0x70, TAP1 settings present.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_dfe_hw_tuning_init(int uniphy_index, u32 uniphy_mode,
					 bool is_long)
{
	return uniphy_pma_init_internal(uniphy_index, uniphy_mode, is_long, 0);
}

/**
 * uniphy_pma_dfe_sw_tuning_init() - PMA init setting with DFE software tuning
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 * @uniphy_mode: port wrapper mode (PORT_WRAPPER_25GBASE_R or other)
 * @is_long: true for long channel, false for short channel
 *
 * PMA DFE SW tuning initialization.
 * RXEQ_CTRL26=0x40, RXEQ_CTRL27=0x05, DFE_SUPPORTED=0x70,
 * RXEQ_CTRL9=0x40 (mask RXEQ_ENGINE_DONE). No DFE_TRAIN_TIME or TAP1.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_dfe_sw_tuning_init(int uniphy_index, u32 uniphy_mode,
					 bool is_long)
{
	return uniphy_pma_init_internal(uniphy_index, uniphy_mode, is_long, 1);
}

/**
 * uniphy_pma_dfe_disabled_init() - PMA init setting with DFE disabled
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 * @uniphy_mode: port wrapper mode (PORT_WRAPPER_25GBASE_R or other)
 * @is_long: true for long channel, false for short channel
 *
 * PMA DFE disabled initialization.
 * Same as SW tuning but WITHOUT DFE_SUPPORTED and WITHOUT RXEQ_CTRL9.
 * RXEQ_CTRL26=0x40, RXEQ_CTRL27=0x05. No DFE_TRAIN_TIME, no TAP1,
 * no DFE_SUPPORTED, no RXEQ_CTRL9.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_dfe_disabled_init(int uniphy_index, u32 uniphy_mode,
					bool is_long)
{
	return uniphy_pma_init_internal(uniphy_index, uniphy_mode, is_long, 2);
}

/**
 * uniphy_pma_dfe_sw_tune() - PMA DFE software tune sequence
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 *
 * Called after SW tuning init to run the DFE software tune sequence.
 *
 * Return: 0 on success, negative error code on failure
 */
static int uniphy_pma_dfe_sw_tune(int uniphy_index)
{
	u32 timeout = UNIPHY_POLLING_TIMEOUT;
	u32 reg_val, tap1_code;

	debug("UNIPHY %d: Starting PMA DFE software tuning\n", uniphy_index);

	/* Poll RO_PMAD_RXEQ_STATUS until bit[4] VGA_DONE is set */
	while (timeout > 0) {
		reg_val = csr_read(uniphy_index,
				   CSR0_ADDR(QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS));
		if (reg_val & BIT(4)) {
			debug("VGA_DONE detected\n");
			break;
		}
		mdelay(UNIPHY_POLLING_DELAY);
		timeout--;
	}

	if (timeout == 0) {
		printf("ERROR: UNIPHY %d VGA_DONE polling timeout\n", uniphy_index);
		return -ETIMEDOUT;
	}

	/* Write DFE TAP1 code sequence (0x80~0x8A) with 10us delays */
	for (tap1_code = 0x80; tap1_code <= 0x8A; tap1_code++) {
		csr_write(uniphy_index,
			  CSR0_ADDR(QSERDES_TX_EXT_DFE_TAP1_CODE_ADDRESS), tap1_code);
		udelay(10);
	}

	/* Unmask RXEQ_ENGINE_DONE flag */
	csr_write(uniphy_index, CSR0_ADDR(QSERDES_RX_EXT_RXEQ_CTRL9_ADDRESS), 0x00);

	debug("UNIPHY %d: PMA DFE software tuning completed\n", uniphy_index);
	return 0;
}

/**
 * uniphy_rxeq_status_check() - Poll RXEQ engine done status (IPQ9650 strong override)
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 *
 * IPQ9650 uses QSERDES-based SerDes which requires polling
 * QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS bit[1] (RXEQ_ENGINE_DONE) after
 * link-up to confirm the RX equalizer engine has completed adaptation.
 *
 * Other SoCs use the weak no-op default in nss_switch_v2.c.
 *
 * Return: 0 on success, -ETIMEDOUT on timeout
 */
int uniphy_rxeq_status_check(int uniphy_index)
{
	u32 reg_val;
	u32 timeout = 100;

	debug("UNIPHY %d: Polling RXEQ engine done\n", uniphy_index);

	while (timeout > 0) {
		reg_val = csr_read(uniphy_index,
				   CSR0_ADDR(QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS_ADDRESS));
		if (reg_val & BIT(1)) {
			debug("UNIPHY %d: RXEQ engine completed successfully\n", uniphy_index);
			return 0;
		}
		mdelay(1);
		timeout--;
	}

	printf("ERROR: UNIPHY %d RXEQ engine done polling timeout\n", uniphy_index);
	return -ETIMEDOUT;
}

/**
 * ppe_uniphy_uxgmii_mode_ctrl_val() - IPQ9650 USXGMII MODE_CONTROL value
 *
 * IPQ9650 uses USXGMII mode which requires USXG_EN (bit 13) in addition to
 * the base UQXGMII/UDXGMII bits.  The value is built directly from named
 * bit positions so the intent of each bit is self-documenting:
 *
 *   bit  0: ch0_autoneg_mode = 1  - enable autoneg
 *   [6:4]: ch0_mode_ctrl_25m = 2  - 25M clock mode
 *   bit 12: xpcs_mode        = 1  - XPCS mode enable
 *   bit 13: usxg_en          = 1  - USXGMII enable (IPQ9650-specific)
 *
 * Result: BIT(0) | (2 << 4) | BIT(12) | BIT(13) = 0x3021
 *
 * Note: The weak default in nss_switch_v2.c returns 0x1021 (no usxg_en).
 * This strong override adds bit 13 for IPQ9650 USXGMII support.
 *
 * Return: 32-bit value to write to PPE_UNIPHY_MODE_CONTROL
 */
u32 ppe_uniphy_uxgmii_mode_ctrl_val(void)
{
	/*
	 * Compute directly from bit positions - no local struct needed.
	 * bit  0: ch0_autoneg_mode = 1
	 * [6:4]: ch0_mode_ctrl_25m = 2  => (2 << 4) = 0x0020
	 * bit 12: xpcs_mode        = 1  => BIT(12)  = 0x1000
	 * bit 13: usxg_en          = 1  => BIT(13)  = 0x2000  (IPQ9650-specific)
	 */
	return BIT(0) | (2 << 4) | BIT(12) | BIT(13);	/* 0x3021 */
}

/**
 * uniphy_pma_init_setting() - PMA initialization settings
 * @port: pointer to port info structure containing uniphy_id
 * @uniphy_mode: port wrapper mode (PORT_WRAPPER_25GBASE_R or other)
 * @dfe_mode: 0 for DFE hardware tuning, 1 for DFE software tuning, 2 for DFE disabled
 * @is_long: true for long channel, false for short channel
 *
 * Main PMA initialization function that selects the appropriate DFE mode.
 * All register accesses use csr_write(index, CSR0_ADDR(offset), val) for
 * QSERDES registers (0xc000-0xdfff) via CSR0 direct access.
 *
 * Note: Uniphy reset should be handled by the caller before and after this function.
 *
 * Return: 0 on success, negative error code on failure
 */
int uniphy_pma_init_setting(struct port_info *port, u32 uniphy_mode,
			    u32 dfe_mode, bool is_long)
{
	int uniphy_index = port->uniphy_id;
	int ret = 0;

	debug("UNIPHY %d: PMA init with mode=%u, dfe_mode=%u, is_long=%d\n",
	      uniphy_index, uniphy_mode, dfe_mode, is_long);

	switch (dfe_mode) {
	case 0:
		ret = uniphy_pma_dfe_hw_tuning_init(uniphy_index, uniphy_mode, is_long);
		break;
	case 1:
		ret = uniphy_pma_dfe_sw_tuning_init(uniphy_index, uniphy_mode, is_long);
		if (!ret)
			ret = uniphy_pma_dfe_sw_tune(uniphy_index);
		break;
	case 2:
		ret = uniphy_pma_dfe_disabled_init(uniphy_index, uniphy_mode, is_long);
		break;
	default:
		printf("ERROR: UNIPHY %d unsupported DFE mode %u\n", uniphy_index, dfe_mode);
		return -EOPNOTSUPP;
	}

	if (ret)
		printf("ERROR: UNIPHY %d PMA initialization failed (dfe_mode=%u)\n",
		       uniphy_index, dfe_mode);

	return ret;
}
