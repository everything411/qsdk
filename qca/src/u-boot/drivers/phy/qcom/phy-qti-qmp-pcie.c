// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <generic-phy.h>
#include <log.h>
#include <power-domain.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dt-bindings/phy/phy.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <clk.h>
#include <reset.h>

#include "phy-qti-qmp-pcie.h"

/* QPHY_SW_RESET bit */
#define SW_RESET				BIT(0)
/* QPHY_POWER_DOWN_CONTROL */
#define SW_PWRDN				BIT(0)
#define REFCLK_DRV_DSBL				BIT(1)
/* QPHY_START_CONTROL bits */
#define SERDES_START				BIT(0)
#define PCS_START				BIT(1)
/* QPHY_PCS_STATUS bit */
#define PHYSTATUS				BIT(6)
#define PHYSTATUS_4_20				BIT(7)

#define PHY_INIT_COMPLETE_TIMEOUT		10000

#define QMP_PHY_INIT_CFG(o, v)		\
	{				\
		.offset = o,		\
		.val = v,		\
		.lane_mask = 0xff,	\
	}

#define QMP_PHY_INIT_CFG_L(o, v)	\
	{				\
		.offset = o,		\
		.val = v,		\
		.in_layout = true,	\
		.lane_mask = 0xff,	\
	}

#define QMP_PHY_INIT_CFG_LANE(o, v, l)	\
	{				\
		.offset = o,		\
		.val = v,		\
		.lane_mask = l,		\
	}

struct qmp_phy_init_tbl {
	unsigned int offset;
	unsigned int val;
	/*
	 * register part of layout ?
	 * if yes, then offset gives index in the reg-layout
	 */
	bool in_layout;
	/*
	 * mask of lanes for which this register is written
	 * for cases when second lane needs different values
	 */
	u8 lane_mask;
};

/* set of registers with offsets different per-PHY */
enum qphy_reg_layout {
        /* Common block control registers */
        QPHY_COM_SW_RESET,
        QPHY_COM_POWER_DOWN_CONTROL,
        QPHY_COM_START_CONTROL,
        QPHY_COM_PCS_READY_STATUS,
        /* PCS registers */
        QPHY_SW_RESET,
        QPHY_START_CTRL,
        QPHY_PCS_STATUS,
        QPHY_PCS_POWER_DOWN_CONTROL,
        /* Keep last to ensure regs_layout arrays are properly initialized */
        QPHY_LAYOUT_SIZE
};

static const unsigned int ipq_pciephy_gen3_regs_layout[QPHY_LAYOUT_SIZE] = {
        [QPHY_SW_RESET]                         = 0x00,
        [QPHY_START_CTRL]                       = 0x44,
        [QPHY_PCS_STATUS]                       = 0x14,
        [QPHY_PCS_POWER_DOWN_CONTROL]           = 0x40,
};

struct qmp_phy_cfg_tables {
	const struct qmp_phy_init_tbl *serdes;
	int serdes_num;
	const struct qmp_phy_init_tbl *tx;
	int tx_num;
	const struct qmp_phy_init_tbl *rx;
	int rx_num;
	const struct qmp_phy_init_tbl *pcs;
	int pcs_num;
	const struct qmp_phy_init_tbl *pcs_misc;
	int pcs_misc_num;
};

/* struct qmp_phy_cfg - per-PHY initialization config */
struct qmp_phy_cfg {
	int lanes;

	/* Main init sequence for PHY blocks - serdes, tx, rx, pcs */
	const struct qmp_phy_cfg_tables tables;

	/* array of registers with different offsets */
	const unsigned int *regs;

	unsigned int start_ctrl;
	unsigned int pwrdn_ctrl;
	/* bit offset of PHYSTATUS in QPHY_PCS_STATUS register */
	unsigned int phy_status;

	bool skip_start_delay;
};

/**
 * struct qmp_phy - per-lane phy descriptor
 *
 * @phy: generic phy
 * @cfg: phy specific configuration
 * @serdes: iomapped memory space for phy's serdes (i.e. PLL)
 * @tx: memory space for lane's tx
 * @rx: memory space for lane's rx
 * @pcs: memory space for lane's pcs
 * @tx2: memory space for second lane's tx (in dual lane PHYs)
 * @rx2: memory space for second lane's rx (in dual lane PHYs)
 * @pcs_misc: memory space for lane's pcs_misc
 * @pipe_clk: pipe clock
 * @qmp: QMP phy to which this lane belongs
 * @mode: currently selected PHY mode
 */
struct qmp_phy {
	struct phy *phy;
	const struct qmp_phy_cfg *cfg;
	phys_addr_t serdes;
	phys_addr_t tx1;
	phys_addr_t rx1;
	phys_addr_t pcs;
	phys_addr_t tx2;
	phys_addr_t rx2;
	phys_addr_t pcs_misc;
	struct clk *pipe_clk;
	struct qcom_qmp *qmp;
	int mode;
};

static const struct qmp_phy_init_tbl ipq9574_gen3x1_pcie_serdes_tbl[] = {
	QMP_PHY_INIT_CFG(QSERDES_PLL_BIAS_EN_CLKBUFLR_EN, 0x18),
	QMP_PHY_INIT_CFG(QSERDES_PLL_BIAS_EN_CTRL_BY_PSM, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_SELECT, 0x31),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_IVCO, 0x0F),
	QMP_PHY_INIT_CFG(QSERDES_PLL_BG_TRIM, 0x0F),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CMN_CONFIG, 0x06),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP_EN, 0x42),
	QMP_PHY_INIT_CFG(QSERDES_PLL_RESETSM_CNTRL, 0x20),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE_MAP, 0x04),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE_TIMER1, 0xFF),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE_TIMER2, 0x3F),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORE_CLK_EN, 0x30),
	QMP_PHY_INIT_CFG(QSERDES_PLL_HSCLK_SEL, 0x21),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DEC_START_MODE0, 0x68),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START3_MODE0, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START2_MODE0, 0xAA),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START1_MODE0, 0xAB),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP2_MODE0, 0x14),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP1_MODE0, 0xD4),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CP_CTRL_MODE0, 0x09),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_RCTRL_MODE0, 0x16),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_CCTRL_MODE0, 0x28),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN1_MODE0, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN0_MODE0, 0xA0),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE2_MODE0, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE1_MODE0, 0x24),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORE_CLK_EN, 0x20),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORECLK_DIV, 0x0A),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_SELECT, 0x32),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SYS_CLK_CTRL, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SYSCLK_BUF_ENABLE, 0x07),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SYSCLK_EN_SEL, 0x08),
	QMP_PHY_INIT_CFG(QSERDES_PLL_BG_TIMER, 0x0A),
	QMP_PHY_INIT_CFG(QSERDES_PLL_HSCLK_SEL, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DEC_START_MODE1, 0x53),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START3_MODE1, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START2_MODE1, 0x55),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START1_MODE1, 0x55),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP2_MODE1, 0x29),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP1_MODE1, 0xAA),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CP_CTRL_MODE1, 0x09),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_RCTRL_MODE1, 0x16),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_CCTRL_MODE1, 0x28),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN1_MODE1, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN0_MODE1, 0xA0),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE2_MODE1, 0x03),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE1_MODE1, 0xB4),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORE_CLK_EN, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORECLK_DIV_MODE1, 0x08),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_EN_CENTER, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_PER1, 0x7D),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_PER2, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_ADJ_PER1, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_ADJ_PER2, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE1_MODE0, 0x0A),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE2_MODE0, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE1_MODE1, 0x08),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE2_MODE1, 0x04),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_EP_DIV_MODE0, 0x19),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_EP_DIV_MODE1, 0x28),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_ENABLE1, 0x90),
	QMP_PHY_INIT_CFG(QSERDES_PLL_HSCLK_SEL, 0x89),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_ENABLE1, 0x10),
};

static const struct qmp_phy_init_tbl ipq9574_gen3x2_pcie_serdes_tbl[] = {
	QMP_PHY_INIT_CFG(QSERDES_PLL_BIAS_EN_CLKBUFLR_EN, 0x18),
	QMP_PHY_INIT_CFG(QSERDES_PLL_BIAS_EN_CTRL_BY_PSM, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_SELECT, 0x31),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_IVCO, 0x0F),
	QMP_PHY_INIT_CFG(QSERDES_PLL_BG_TRIM, 0x0F),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CMN_CONFIG, 0x06),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP_EN, 0x42),
	QMP_PHY_INIT_CFG(QSERDES_PLL_RESETSM_CNTRL, 0x20),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE_MAP, 0x04),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE_TIMER1, 0xFF),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE_TIMER2, 0x3F),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORE_CLK_EN, 0x30),
	QMP_PHY_INIT_CFG(QSERDES_PLL_HSCLK_SEL, 0x21),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DEC_START_MODE0, 0x68),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START3_MODE0, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START2_MODE0, 0xAA),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START1_MODE0, 0xAB),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP2_MODE0, 0x14),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP1_MODE0, 0xD4),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CP_CTRL_MODE0, 0x09),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_RCTRL_MODE0, 0x16),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_CCTRL_MODE0, 0x28),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN1_MODE0, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN0_MODE0, 0xA0),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE2_MODE0, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE1_MODE0, 0x24),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORE_CLK_EN, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORECLK_DIV, 0x0A),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_SELECT, 0x32),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SYS_CLK_CTRL, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SYSCLK_BUF_ENABLE, 0x07),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SYSCLK_EN_SEL, 0x08),
	QMP_PHY_INIT_CFG(QSERDES_PLL_BG_TIMER, 0x0A),
	QMP_PHY_INIT_CFG(QSERDES_PLL_HSCLK_SEL, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DEC_START_MODE1, 0x53),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START3_MODE1, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START2_MODE1, 0x55),
	QMP_PHY_INIT_CFG(QSERDES_PLL_DIV_FRAC_START1_MODE1, 0x55),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP2_MODE1, 0x29),
	QMP_PHY_INIT_CFG(QSERDES_PLL_LOCK_CMP1_MODE1, 0xAA),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CP_CTRL_MODE1, 0x09),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_RCTRL_MODE1, 0x16),
	QMP_PHY_INIT_CFG(QSERDES_PLL_PLL_CCTRL_MODE1, 0x28),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN1_MODE1, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_INTEGLOOP_GAIN0_MODE1, 0xA0),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE2_MODE1, 0x03),
	QMP_PHY_INIT_CFG(QSERDES_PLL_VCO_TUNE1_MODE1, 0xB4),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SVS_MODE_CLK_SEL, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORE_CLK_EN, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CORECLK_DIV_MODE1, 0x08),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_EN_CENTER, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_PER1, 0x7D),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_PER2, 0x01),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_ADJ_PER1, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_ADJ_PER2, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE1_MODE0, 0x0A),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE2_MODE0, 0x05),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE1_MODE1, 0x08),
	QMP_PHY_INIT_CFG(QSERDES_PLL_SSC_STEP_SIZE2_MODE1, 0x04),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_EP_DIV_MODE0, 0x19),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_EP_DIV_MODE1, 0x28),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_ENABLE1, 0x90),
	QMP_PHY_INIT_CFG(QSERDES_PLL_HSCLK_SEL, 0x89),
	QMP_PHY_INIT_CFG(QSERDES_PLL_CLK_ENABLE1, 0x10),
};

static const struct qmp_phy_init_tbl ipq9574_pcie_tx_tbl[] = {
	QMP_PHY_INIT_CFG(QSERDES_V4_TX_RES_CODE_LANE_OFFSET_TX, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_V4_TX_RCV_DETECT_LVL_2, 0x12),
	QMP_PHY_INIT_CFG(QSERDES_V4_TX_HIGHZ_DRVR_EN, 0x10),
	QMP_PHY_INIT_CFG(QSERDES_V4_TX_LANE_MODE_1, 0x06),
};

static const struct qmp_phy_init_tbl ipq9574_pcie_rx_tbl[] = {
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_SIGDET_CNTRL, 0x03),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_SIGDET_ENABLES, 0x1C),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_SIGDET_DEGLITCH_CNTRL, 0x14),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_EQU_ADAPTOR_CNTRL2, 0x61),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_EQU_ADAPTOR_CNTRL3, 0x04),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_EQU_ADAPTOR_CNTRL4, 0x1E),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_DFE_EN_TIMER, 0x04),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_UCDR_FO_GAIN, 0x0C),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_UCDR_SO_GAIN, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_UCDR_SO_SATURATION_AND_ENABLE, 0x7F),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_UCDR_PI_CONTROLS, 0x70),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1, 0x73),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_OFFSET_ADAPTOR_CNTRL2, 0x80),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_10_LOW, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_10_HIGH, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_10_HIGH2, 0xC8),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_10_HIGH3, 0x09),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_10_HIGH4, 0xB1),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_01_LOW, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_01_HIGH, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_01_HIGH2, 0xC8),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_01_HIGH3, 0x09),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_01_HIGH4, 0xB1),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_00_LOW, 0xF0),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_00_HIGH, 0x02),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_00_HIGH2, 0x2F),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_00_HIGH3, 0xD3),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_MODE_00_HIGH4, 0x40),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_IDAC_TSETTLE_HIGH, 0x00),
	QMP_PHY_INIT_CFG(QSERDES_V4_RX_RX_IDAC_TSETTLE_LOW, 0xC0),
};

static const struct qmp_phy_init_tbl ipq9574_gen3x1_pcie_pcs_tbl[] = {
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_P2U3_WAKEUP_DLY_TIME_AUXCLK_H, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_P2U3_WAKEUP_DLY_TIME_AUXCLK_L, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_RX_DCC_CAL_CONFIG, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_RX_SIGDET_LVL, 0xAA),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_REFGEN_REQ_CONFIG1, 0x0D),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_G12S1_TXDEEMPH_M3P5DB, 0x10),
};

static const struct qmp_phy_init_tbl ipq9574_gen3x1_pcie_pcs_misc_tbl[] = {
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_OSC_DTCT_ACTIONS, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_POWER_STATE_CONFIG2, 0x0D),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_L1P1_WAKEUP_DLY_TIME_AUXCLK_H, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_L1P1_WAKEUP_DLY_TIME_AUXCLK_L, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_L1P2_WAKEUP_DLY_TIME_AUXCLK_H, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_L1P2_WAKEUP_DLY_TIME_AUXCLK_L, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_EQ_CONFIG1, 0x14),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_EQ_CONFIG1, 0x10),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_EQ_CONFIG2, 0x0B),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_PRESET_P10_PRE, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_PRESET_P10_POST, 0x58),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_POWER_STATE_CONFIG4, 0x07),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_OSC_DTCT_CONFIG2, 0x52),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_INT_AUX_CLK_CONFIG1, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_OSC_DTCT_MODE2_CONFIG2, 0x50),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_OSC_DTCT_MODE2_CONFIG4, 0x1A),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_OSC_DTCT_MODE2_CONFIG5, 0x06),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_OSC_DTCT_MODE2_CONFIG6, 0x03),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_PCIE_ENDPOINT_REFCLK_DRIVE, 0xC1),
};

static const struct qmp_phy_init_tbl ipq9574_gen3x2_pcie_pcs_tbl[] = {
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_REFGEN_REQ_CONFIG1, 0x0D),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_G12S1_TXDEEMPH_M3P5DB, 0x10),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_P2U3_WAKEUP_DLY_TIME_AUXCLK_H, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_P2U3_WAKEUP_DLY_TIME_AUXCLK_L, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_RX_DCC_CAL_CONFIG, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V4_PCS_RX_SIGDET_LVL, 0xAA),
};

static const struct qmp_phy_init_tbl ipq9574_gen3x2_pcie_pcs_misc_tbl[] = {
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_ACTIONS, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_POWER_STATE_CONFIG2, 0x1D),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_L1P1_WAKEUP_DLY_TIME_AUXCLK_H, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_L1P1_WAKEUP_DLY_TIME_AUXCLK_L, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_L1P2_WAKEUP_DLY_TIME_AUXCLK_H, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_L1P2_WAKEUP_DLY_TIME_AUXCLK_L, 0x01),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_EQ_CONFIG1, 0x14),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_EQ_CONFIG1, 0x10),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_EQ_CONFIG2, 0x0B),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_PRESET_P10_PRE, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_PRESET_P10_POST, 0x58),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_POWER_STATE_CONFIG4, 0x07),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_CONFIG1, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_CONFIG2, 0x52),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_CONFIG4, 0x19),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_INT_AUX_CLK_CONFIG1, 0x00),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_MODE2_CONFIG2, 0x49),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_MODE2_CONFIG4, 0x2A),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_MODE2_CONFIG5, 0x02),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_OSC_DTCT_MODE2_CONFIG6, 0x03),
	QMP_PHY_INIT_CFG(QPHY_V5_PCS_PCIE_ENDPOINT_REFCLK_DRIVE, 0xC1),
};

static const struct qmp_phy_cfg ipq9574_gen3x1_pciephy_cfg = {
	.lanes                  = 1,

	.tables = {
		.serdes         = ipq9574_gen3x1_pcie_serdes_tbl,
		.serdes_num     = ARRAY_SIZE(ipq9574_gen3x1_pcie_serdes_tbl),
		.tx             = ipq9574_pcie_tx_tbl,
		.tx_num         = ARRAY_SIZE(ipq9574_pcie_tx_tbl),
		.rx             = ipq9574_pcie_rx_tbl,
		.rx_num         = ARRAY_SIZE(ipq9574_pcie_rx_tbl),
		.pcs            = ipq9574_gen3x1_pcie_pcs_tbl,
		.pcs_num        = ARRAY_SIZE(ipq9574_gen3x1_pcie_pcs_tbl),
		.pcs_misc       = ipq9574_gen3x1_pcie_pcs_misc_tbl,
		.pcs_misc_num   = ARRAY_SIZE(ipq9574_gen3x1_pcie_pcs_misc_tbl),
	},
	.regs                   = ipq_pciephy_gen3_regs_layout,

	.start_ctrl             = SERDES_START | PCS_START,
	.pwrdn_ctrl             = SW_PWRDN | REFCLK_DRV_DSBL,
	.phy_status             = PHYSTATUS,
};

static const struct qmp_phy_cfg ipq9574_gen3x2_pciephy_cfg = {
	.lanes                  = 2,

	.tables = {
		.serdes         = ipq9574_gen3x2_pcie_serdes_tbl,
		.serdes_num     = ARRAY_SIZE(ipq9574_gen3x2_pcie_serdes_tbl),
		.tx             = ipq9574_pcie_tx_tbl,
		.tx_num         = ARRAY_SIZE(ipq9574_pcie_tx_tbl),
		.rx             = ipq9574_pcie_rx_tbl,
		.rx_num		= ARRAY_SIZE(ipq9574_pcie_rx_tbl),
		.pcs            = ipq9574_gen3x2_pcie_pcs_tbl,
		.pcs_num        = ARRAY_SIZE(ipq9574_gen3x2_pcie_pcs_tbl),
		.pcs_misc       = ipq9574_gen3x2_pcie_pcs_misc_tbl,
		.pcs_misc_num   = ARRAY_SIZE(ipq9574_gen3x2_pcie_pcs_misc_tbl),
	},
	.regs                   = ipq_pciephy_gen3_regs_layout,

	.start_ctrl             = SERDES_START | PCS_START,
	.pwrdn_ctrl             = SW_PWRDN | REFCLK_DRV_DSBL,
	.phy_status             = PHYSTATUS,
};

static void qmp_pcie_configure(phys_addr_t base,
				const struct qmp_phy_init_tbl tbl[],
				int num)
{
	int i;
	const struct qmp_phy_init_tbl *t = tbl;

	if (!t)
		return;

	for (i = 0; i < num; i++, t++) {
		writel(t->val, base + t->offset);
	}
}

static int qti_pcie_phy_power_off(struct phy *phy)
{
	struct qmp_phy *qphy = dev_get_priv(phy->dev);
	const struct qmp_phy_cfg *cfg = qphy->cfg;

	/* PHY reset */
	setbits_le32(qphy->pcs + cfg->regs[QPHY_SW_RESET], SW_RESET);

	/* stop SerDes and Phy-Coding-Sublayer */
	clrbits_le32(qphy->pcs + cfg->regs[QPHY_START_CTRL], cfg->start_ctrl);

	return 0;
}

static int qti_pcie_phy_init(struct phy *phy)
{
	struct qmp_phy *qphy = dev_get_priv(phy->dev);
	const struct qmp_phy_cfg *cfg = qphy->cfg;

	/* set PCS power down control */
	setbits_le32(qphy->pcs + cfg->regs[QPHY_PCS_POWER_DOWN_CONTROL],
			cfg->pwrdn_ctrl);

	return 0;
}

static int qti_pcie_phy_exit(struct phy *phy)
{
	struct qmp_phy *qphy = dev_get_priv(phy->dev);
	const struct qmp_phy_cfg *cfg = qphy->cfg;

	/* Put PHY into POWER DOWN state: active low */
	clrbits_le32(qphy->pcs + cfg->regs[QPHY_PCS_POWER_DOWN_CONTROL],
			cfg->pwrdn_ctrl);

	return 0;
}

static int qti_pcie_phy_power_on(struct phy *phy)
{
	struct qmp_phy *qphy = dev_get_priv(phy->dev);
	const struct qmp_phy_cfg *cfg = qphy->cfg;
	const struct qmp_phy_cfg_tables *tables = &cfg->tables;
	struct clk_bulk clocks;
	phys_addr_t status;
	int mask, ret, val, ready = 0;

	/* configure serdes*/
	qmp_pcie_configure(qphy->serdes, tables->serdes, tables->serdes_num);

	/* configure TX1 */
	qmp_pcie_configure(qphy->tx1, tables->tx, tables->tx_num);

	/* configure TX2 */
	if (cfg->lanes >= 2)
		qmp_pcie_configure(qphy->tx2, tables->tx, tables->tx_num);

	/* configure RX1 */
	qmp_pcie_configure(qphy->rx1, tables->rx, tables->rx_num);

	if (cfg->lanes >= 2)
	/* configure RX2 */
		qmp_pcie_configure(qphy->rx2, tables->rx, tables->rx_num);

	/* configure PCS */
	qmp_pcie_configure(qphy->pcs, tables->pcs, tables->pcs_num);

	/* configure PCS_MISC */
	qmp_pcie_configure(qphy->pcs_misc, tables->pcs_misc,
				tables->pcs_misc_num);

	/* Pull PHY out of reset state */
	clrbits_le32(qphy->pcs + cfg->regs[QPHY_SW_RESET], SW_RESET);

	/* start SerDes and Phy-Coding-Sublayer */
	setbits_le32(qphy->pcs + cfg->regs[QPHY_START_CTRL], cfg->start_ctrl);

	if (!cfg->skip_start_delay)
		mdelay(10);

	/* Enable PIPE & LANE clk after enable init phy */
	ret = clk_get_bulk(phy->dev, &clocks);
	if (!ret) {
		ret = clk_enable_bulk(&clocks);
		if (ret) {
			dev_err(phy->dev, "Failed to enable clocks (ret=%d)\n",
					ret);
			return ret;
		}
	}

	status = qphy->pcs + cfg->regs[QPHY_PCS_STATUS];
	mask = cfg->phy_status;

	ret = readl_poll_timeout(status, val, (val & mask) == ready, 10000000);
	if (ret) {
		dev_err(phy->dev, "phy initialization timed-out\n");
	}

	return ret;
}

static int qti_pcie_phy_reset(struct phy *phy)
{
	qti_pcie_phy_power_off(phy);
	mdelay(1);
	qti_pcie_phy_exit(phy);
	mdelay(1);
	qti_pcie_phy_init(phy);
	mdelay(1);
	qti_pcie_phy_power_on(phy);

	return 0;
}

static int qti_pcie_phy_probe(struct udevice *dev)
{
	struct qmp_phy *qphy = dev_get_priv(dev);
	struct reset_ctl_bulk resets;
	int ret;

	ret = reset_get_bulk(dev, &resets);
	if (!ret) {
		ret = reset_assert_bulk(&resets);
		if (ret)
			return ret;

		mdelay(10);

		ret = reset_deassert_bulk(&resets);
		if (ret)
			return ret;
	}

	qphy->cfg = (struct qmp_phy_cfg *)dev_get_driver_data(dev);

	qphy->serdes = dev_read_addr_index(dev, 0);
	if (!qphy->serdes) {
		dev_err(dev, "Can't find phy base address \n");
		return -EINVAL;
        }

	qphy->tx1 = dev_read_addr_index(dev, 1);
	if (!qphy->tx1) {
		dev_err(dev, "Can't find Lane TX1 address\n");
		return -EINVAL;
        }

	qphy->rx1 = dev_read_addr_index(dev, 2);
        if (!qphy->rx1) {
		dev_err(dev, "Can't find Lane RX1 address\n");
		return -EINVAL;
        }

	qphy->pcs = dev_read_addr_index(dev, 3);
	if (!qphy->pcs) {
		dev_err(dev, "Can't find pcs address\n");
		return -EINVAL;
        }

	if (qphy->cfg->lanes >= 2) {
		qphy->tx2 = dev_read_addr_index(dev, 4);
		if (!qphy->tx2) {
			dev_err(dev, "Can't find Lane TX2 address\n");
			return -EINVAL;
		}

		qphy->rx2 = dev_read_addr_index(dev, 5);
		if (!qphy->rx2) {
			dev_err(dev, "Can't find Lane RX2 address\n");
			return -EINVAL;
		}

		qphy->pcs_misc = dev_read_addr_index(dev, 6);
		if (!qphy->pcs_misc) {
			dev_err(dev, "Can't find pcs_misc address\n");
			return -EINVAL;
		}
	} else {
		qphy->pcs_misc = dev_read_addr_index(dev, 4);
		if (!qphy->pcs_misc) {
			dev_err(dev, "Can't find pcs_misc address\n");
			return -EINVAL;
		}
	}

	return 0;
}

static const struct udevice_id qti_pcie_phy_ids[] = {
	{
		.compatible = "qti,ipq9574-qmp-gen3x1-pcie-phy",
		.data = (ulong)&ipq9574_gen3x1_pciephy_cfg,
	}, {
		.compatible = "qti,ipq9574-qmp-gen3x2-pcie-phy",
		.data = (ulong)&ipq9574_gen3x2_pciephy_cfg,
	},
	{ },
};

static const struct phy_ops qti_pcie_phy_ops = {
	.init = qti_pcie_phy_init,
	.exit = qti_pcie_phy_exit,
	.reset = qti_pcie_phy_reset,
	.power_on = qti_pcie_phy_power_on,
	.power_off = qti_pcie_phy_power_off,
};

U_BOOT_DRIVER(psgtr_phy) = {
	.name = "qti-qmp-phy",
	.id = UCLASS_PHY,
	.of_match = qti_pcie_phy_ids,
	.ops = &qti_pcie_phy_ops,
	.probe = qti_pcie_phy_probe,
	.priv_auto = sizeof(struct qmp_phy),
};
