/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include <linux/bitops.h>
#include <linux/iopoll.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/phy.h>
#include "qce1204.h"

struct clk_init_entry {
	struct clk **clk_ptr;
	const char *name;
	const char *desc;
};

struct reset_init_entry {
	struct reset_control **reset_ptr;
	const char *name;
	const char *desc;
};

#define QCE1204_PHY_SPEC_CONTROL					0x10
#define QCE1204_PHY_MDI_MASK						GENMASK(6, 5)
#define QCE1204_PHY_MDI_AUTO						0x3
#define QCE1204_PHY_MDI_X						0x1
#define QCE1204_PHY_MDI							0
#define QCE1204_PHY_SPEC_STATUS						0x11
#define QCE1204_PHY_SS_LINK_STATUS					0x400
#define QCE1204_PHY_INTR_DOWNSHIFT					0x20
#define QCE1204_PHY_SS_DUPLEX_FULL					0x2000
#define QCE1204_PHY_SS_SPEED_MASK					0x380
#define QCE1204_PHY_SS_SPEED_2500					0x200
#define QCE1204_PHY_SS_SPEED_1000					0x100
#define QCE1204_PHY_SS_SPEED_100					0x80
#define QCE1204_PHY_SS_SPEED_10						0
#define QCE1204_PHY_SS_MDIX						0x40
#define QCE1204_PHY_INTR_MASK						0x12

#define QCE1204_PHY_INTR_STATUS						0x13
#define QCE1204_PHY_INTR_STATUS_DOWN					0x800
#define QCE1204_PHY_INTR_STATUS_UP					0x400
#define QCE1204_PHY_1000BASET_CONTROL					0x9
#define QCE1204_PHY_ADVERTISE_1000FULL					0x200
#define QCE1204_PHY_1000BASET_STATUS					0xa
#define QCE1204_PHY_LP_ADVERTISE_1000FULL				0x800
#define QCE1204_PHY_CONTROL						0x19
#define QCE1204_PHY_FIFO_RESET						0x3
#define QCE1204_PHY_MMD7_IPG_OP						0x901d
#define QCE1204_PHY_IPG_10_TO_11_EN					BIT(0)
#define QCE1204_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL0				0xa02c
#define QCE1204_PHY_2P5G_EEE_TX_QUIET_TIME0				0x3db
#define QCE1204_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL1				0xa031
#define QCE1204_PHY_2P5G_EEE_TX_QUIET_TIME1				0x14
#define QCE1204_PHY_2P5G_EEE_TX_LPI_WAKE_CTRL				0xa10c
#define QCE1204_PHY_2P5G_EEE_TX_WAKE_TIME				0x2fc

#define QCE1204_MMD3_CDT_THRESH_CTRL14					0x807f
#define QCE1204_MMD3_CDT_THRESH_CTRL14_VAL				0x9ab0
#define QCE1204_MMD3_CDT_THRESH_CTRL2					0x8073
#define QCE1204_MMD3_CDT_THRESH_CTRL2_VAL				0xb03f
#define QCE1204_MMD3_CDT_THRESH_CTRL3					0x8074
#define QCE1204_MMD3_CDT_THRESH_CTRL3_VAL				0xc040
#define QCE1204_MMD3_CDT_THRESH_CTRL4					0x8075
#define QCE1204_MMD3_CDT_THRESH_CTRL4_VAL				0xa060
#define QCE1204_MMD3_CDT_THRESH_CTRL5					0x8076
#define QCE1204_MMD3_CDT_THRESH_CTRL5_VAL				0xc040
#define QCE1204_MMD3_CDT_THRESH_CTRL6					0x8077
#define QCE1204_MMD3_CDT_THRESH_CTRL6_VAL				0xa060
#define QCE1204_MMD3_CDT_THRESH_CTRL7					0x8078
#define QCE1204_MMD3_CDT_THRESH_CTRL7_VAL				0xae50
#define QCE1204_MMD3_CDT_THRESH_CTRL9					0x807a
#define QCE1204_MMD3_CDT_THRESH_CTRL9_VAL				0xc060
#define QCE1204_MMD3_CDT_THRESH_CTRL13					0x807e
#define QCE1204_MMD3_CDT_THRESH_CTRL13_VAL				0xb060

#define QCE1204_MMD3_FORCE_LINK_CTRL					0xa05a
#define QCE1204_FORCE_LINK_EN						BIT(6)

#define QCE1204_MMD7_LED0_CTRL						0x8078
#define QCE1204_SPEED_10M_ON						BIT(4)
#define QCE1204_MMD7_LED_POLARITY_CTRL					0x901a
#define QCE1204_LED_ACTIVE_HIGH						BIT(6)

#define QCE1204_DEBUG_ANA_10M_DAC_CTRL0					0x2880
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL0_VAL				0x7777
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL1					0x3880
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL1_VAL				0xa4a4
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL2					0x3980
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL2_VAL				0xa4a4
#define QCE1204_DEBUG_ANA_2P5G_TX_GAIN_CTRL				0xbb80
#define QCE1204_DEBUG_ANA_2P5G_TX_GAIN_VAL				0xf33
#define QCE1204_DEBUG_PLL_CTRL						0x1f
#define QCE1204_DEBUG_PLL_EN						BIT(2)
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL3					0xc980
#define QCE1204_DEBUG_ANA_10M_DAC_CTRL3_VAL				0xc0

/* 1G EEE configuration registers */
#define QCE1204_PHY_EEE_1G_TX_CTRL					0x8008
#define QCE1204_PHY_EEE_1G_TX_CTRL_VAL					0xe050
#define QCE1204_PHY_EEE_1G_RX_CTRL					0xa192
#define QCE1204_PHY_EEE_1G_RX_CTRL_VAL					0x2dec
/*
 * 1G EEE analog debug registers (CTRL0~CTRL2): Reset to zero to clear any
 * non-default state before applying tuned values in CTRL3~CTRL4.
 */
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL0					0x4f80
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL0_VAL				0x0000
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL1					0x5080
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL1_VAL				0x0000
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL2					0x5180
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL2_VAL				0x0000
/* 1G EEE analog tuning: CTRL3 and CTRL4 configure analog circuit parameters */
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL3					0x5280
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL3_VAL				0x1111
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL4					0x5380
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL4_VAL				0x7777
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL5					0x5480
#define QCE1204_DEBUG_ANA_EEE_1G_CTRL5_EN				BIT(15)

/* 100M EEE configuration registers */
/* 100M EEE analog tuning: Configure analog circuit parameters for 100M EEE */
#define QCE1204_DEBUG_ANA_EEE_100M_CTRL0				0x2980
#define QCE1204_DEBUG_ANA_EEE_100M_CTRL0_VAL				0x6666
#define QCE1204_DEBUG_ANA_EEE_100M_CTRL1				0x2a80
#define QCE1204_DEBUG_ANA_EEE_100M_CTRL1_VAL				0x0000

#define QCE1204_PCS_MMD1_CDA_CONTROL1					0x20
#define QCE1204_PCS_MMD1_CALIBRATION4					0x78
#define QCE1204_PCS_MMD1_MODE_CTRL					0x11b
#define QCE1204_PCS_MMD1_BYPASS_TUNING_IPG				0x189
#define QCE1204_PCS_MMD1_QUSGMII_RESET					0x18c
#define QCE1204_PCS_MMD1_BYPASS_TUNING_IPG_EN				0x0fff
#define QCE1204_PCS_MMD1_XPCS_MODE					0x1000
#define QCE1204_PCS_MMD1_CALIBRATION_DONE				0x80
#define QCE1204_PCS_MMD1_SSCG_ENABLE					0x8
#define QCE1204_PCS_MMD1_PLL_POWER_ON_AND_RESET				0x1e0
#define QCE1204_PCS_MMD1_ANA_SOFT_RESET_MASK				0x40
#define QCE1204_PCS_MMD1_ANA_SOFT_RESET					0
#define QCE1204_PCS_MMD1_ANA_SOFT_RELEASE				0x40
#define QCE1204_PCS_MMD1_SSC_CLK					0x166
#define QCE1204_PCS_MMD1_SSC_CLK_EN					BIT(7)
#define QCE1204_PCS_MMD1_MS_LDO0					0x16d
#define QCE1204_PCS_MMD1_MS_LDO1					0x16e
#define QCE1204_PCS_MMD3_PCS_CTRL2					0x7
#define QCE1204_PCS_MMD3_AN_LP_BASE_ABL2				0x14
#define QCE1204_PCS_MMD3_10GBASE_PCS_STATUS1				0x20
#define QCE1204_PCS_MMD3_DIG_CTRL1					0x8000
#define QCE1204_PCS_MMD3_EEE_MODE_CTRL					0x8006
#define QCE1204_PCS_MMD3_VR_RPCS_TPC					0x8007
#define QCE1204_PCS_MMD3_EEE_TX_TIMER					0x8008
#define QCE1204_PCS_MMD3_EEE_RX_TIMER					0x8009
#define QCE1204_PCS_MMD3_MII_AM_INTERVAL				0x800a
#define QCE1204_PCS_MMD3_EEE_MODE_CTRL1					0x800b
#define QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R				0
#define QCE1204_PCS_MMD3_10GBASE_UP					0x1000
#define QCE1204_PCS_MMD3_QUSGMII_EN					0x200
#define QCE1204_PCS_MMD3_QUSGMII_MODE					0x1400
#define QCE1204_PCS_MMD3_MII_AM_INTERVAL_VAL				0x6018
#define QCE1204_PCS_MMD3_XPCS_SOFT_RESET				0x8000
#define QCE1204_PCS_MMD3_XPCS_EEE_CAP					0x40
#define QCE1204_PCS_MMD3_EEE_RES_REGS					0x100
#define QCE1204_PCS_MMD3_EEE_SIGN_BIT_REGS				0x40
#define QCE1204_PCS_MMD3_EEE_EN						0x3
#define QCE1204_PCS_MMD3_EEE_TSL_REGS					0xa
#define QCE1204_PCS_MMD3_EEE_TLU_REGS					0xc0
#define QCE1204_PCS_MMD3_EEE_TWL_REGS					0x1600
#define QCE1204_PCS_MMD3_EEE_100US_REG_REGS				0xc8
#define QCE1204_PCS_MMD3_EEE_RWR_REG_REGS				0x1c00
#define QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE				0x1
#define QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE				0x100
#define QCE1204_PCS_MMD3_QUSGMII_FIFO_RESET				0x400
#define QCE1204_PCS_MMD_MII_CTRL					0
#define QCE1204_PCS_SPEED_MASK						0x2060
#define QCE1204_PCS_SPEED_10M						0
#define QCE1204_PCS_MMD_MII_DIG_CTRL					0x8000
#define QCE1204_PCS_MMD_MII_AN_INT_MSK					0x8001
#define QCE1204_PCS_MMD_MII_ERR_SEL					0x8002
#define QCE1204_PCS_MMD_MII_XAUI_MODE_CTRL				0x8004
/* bit 0 in MII_AN_INT_MSK (enable) and MII_ERR_SEL (status);
 * qce1204 neither enables the interrupt nor polls/clears the status */
#define QCE1204_PCS_MMD_AN_COMPLETE_INT					0x1
#define QCE1204_PCS_MMD_MII_4BITS_CTRL					0x0
#define QCE1204_PCS_MMD_TX_CONFIG_CTRL					0x8
#define QCE1204_PCS_MMD_MII_AN_ENABLE					0x1000
#define QCE1204_PCS_MMD_MII_AN_RESTART					0x200
#define QCE1204_PCS_MMD_QUSGMII_FIFO_RESET				0x20
#define QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE				0x1
#define QCE1204_PCS_MMD_PHY_MODE_CTRL_EN				0x1
#define QCE1204_PCS_MMD_CH2						26
#define QCE1204_PCS_MMD_CH3						27
#define QCE1204_PCS_MMD_CH4						28

/* Valid clock rates for QCE1204 */
#define QCE1204_CLK_RATE_2P5M						2500000
#define QCE1204_CLK_RATE_25M						25000000
#define QCE1204_CLK_RATE_78P125M					78125000
#define QCE1204_CLK_RATE_104M						104166666
#define QCE1204_CLK_RATE_125M						125000000
#define QCE1204_CLK_RATE_312P5M						312500000

/* SOC SEC_TCSR registers */
#define QCE1204_EPHY_CFG						0x90F018
#define QCE1204_EPHY_LDO_CTRL						GENMASK(9, 8)
#define QCE1204_WORK_MODE_SEL						0x90f030
#define QCE1204_PHY_MODE_MASK						GENMASK(3, 0)
#define QCE1204_PHY_MODE						0xf
#define QCE1204_SWITCH_MODE_MASK					GENMASK(5, 0)
#define QCE1204_SWITCH_MODE						0x10

/**
 * qce1204_validate_clock_rate - Validate if clock rate is supported
 * @rate: Clock rate to validate
 * Returns: 0 if valid, -EINVAL if invalid
 */
static int qce1204_validate_clock_rate(unsigned long rate)
{
	switch (rate) {
	case QCE1204_CLK_RATE_2P5M:
	case QCE1204_CLK_RATE_25M:
	case QCE1204_CLK_RATE_78P125M:
	case QCE1204_CLK_RATE_104M:
	case QCE1204_CLK_RATE_125M:
	case QCE1204_CLK_RATE_312P5M:
		return 0;
	default:
		pr_err("Invalid clock rate: %lu Hz\n", rate);
		return -EINVAL;
	}
}

static int qce1204_soc_addr_get(struct phy_device *phydev)
{
	if (!phydev || !phydev->shared)
		return PHY_MAX_ADDR;

	return phydev->shared->addr + QCE1204_SOC_ADDR_OFFSET;
}

static void qce1204_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid,
	u16 *reg_high)
{
	*reg_low = (regaddr & 0xc) << 1;

	*reg_mid = regaddr >> 4 & 0xffff;

	*reg_high = ((regaddr >> 20 & 0xf) << 1) | BIT(0);
}

u32 __qce1204_soc_read(struct phy_device *phydev, u32 reg)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;

	addr = qce1204_soc_addr_get(phydev);
	if (addr >= PHY_MAX_ADDR)
		return 0xFFFFFFFF;

	qce1204_split_addr(reg, &reg_low, &reg_mid, &reg_high);
	/* write ahb address bit4~bit23 */
	__mdiobus_write(phydev->mdio.bus, addr, reg_high & 0x1f, reg_mid);
	udelay(100);
	/* write ahb address bit0~bit3 and read low 16bit data */
	lo = __mdiobus_read(phydev->mdio.bus, addr, reg_low);
	/* write ahb address bit0~bit3 and read high 16 bit data */
	hi = __mdiobus_read(phydev->mdio.bus, addr, (reg_low + 4));

	return (hi << 16) | lo;
}

void __qce1204_soc_write(struct phy_device *phydev, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;

	addr = qce1204_soc_addr_get(phydev);
	if (addr >= PHY_MAX_ADDR)
		return;

	qce1204_split_addr(reg, &reg_low, &reg_mid, &reg_high);
	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	/* write ahb address bit4~bit23 */
	__mdiobus_write(phydev->mdio.bus, addr, reg_high & 0x1f, reg_mid);
	udelay(100);
	/* write ahb address bit0~bit3 and write low 16 bit data */
	__mdiobus_write(phydev->mdio.bus, addr, reg_low, lo);
	/* write ahb address bit0~bit3 and write high 16 bit data */
	__mdiobus_write(phydev->mdio.bus, addr, (reg_low + 4), hi);
}

int __qce1204_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set)
{
	u32 val;

	val = __qce1204_soc_read(phydev, reg);
	val = (val & ~mask) | set;
	__qce1204_soc_write(phydev, reg, val);

	return 0;
}

u32 qce1204_soc_read(struct phy_device *phydev, u32 reg)
{
	u32 val;

	phy_lock_mdio_bus(phydev);
	val = __qce1204_soc_read(phydev, reg);
	phy_unlock_mdio_bus(phydev);

	return val;
}

int qce1204_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set)
{
	phy_lock_mdio_bus(phydev);
	__qce1204_soc_modify(phydev, reg, mask, set);
	phy_unlock_mdio_bus(phydev);

	return 0;
}
EXPORT_SYMBOL_GPL(qce1204_soc_modify);

static int qce1204_pcs_addr_get(struct phy_device *phydev, int offset)
{
	if (phydev->shared)
		return phydev->shared->addr + offset;

	return PHY_MAX_ADDR;
}

static int qce1204_pcs_read_mmd(struct phy_device *phydev, int devad, u32 regnum)
{
	int addr = qce1204_pcs_addr_get(phydev, PCS1_ADDR_OFFSET);

	if (addr >= PHY_MAX_ADDR)
		return -EINVAL;

	return mdiobus_c45_read(phydev->mdio.bus, addr, devad, regnum);
}

static int qce1204_pcs_write_mmd(struct phy_device *phydev, int devad, u32 regnum, u16 val)
{
	int addr = qce1204_pcs_addr_get(phydev, PCS1_ADDR_OFFSET);

	if (addr >= PHY_MAX_ADDR)
		return -EINVAL;

	return mdiobus_c45_write(phydev->mdio.bus, addr, devad, regnum, val);
}

static int qce1204_pcs_modify_mmd(struct phy_device *phydev,
	int devad, u32 regnum, u16 mask, u16 set)
{
	int addr = qce1204_pcs_addr_get(phydev, PCS1_ADDR_OFFSET);

	if (addr >= PHY_MAX_ADDR)
		return -EINVAL;

	return mdiobus_c45_modify(phydev->mdio.bus, addr, devad, regnum,
		mask, set);
}

static int qce1204_pcs_mmd_get(struct phy_device *phydev, int channel)
{
	switch(channel) {
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
	u32 channel, u32 regnum, u16 mask, u16 set)
{
	int mmd_id = qce1204_pcs_mmd_get(phydev, channel);

	if (mmd_id < 0)
		return -EOPNOTSUPP;

	return qce1204_pcs_modify_mmd(phydev, mmd_id, regnum, mask, set);
}

static int qce1204_pcs_10g_linkup(struct phy_device *phydev)
{
	u16 xpcs_data = 0;
	u32 retries = 100, linkup = 0;

	/* wait 10G_R link up */
	while (linkup != QCE1204_PCS_MMD3_10GBASE_UP) {
		mdelay(1);
		if (retries-- == 0) {
			phydev_err (phydev, "10g_r link up timeout\n");
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
	int ret = 0;
	u16 pcs_data = 0;
	u32 retries = 100, reset_done = QCE1204_PCS_MMD3_XPCS_SOFT_RESET;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_DIG_CTRL1, 0x8000, QCE1204_PCS_MMD3_XPCS_SOFT_RESET);
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
		QCE1204_PCS_MMD3_EEE_MODE_CTRL, 0x0f40, QCE1204_PCS_MMD3_EEE_RES_REGS |
		QCE1204_PCS_MMD3_EEE_SIGN_BIT_REGS);

	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_EEE_TX_TIMER, 0x1fff, QCE1204_PCS_MMD3_EEE_TSL_REGS|
		QCE1204_PCS_MMD3_EEE_TLU_REGS | QCE1204_PCS_MMD3_EEE_TWL_REGS);

	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_EEE_RX_TIMER, 0x1fff, QCE1204_PCS_MMD3_EEE_100US_REG_REGS|
		QCE1204_PCS_MMD3_EEE_RWR_REG_REGS);

	/* enable TRN_LPI */
	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_EEE_MODE_CTRL1, 0x101, QCE1204_PCS_MMD3_EEE_TRANS_LPI_MODE|
		QCE1204_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);

	/* enable TX/RX LPI pattern */
	qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_EEE_MODE_CTRL, 0x3, QCE1204_PCS_MMD3_EEE_EN);

	return 0;
}

static int qce1204_pcs_calibration(struct phy_device *phydev)
{
	int pcs_data, ret;

	/* wait calibration done to uniphy */
	ret = read_poll_timeout(qce1204_pcs_read_mmd,
		pcs_data, (pcs_data & QCE1204_PCS_MMD1_CALIBRATION_DONE),
		1000, 100000, true, phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_CALIBRATION4);
	if (ret < 0)
		phydev_err(phydev, "pcs calibration time out!\n");

	return ret;
}

static int qce1204_pcs_ana_assert(struct phy_device *phydev,
	bool assert)
{
	return qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_PLL_POWER_ON_AND_RESET,
		QCE1204_PCS_MMD1_ANA_SOFT_RESET_MASK,
		assert ? QCE1204_PCS_MMD1_ANA_SOFT_RESET :
		QCE1204_PCS_MMD1_ANA_SOFT_RELEASE);
}

/*
 * Count the number of PHY ports configured in DTS for this package.
 * Counts available child nodes of the package node whose "reg" value falls
 * within the valid address range [shared->addr, shared->addr + QCE1204_PORT_NUM).
 * The "reg" property of each child uses the same MDIO address space as
 * shared->addr, so a direct numeric comparison is valid.
 * Falls back to QCE1204_PORT_NUM if the package node is unavailable, or if the
 * count is zero or exceeds QCE1204_PORT_NUM.  Note that a DTS repeating a "reg"
 * value within range is not detected here: it inflates the count above the
 * number of ports that can ever set their port_active bit, which leaves
 * probe_complete unset and so keeps the PCS de-asserted (link stays up, the
 * hibernation optimisation is simply disabled).
 */
static u8 qce1204_count_package_ports(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	u8 count = 0;
	u32 reg;

	if (!shared || !shared->np)
		return QCE1204_PORT_NUM;

	for_each_available_child_of_node_scoped(shared->np, child) {
		if (!of_property_read_u32(child, "reg", &reg) &&
		    reg >= shared->addr &&
		    reg < shared->addr + QCE1204_PORT_NUM)
			count++;
	}

	if (!count || count > QCE1204_PORT_NUM)
		return QCE1204_PORT_NUM;

	return count;
}

/*
 * Check hibernation state across all active ports and assert/de-assert the
 * shared PCS ANA soft-reset accordingly.  Assert only when every active port
 * is hibernating; de-assert as soon as any active port is not hibernating.
 * Active ports are those that have completed probe (tracked in port_active).
 * Assertion is suppressed until probe_complete is set, i.e. until all ports
 * configured in DTS have finished probe and joined port_active.
 */
static int qce1204_pcs_check_hibernate(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct qce1204_shared_priv *shared_priv;
	bool hibernation, all_hibernate;
	int port_idx, ret, i;

	if (!shared)
		return -EINVAL;

	shared_priv = (struct qce1204_shared_priv *)shared->priv;
	if (!shared_priv)
		return -EINVAL;

	port_idx = phydev->mdio.addr - shared->addr;
	if (port_idx < 0 || port_idx >= QCE1204_PORT_NUM)
		return -EINVAL;

	ret = qca81xx_phy_hibernation_get(phydev, &hibernation);
	if (ret < 0)
		return ret;

	mutex_lock(&shared_priv->pcs_lock);
	shared_priv->port_hibernation[port_idx] = hibernation;

	all_hibernate = true;
	for (i = 0; i < QCE1204_PORT_NUM; i++) {
		if (!(shared_priv->port_active & BIT(i)))
			continue;
		if (!shared_priv->port_hibernation[i]) {
			all_hibernate = false;
			break;
		}
	}

	/*
	 * Do not assert until all configured ports have completed probe.  Warn
	 * once if that is what suppressed an otherwise valid assertion: a
	 * sibling port whose probe failed never sets its port_active bit, so
	 * probe_complete stays clear and the power saving is silently lost.
	 */
	if (all_hibernate && !shared_priv->probe_complete) {
		all_hibernate = false;
		if (!shared_priv->incomplete_warned) {
			shared_priv->incomplete_warned = true;
			phydev_warn(phydev,
				    "only %u of %u DTS ports probed (active 0x%x); PCS power saving disabled\n",
				    hweight8(shared_priv->port_active),
				    shared_priv->num_active_ports,
				    shared_priv->port_active);
		}
	}

	if (all_hibernate == shared_priv->pcs_assert) {
		mutex_unlock(&shared_priv->pcs_lock);
		return 0;
	}

	ret = qce1204_pcs_ana_assert(phydev, all_hibernate);
	if (!ret)
		shared_priv->pcs_assert = all_hibernate;
	mutex_unlock(&shared_priv->pcs_lock);

	/*
	 * After de-asserting PCS ANA soft-reset, wait for PLL to re-lock.
	 * Done outside the lock to avoid blocking other ports' read_status
	 * for up to 100 ms.
	 */
	if (!ret && !all_hibernate)
		ret = qce1204_pcs_calibration(phydev);

	return ret;
}

/**
 * qce1204_get_clk_data - Get clock data from PHY private structure
 * @phydev: PHY device
 * Returns: Pointer to clock data structure, or NULL if not available
 */
static struct qce1204_clk_data *qce1204_get_clk_data(struct phy_device *phydev)
{
	struct qce1204_priv *priv = (struct qce1204_priv *)phydev->priv;

	if (!priv)
		return NULL;

	return &priv->clk_data;
}

/**
 * qce1204_get_shared_clk_data - Get shared clock data
 * @phydev: PHY device
 * Returns: Pointer to shared clock data structure, or NULL if not available
 */
static struct qce1204_shared_clk_data *qce1204_get_shared_clk_data(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct qce1204_shared_priv *priv;

	if (!shared)
		return NULL;

	priv = (struct qce1204_shared_priv *)shared->priv;
	if (!priv)
		return NULL;

	return &priv->shared_clk_data;
}

/**
 * qce1204_get_package_mode - Get package mode from shared private data
 * @phydev: PHY device
 * Returns: Package mode (phy_interface_t), or PHY_INTERFACE_MODE_NA if not available
 */
static phy_interface_t qce1204_get_package_mode(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct qce1204_shared_priv *priv;

	if (!shared)
		return PHY_INTERFACE_MODE_NA;

	priv = (struct qce1204_shared_priv *)shared->priv;
	if (!priv)
		return PHY_INTERFACE_MODE_NA;

	return priv->package_mode;
}

/**
 * qce1204_clk_get - Get a single clock resource
 */
static int qce1204_clk_get(struct phy_device *phydev, struct device *dev,
			   struct clk **clk_ptr, const char *name,
			   const char *desc)
{
	*clk_ptr = devm_clk_get_optional(dev, name);
	if (IS_ERR(*clk_ptr)) {
		phydev_err(phydev, "Failed to get %s: %ld\n", desc, PTR_ERR(*clk_ptr));
		return PTR_ERR(*clk_ptr);
	}
	if (*clk_ptr)
		phydev_dbg(phydev, "Got %s\n", desc);
	return 0;
}

/**
 * qce1204_reset_get - Get a single reset control resource
 */
static int qce1204_reset_get(struct phy_device *phydev, struct device *dev,
			     struct reset_control **reset_ptr, const char *name,
			     const char *desc)
{
	*reset_ptr = devm_reset_control_get_optional(dev, name);
	if (IS_ERR(*reset_ptr)) {
		phydev_err(phydev, "Failed to get %s: %ld\n", desc, PTR_ERR(*reset_ptr));
		return PTR_ERR(*reset_ptr);
	}
	if (*reset_ptr)
		phydev_dbg(phydev, "Got %s\n", desc);
	return 0;
}

/**
 * qce1204_phy_tx_clk_set - Enable/disable PHY TX clock
 */
static int qce1204_phy_tx_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_clk_data *clk_data;
	int ret = 0;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Clock is optional, return success if not present */
	if (!clk_data->tx_clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk_data->tx_clk);
		if (ret < 0)
			phydev_err(phydev, "Failed to enable PHY TX clock: %d\n", ret);
	} else {
		clk_disable_unprepare(clk_data->tx_clk);
	}

	return ret;
}

/**
 * qce1204_phy_rx_clk_set - Enable/disable PHY RX clock
 */
static int qce1204_phy_rx_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_clk_data *clk_data;
	int ret = 0;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Clock is optional, return success if not present */
	if (!clk_data->rx_clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk_data->rx_clk);
		if (ret < 0)
			phydev_err(phydev, "Failed to enable PHY RX clock: %d\n", ret);
	} else {
		clk_disable_unprepare(clk_data->rx_clk);
	}

	return ret;
}

static int qce1204_phy_clk_parent_init(struct phy_device *phydev)
{
	struct qce1204_shared_clk_data *clk_shared_data;
	struct qce1204_clk_data *clk_data;
	int ret;

	clk_shared_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_shared_data)
		return -EINVAL;

	if (!clk_shared_data->tx_parent || !clk_shared_data->rx_parent)
		return 0;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	if (!clk_data->tx_src_clk || !clk_data->rx_src_clk)
		return 0;

	ret = clk_set_parent(clk_data->tx_src_clk, clk_shared_data->tx_parent);
	if (ret)
		return ret;

	return clk_set_parent(clk_data->rx_src_clk, clk_shared_data->rx_parent);
}

/**
 * qce1204_phy_sys_clk_set - Enable/disable PHY SYS clock
 * @phydev: PHY device
 * @enable: true to enable clock, false to disable
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_sys_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_clk_data *clk_data;
	int ret = 0;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Clock is optional, return success if not present */
	if (!clk_data->sys_clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk_data->sys_clk);
		if (ret < 0)
			phydev_err(phydev, "Failed to enable PHY SYS clock: %d\n", ret);
	} else {
		clk_disable_unprepare(clk_data->sys_clk);
	}

	return ret;
}

/**
 * qce1204_phy_clk_set - Enable/disable clocks for this PHY
 * Only enables clocks that are defined in DTS
 */
static int qce1204_phy_clk_set(struct phy_device *phydev, bool enable)
{
	int ret;

	ret = qce1204_phy_tx_clk_set(phydev, enable);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_rx_clk_set(phydev, enable);

	return ret;
}

/**
 * qce1204_phy_tx_reset_assert - Assert/deassert PHY TX reset
 * @phydev: PHY device
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_tx_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Reset is optional, return success if not present */
	if (!clk_data->tx_reset)
		return 0;

	if (assert)
		ret = reset_control_assert(clk_data->tx_reset);
	else
		ret = reset_control_deassert(clk_data->tx_reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s TX reset: %d\n",
			   assert ? "assert" : "deassert", ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_phy_rx_reset_assert - Assert/deassert PHY RX reset
 * @phydev: PHY device
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_rx_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Reset is optional, return success if not present */
	if (!clk_data->rx_reset)
		return 0;

	if (assert)
		ret = reset_control_assert(clk_data->rx_reset);
	else
		ret = reset_control_deassert(clk_data->rx_reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s RX reset: %d\n",
			   assert ? "assert" : "deassert", ret);
		return ret;
	}

	return 0;
}


/**
 * qce1204_phy_clk_reset_assert - Assert/deassert all PHY clock resets
 * @phydev: PHY device
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 *
 * This function controls all PHY clock-related resets:
 * - TX reset
 * - RX reset
 */
static int qce1204_phy_clk_reset_assert(struct phy_device *phydev, bool assert)
{
	int ret;

	/* Control TX reset */
	ret = qce1204_phy_tx_reset_assert(phydev, assert);
	if (ret < 0)
		return ret;

	/* Control RX reset */
	ret = qce1204_phy_rx_reset_assert(phydev, assert);
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
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * qce1204_phy_sys_reset_assert - Assert/deassert PHY SYS reset
 * @phydev: PHY device
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_sys_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Reset is optional, return success if not present */
	if (!clk_data->sys_reset)
		return 0;

	if (assert)
		ret = reset_control_assert(clk_data->sys_reset);
	else
		ret = reset_control_deassert(clk_data->sys_reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s SYS reset: %d\n",
			   assert ? "assert" : "deassert", ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_phy_sys_reset - Reset PHY SYS (assert then deassert)
 * @phydev: PHY device
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_sys_reset(struct phy_device *phydev)
{
	int ret;

	ret = qce1204_phy_sys_reset_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(10);
	ret = qce1204_phy_sys_reset_assert(phydev, false);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * qce1204_clk_set_rate - Set clock rate based on link speed
 * Only sets rate for clocks that are defined in DTS
 */
static int qce1204_pcs_clk_set_rate(struct phy_device *phydev, u32 channel,
	unsigned long gmii_clk_rate, unsigned long xgmii_clk_rate)
{
	struct qce1204_shared_clk_data *clk_data;
	struct qce1204_channel_clk *ch_clk;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	/* Validate clock rates */
	ret = qce1204_validate_clock_rate(gmii_clk_rate);
	if (ret < 0) {
		phydev_err(phydev, "Invalid GMII clock rate: %lu Hz\n", gmii_clk_rate);
		return ret;
	}

	ret = qce1204_validate_clock_rate(xgmii_clk_rate);
	if (ret < 0) {
		phydev_err(phydev, "Invalid XGMII clock rate: %lu Hz\n", xgmii_clk_rate);
		return ret;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	ch_clk = &clk_data->channels[channel - 1];

	/* Set GMII TX clock rate */
	if (ch_clk->clks[QCE1204_CLK_GMII_TX]) {
		ret = clk_set_rate(ch_clk->clks[QCE1204_CLK_GMII_TX], gmii_clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set GMII TX clock rate: %d\n", ret);
			return ret;
		}
	}

	/* Set GMII RX clock rate */
	if (ch_clk->clks[QCE1204_CLK_GMII_RX]) {
		ret = clk_set_rate(ch_clk->clks[QCE1204_CLK_GMII_RX], gmii_clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set GMII RX clock rate: %d\n", ret);
			return ret;
		}
	}

	/* Set XGMII TX clock rate */
	if (ch_clk->clks[QCE1204_CLK_XGMII_TX]) {
		ret = clk_set_rate(ch_clk->clks[QCE1204_CLK_XGMII_TX], xgmii_clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set XGMII TX clock rate: %d\n", ret);
			return ret;
		}
	}

	/* Set XGMII RX clock rate */
	if (ch_clk->clks[QCE1204_CLK_XGMII_RX]) {
		ret = clk_set_rate(ch_clk->clks[QCE1204_CLK_XGMII_RX], xgmii_clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set XGMII RX clock rate: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

/**
 * qce1204_xpcs_reset_assert - Assert/deassert XPCS reset
 * @phydev: PHY device
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_xpcs_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Reset is optional, return success if not present */
	if (!clk_data->xpcs_reset)
		return 0;

	if (assert)
		ret = reset_control_assert(clk_data->xpcs_reset);
	else
		ret = reset_control_deassert(clk_data->xpcs_reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s XPCS reset: %d\n",
			   assert ? "assert" : "deassert", ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_gmii_tx_reset_assert - Assert/deassert GMII TX reset for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_gmii_tx_reset_assert(struct phy_device *phydev, u32 channel, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	struct reset_control *reset;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	reset = clk_data->channels[channel - 1].resets[QCE1204_CLK_GMII_TX];

	/* Reset is optional, return success if not present */
	if (!reset)
		return 0;

	if (assert)
		ret = reset_control_assert(reset);
	else
		ret = reset_control_deassert(reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s CH%d GMII TX reset: %d\n",
			   assert ? "assert" : "deassert", channel, ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_gmii_rx_reset_assert - Assert/deassert GMII RX reset for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_gmii_rx_reset_assert(struct phy_device *phydev, u32 channel, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	struct reset_control *reset;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	reset = clk_data->channels[channel - 1].resets[QCE1204_CLK_GMII_RX];

	/* Reset is optional, return success if not present */
	if (!reset)
		return 0;

	if (assert)
		ret = reset_control_assert(reset);
	else
		ret = reset_control_deassert(reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s CH%d GMII RX reset: %d\n",
			   assert ? "assert" : "deassert", channel, ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_xgmii_tx_reset_assert - Assert/deassert XGMII TX reset for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_xgmii_tx_reset_assert(struct phy_device *phydev, u32 channel, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	struct reset_control *reset;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	reset = clk_data->channels[channel - 1].resets[QCE1204_CLK_XGMII_TX];

	/* Reset is optional, return success if not present */
	if (!reset)
		return 0;

	if (assert)
		ret = reset_control_assert(reset);
	else
		ret = reset_control_deassert(reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s CH%d XGMII TX reset: %d\n",
			   assert ? "assert" : "deassert", channel, ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_xgmii_rx_reset_assert - Assert/deassert XGMII RX reset for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_xgmii_rx_reset_assert(struct phy_device *phydev, u32 channel, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	struct reset_control *reset;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	reset = clk_data->channels[channel - 1].resets[QCE1204_CLK_XGMII_RX];

	/* Reset is optional, return success if not present */
	if (!reset)
		return 0;

	if (assert)
		ret = reset_control_assert(reset);
	else
		ret = reset_control_deassert(reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s CH%d XGMII RX reset: %d\n",
			   assert ? "assert" : "deassert", channel, ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_clk_assert - Assert/deassert PCS resets for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 *
 * This function controls all PCS-related resets for a specific channel:
 * - GMII TX reset
 * - GMII RX reset
 * - XGMII TX reset
 * - XGMII RX reset
 */
static int qce1204_pcs_clk_reset_assert(struct phy_device *phydev, u32 channel, bool assert)
{
	int ret;

	/* Control GMII TX reset */
	ret = qce1204_pcs_gmii_tx_reset_assert(phydev, channel, assert);
	if (ret < 0)
		return ret;

	/* Control GMII RX reset */
	ret = qce1204_pcs_gmii_rx_reset_assert(phydev, channel, assert);
	if (ret < 0)
		return ret;

	/* Control XGMII TX reset */
	ret = qce1204_pcs_xgmii_tx_reset_assert(phydev, channel, assert);
	if (ret < 0)
		return ret;

	/* Control XGMII RX reset */
	ret = qce1204_pcs_xgmii_rx_reset_assert(phydev, channel, assert);
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

/**
 * qce1204_pcs_gmii_tx_clk_set - Enable/disable GMII TX clock for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @enable: true to enable clock, false to disable
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_gmii_tx_clk_set(struct phy_device *phydev, u32 channel, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;
	struct clk *clk;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	clk = clk_data->channels[channel - 1].clks[QCE1204_CLK_GMII_TX];

	/* Clock is optional, return success if not present */
	if (!clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			phydev_err(phydev, "Failed to enable CH%d GMII TX clock: %d\n",
				   channel, ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(clk);
	}

	return 0;
}

/**
 * qce1204_pcs_gmii_rx_clk_set - Enable/disable GMII RX clock for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @enable: true to enable clock, false to disable
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_gmii_rx_clk_set(struct phy_device *phydev, u32 channel, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;
	struct clk *clk;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	clk = clk_data->channels[channel - 1].clks[QCE1204_CLK_GMII_RX];

	/* Clock is optional, return success if not present */
	if (!clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			phydev_err(phydev, "Failed to enable CH%d GMII RX clock: %d\n",
				   channel, ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(clk);
	}

	return 0;
}

/**
 * qce1204_pcs_xgmii_tx_clk_set - Enable/disable XGMII TX clock for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @enable: true to enable clock, false to disable
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_xgmii_tx_clk_set(struct phy_device *phydev, u32 channel, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;
	struct clk *clk;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	clk = clk_data->channels[channel - 1].clks[QCE1204_CLK_XGMII_TX];

	/* Clock is optional, return success if not present */
	if (!clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			phydev_err(phydev, "Failed to enable CH%d XGMII TX clock: %d\n",
				   channel, ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(clk);
	}

	return 0;
}

/**
 * qce1204_pcs_xgmii_rx_clk_set - Enable/disable XGMII RX clock for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @enable: true to enable clock, false to disable
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_xgmii_rx_clk_set(struct phy_device *phydev, u32 channel, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;
	struct clk *clk;
	int ret;

	/* Validate channel range */
	if (channel < 1 || channel > 4) {
		phydev_err(phydev, "Invalid channel: %d (must be 1-4)\n", channel);
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	clk = clk_data->channels[channel - 1].clks[QCE1204_CLK_XGMII_RX];

	/* Clock is optional, return success if not present */
	if (!clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			phydev_err(phydev, "Failed to enable CH%d XGMII RX clock: %d\n",
				   channel, ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(clk);
	}

	return 0;
}

/**
 * qce1204_pcs_clk_set - Enable/disable PCS clocks for a channel
 * @phydev: PHY device
 * @channel: Channel number (0-3)
 * @enable: true to enable clocks, false to disable
 * Returns: 0 on success, negative error code on failure
 *
 * This function controls all PCS-related clocks for a specific channel:
 * - GMII TX clock
 * - GMII RX clock
 * - XGMII TX clock
 * - XGMII RX clock
 */
static int qce1204_pcs_clk_set(struct phy_device *phydev, u32 channel, bool enable)
{
	int ret;

	/* Control GMII TX clock */
	ret = qce1204_pcs_gmii_tx_clk_set(phydev, channel, enable);
	if (ret < 0)
		return ret;

	/* Control GMII RX clock */
	ret = qce1204_pcs_gmii_rx_clk_set(phydev, channel, enable);
	if (ret < 0)
		return ret;

	/* Control XGMII TX clock */
	ret = qce1204_pcs_xgmii_tx_clk_set(phydev, channel, enable);
	if (ret < 0)
		return ret;

	/* Control XGMII RX clock */
	ret = qce1204_pcs_xgmii_rx_clk_set(phydev, channel, enable);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * qce1204_ahb_clk_set_rate - Set AHB clock rate
 * @phydev: PHY device
 * @rate: Clock rate in Hz
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_ahb_clk_set_rate(struct phy_device *phydev, unsigned long rate)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Clock is optional, return success if not present */
	if (!clk_data->ahb_clk)
		return 0;

	/* Validate clock rate */
	ret = qce1204_validate_clock_rate(rate);
	if (ret < 0) {
		phydev_err(phydev, "Invalid AHB clock rate: %lu Hz\n", rate);
		return ret;
	}

	ret = clk_set_rate(clk_data->ahb_clk, rate);
	if (ret < 0) {
		phydev_err(phydev, "Failed to set AHB clock rate to %lu Hz: %d\n", rate, ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_sys_clk_set - Enable/disable PCS SYS clock
 * @phydev: PHY device
 * @enable: true to enable clock, false to disable
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_sys_clk_set(struct phy_device *phydev, bool enable)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret = 0;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Clock is optional, return success if not present */
	if (!clk_data->pcs_sys_clk)
		return 0;

	if (enable) {
		ret = clk_prepare_enable(clk_data->pcs_sys_clk);
		if (ret < 0)
			phydev_err(phydev, "Failed to enable PCS SYS clock: %d\n", ret);
	} else {
		clk_disable_unprepare(clk_data->pcs_sys_clk);
	}

	return ret;
}

/**
 * qce1204_pcs_sys_reset_assert - Assert/deassert PCS SYS reset
 * @phydev: PHY device
 * @assert: true to assert reset, false to deassert
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_sys_reset_assert(struct phy_device *phydev, bool assert)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Reset is optional, return success if not present */
	if (!clk_data->pcs_sys_reset)
		return 0;

	if (assert)
		ret = reset_control_assert(clk_data->pcs_sys_reset);
	else
		ret = reset_control_deassert(clk_data->pcs_sys_reset);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s PCS SYS reset: %d\n",
			   assert ? "assert" : "deassert", ret);
		return ret;
	}

	return 0;
}

/**
 * qce1204_pcs_sys_reset - Reset PCS SYS (assert then deassert)
 * @phydev: PHY device
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_sys_reset(struct phy_device *phydev)
{
	int ret;

	ret = qce1204_pcs_sys_reset_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(10);
	ret = qce1204_pcs_sys_reset_assert(phydev, false);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * qce1204_pcs_sys_clk_set_rate - Set PCS system clock rate
 * @phydev: PHY device
 * @rate: Clock rate in Hz
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_pcs_sys_clk_set_rate(struct phy_device *phydev, unsigned long rate)
{
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;

	/* Clock is optional, return success if not present */
	if (!clk_data->pcs_sys_clk)
		return 0;

	/* Validate clock rate */
	ret = qce1204_validate_clock_rate(rate);
	if (ret < 0) {
		phydev_err(phydev, "Invalid PCS system clock rate: %lu Hz\n", rate);
		return ret;
	}

	ret = clk_set_rate(clk_data->pcs_sys_clk, rate);
	if (ret < 0) {
		phydev_err(phydev, "Failed to set pcs system clock rate to %lu Hz: %d\n", rate, ret);
		return ret;
	}

	return 0;
}

static int _qce1204_pcs_qusgmii_mode_set(struct phy_device *phydev)
{
	int ret = 0, channel = 0;

	/* Uniphy MSLDO settings */
	ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_MS_LDO0, 0xcd);
	if (ret < 0)
		return ret;
	ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_MS_LDO1, 0x7f6d);
	if (ret < 0)
		return ret;
	/* assert xpcs */
	ret = qce1204_xpcs_reset_assert(phydev, true);
	if (ret < 0)
		return ret;

	/* select xpcs mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_MODE_CTRL, 0x1f00, QCE1204_PCS_MMD1_XPCS_MODE);
	if (ret < 0)
		return ret;
	/* reset and release PCS GMII XGMII */
	for(channel = 1; channel <= 4; channel++) {
		ret = qce1204_pcs_clk_reset(phydev, channel);
		if (ret < 0)
			return ret;
	}
	/* ana sw reset and release */
	ret = qce1204_pcs_ana_assert(phydev, true);
	if (ret < 0)
		return ret;
	ret = qce1204_pcs_ana_assert(phydev, false);
	if (ret < 0)
		return ret;
	/* Wait calibration done */
	qce1204_pcs_calibration(phydev);
	/* open ssc clock */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_SSC_CLK, QCE1204_PCS_MMD1_SSC_CLK_EN,
		QCE1204_PCS_MMD1_SSC_CLK_EN);
	if (ret < 0)
		return ret;
	/* Enable SSCG(Spread Spectrum Clock Generator) */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_CDA_CONTROL1, 0x8, QCE1204_PCS_MMD1_SSCG_ENABLE);
	if (ret < 0)
		return ret;
	/* de-assert XPCS */
	ret = qce1204_xpcs_reset_assert(phydev, false);
	if (ret < 0)
		return ret;
	/* Set BaseR mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_PCS_CTRL2, 0xf, QCE1204_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret < 0)
		return ret;
	/* wait PCS 10G link up */
	ret = qce1204_pcs_10g_linkup(phydev);
	if (ret < 0)
		return ret;
	/* enable QUSGMII mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_DIG_CTRL1, 0x200, QCE1204_PCS_MMD3_QUSGMII_EN);
	if (ret < 0)
		return ret;
	/* set QUSGMII mode */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_VR_RPCS_TPC, 0x1c00, QCE1204_PCS_MMD3_QUSGMII_MODE);
	if (ret < 0)
		return ret;
	/* set xpcs speed as 10M */
	for (channel = 1; channel <= 4; channel++) {
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel, QCE1204_PCS_MMD_MII_CTRL,
			QCE1204_PCS_SPEED_MASK, QCE1204_PCS_SPEED_10M);
		if (ret < 0)
			return ret;
	}
	/* set AM interval */
	ret = qce1204_pcs_write_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PCS_MMD3_MII_AM_INTERVAL, QCE1204_PCS_MMD3_MII_AM_INTERVAL_VAL);
	if (ret < 0)
		return ret;
	/* xpcs software reset */
	ret = qce1204_pcs_soft_reset(phydev);

	return ret;
}

static int qce1204_pcs_qusgmii_mode_set(struct phy_device *phydev)
{
	int ret = 0;
	u32 channel = 0;

	/* disable IPG_tuning bypass */
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_BYPASS_TUNING_IPG,
		QCE1204_PCS_MMD1_BYPASS_TUNING_IPG_EN, 0);
	if (ret < 0)
		return ret;
	/* configure qusgmii mode */
	ret = _qce1204_pcs_qusgmii_mode_set(phydev);
	if (ret < 0)
		return ret;
	/*
	* disable auto-neg complete interrupt, enable Mii using mii-4bits,
	* configure as PHY mode, enable autoneg ability
	*/
	for (channel = 1; channel <= 4; channel++)
	{
		/*
		* disable auto-neg complete interrupt, enable Mii using mii-4bits,
		* configure as PHY mode
		*/
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
			QCE1204_PCS_MMD_MII_AN_INT_MSK, 0x109,
			QCE1204_PCS_MMD_MII_4BITS_CTRL |
			QCE1204_PCS_MMD_TX_CONFIG_CTRL);
		if (ret < 0)
			return ret;

		/* enable autoneg ability */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
			QCE1204_PCS_MMD_MII_CTRL, QCE1204_PCS_MMD_MII_AN_ENABLE,
			QCE1204_PCS_MMD_MII_AN_ENABLE);
		if (ret < 0)
			return ret;
		/* disable TICD */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
			QCE1204_PCS_MMD_MII_XAUI_MODE_CTRL, 0x1,
			QCE1204_PCS_MMD_TX_IPG_CHECK_DISABLE);
		if (ret < 0)
			return ret;
		/* enable PHY mode control to sync phy link information to XPCS */
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
			QCE1204_PCS_MMD_MII_DIG_CTRL, BIT(0),
			QCE1204_PCS_MMD_PHY_MODE_CTRL_EN);
		if (ret < 0)
			return ret;
	}

	/* enable EEE for xpcs */
	ret = qce1204_pcs_8023az_enable(phydev);

	return ret;
}

static int qce1204_pcs_speed_clock_set(struct phy_device *phydev,
	u32 channel, u32 speed)
{
	unsigned long gmii_clk_rate, xgmii_clk_rate;

	/* Determine clock rates based on speed */
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
		phydev_err(phydev, "Unsupported speed: %d\n", speed);
		return -EOPNOTSUPP;
	}

	return qce1204_pcs_clk_set_rate(phydev, channel, gmii_clk_rate, xgmii_clk_rate);
}

static int qce1204_pcs_qusgmii_function_reset(struct phy_device *phydev,
	u32 channel)
{
	int ret = 0;

	if (channel == 1)
		ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
			QCE1204_PCS_MMD_MII_DIG_CTRL,
			0x400, QCE1204_PCS_MMD3_QUSGMII_FIFO_RESET);
	else
		ret = qce1204_pcs_modify_channel_mmd(phydev, channel,
			QCE1204_PCS_MMD_MII_DIG_CTRL,
			QCE1204_PCS_MMD_QUSGMII_FIFO_RESET,
			QCE1204_PCS_MMD_QUSGMII_FIFO_RESET);

	return ret;
}

static int qce1204_pcs_qusgmii_reset(struct phy_device *phydev, u32 channel)
{
	int ret;

	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_QUSGMII_RESET, BIT(channel - 1), 0);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qce1204_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCE1204_PCS_MMD1_QUSGMII_RESET, BIT(channel - 1), BIT(channel - 1));

	return ret;
}

static int qce1204_phy_fifo_reset(struct phy_device *phydev, bool enable)
{
	u16 phy_data = 0;

	if (!enable)
		phy_data |= QCE1204_PHY_FIFO_RESET;

	return phy_modify_mmd(phydev, MDIO_MMD_VEND2,
		QCE1204_PHY_CONTROL,
		QCE1204_PHY_FIFO_RESET, phy_data);
}

int qce1204_phy_soft_reset(struct phy_device *phydev)
{
	return phy_modify_mmd(phydev, MDIO_MMD_VEND2,
		MII_BMCR, BMCR_RESET, BMCR_RESET);
}

static int qce1204_phy_mdix_ctrl_set(struct phy_device *phydev)
{
	int ret;
	u16 val;

	switch (phydev->mdix_ctrl) {
	case ETH_TP_MDI:
		val = QCE1204_PHY_MDI;
		break;
	case ETH_TP_MDI_X:
		val = QCE1204_PHY_MDI_X;
		break;
	case ETH_TP_MDI_AUTO:
		val = QCE1204_PHY_MDI_AUTO;
		break;
	default:
		return 0;
	}
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_VEND2,
		QCE1204_PHY_SPEC_CONTROL, QCE1204_PHY_MDI_MASK,
		FIELD_PREP(QCE1204_PHY_MDI_MASK, val));
	if (ret <= 0)
		return ret;

	return qce1204_phy_soft_reset(phydev);
}

static int qce1204_phy_mdix_ctrl_get(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCE1204_PHY_SPEC_CONTROL);
	if (ret < 0)
		return ret;
	switch (FIELD_GET(QCE1204_PHY_MDI_MASK, ret)) {
	case QCE1204_PHY_MDI:
		phydev->mdix_ctrl = ETH_TP_MDI;
		break;
	case QCE1204_PHY_MDI_X:
		phydev->mdix_ctrl = ETH_TP_MDI_X;
		break;
	case QCE1204_PHY_MDI_AUTO:
		phydev->mdix_ctrl = ETH_TP_MDI_AUTO;
		break;
	}

	return 0;
}

int qce1204_phy_config_aneg(struct phy_device *phydev)
{
	bool changed = false;
	u16 reg = 0;
	int ret = 0;

	if (phydev->autoneg == AUTONEG_DISABLE) {
		int duplex_val = BMCR_FULLDPLX, duplex_tmp = DUPLEX_FULL;
		/*
		* genphy_c45_pma_setup_forced only support duplex full,
		* so need to set duplex as full to configure speed
		* when duplex is half
		*/
		duplex_tmp = phydev->duplex;
		if (phydev->duplex == DUPLEX_HALF) {
			phydev->duplex = DUPLEX_FULL;
			duplex_val = 0;
		}
		ret = genphy_c45_pma_setup_forced(phydev);
		if (ret < 0)
			return ret;
		phydev->duplex = duplex_tmp;
		return phy_modify_mmd(phydev, MDIO_MMD_VEND2, MII_BMCR,
			BMCR_FULLDPLX, duplex_val);
	}

	ret = genphy_c45_an_config_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/*
	* Clause 45 has no standardized support for 1000BaseT,
	* therefore use vendor registers
	*/
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		phydev->advertising))
		reg |= QCE1204_PHY_ADVERTISE_1000FULL;
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_VEND2,
		QCE1204_PHY_1000BASET_CONTROL,
		QCE1204_PHY_ADVERTISE_1000FULL,
		reg);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	ret = qce1204_phy_mdix_ctrl_set(phydev);
	if (ret < 0)
		return ret;

	/* configure master/slave mode */
	ret = qca81xx_phy_master_slave_set(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	return genphy_c45_check_and_restart_aneg(phydev, changed);
}

int qce1204_phy_channel_get(struct phy_device *phydev)
{
	return (phydev->mdio.addr - phydev->shared->addr + 1);
}

/**
 * qce1204_phy_shared_clk_init - Initialize shared clocks and resets
 * @phydev: PHY device
 * @dev: Device structure
 * @clk_data: Shared clock data structure
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_shared_clk_init(struct phy_device *phydev, struct device *dev,
				       struct qce1204_shared_clk_data *clk_data)
{
	struct clk_init_entry clk_table[] = {
		{&clk_data->channels[0].clks[QCE1204_CLK_GMII_TX], "ch0_gmii_tx_clk", "CH0 GMII TX clock"},
		{&clk_data->channels[0].clks[QCE1204_CLK_GMII_RX], "ch0_gmii_rx_clk", "CH0 GMII RX clock"},
		{&clk_data->channels[0].clks[QCE1204_CLK_XGMII_TX], "ch0_xgmii_tx_clk", "CH0 XGMII TX clock"},
		{&clk_data->channels[0].clks[QCE1204_CLK_XGMII_RX], "ch0_xgmii_rx_clk", "CH0 XGMII RX clock"},
		{&clk_data->channels[1].clks[QCE1204_CLK_GMII_TX], "ch1_gmii_tx_clk", "CH1 GMII TX clock"},
		{&clk_data->channels[1].clks[QCE1204_CLK_GMII_RX], "ch1_gmii_rx_clk", "CH1 GMII RX clock"},
		{&clk_data->channels[1].clks[QCE1204_CLK_XGMII_TX], "ch1_xgmii_tx_clk", "CH1 XGMII TX clock"},
		{&clk_data->channels[1].clks[QCE1204_CLK_XGMII_RX], "ch1_xgmii_rx_clk", "CH1 XGMII RX clock"},
		{&clk_data->channels[2].clks[QCE1204_CLK_GMII_TX], "ch2_gmii_tx_clk", "CH2 GMII TX clock"},
		{&clk_data->channels[2].clks[QCE1204_CLK_GMII_RX], "ch2_gmii_rx_clk", "CH2 GMII RX clock"},
		{&clk_data->channels[2].clks[QCE1204_CLK_XGMII_TX], "ch2_xgmii_tx_clk", "CH2 XGMII TX clock"},
		{&clk_data->channels[2].clks[QCE1204_CLK_XGMII_RX], "ch2_xgmii_rx_clk", "CH2 XGMII RX clock"},
		{&clk_data->channels[3].clks[QCE1204_CLK_GMII_TX], "ch3_gmii_tx_clk", "CH3 GMII TX clock"},
		{&clk_data->channels[3].clks[QCE1204_CLK_GMII_RX], "ch3_gmii_rx_clk", "CH3 GMII RX clock"},
		{&clk_data->channels[3].clks[QCE1204_CLK_XGMII_TX], "ch3_xgmii_tx_clk", "CH3 XGMII TX clock"},
		{&clk_data->channels[3].clks[QCE1204_CLK_XGMII_RX], "ch3_xgmii_rx_clk", "CH3 XGMII RX clock"},
		{&clk_data->pcs_sys_clk, "pcs_sys_clk", "PCS SYS clock"},
		{&clk_data->ahb_clk, "ahb_clk", "AHB clock"},
		{&clk_data->tx_parent, "tx_parent", "TX parent clock"},
		{&clk_data->rx_parent, "rx_parent", "RX parent clock"},
	};
	struct reset_init_entry reset_table[] = {
		{&clk_data->channels[0].resets[QCE1204_CLK_GMII_TX], "ch0_gmii_tx_reset", "CH0 GMII TX reset"},
		{&clk_data->channels[0].resets[QCE1204_CLK_GMII_RX], "ch0_gmii_rx_reset", "CH0 GMII RX reset"},
		{&clk_data->channels[0].resets[QCE1204_CLK_XGMII_TX], "ch0_xgmii_tx_reset", "CH0 XGMII TX reset"},
		{&clk_data->channels[0].resets[QCE1204_CLK_XGMII_RX], "ch0_xgmii_rx_reset", "CH0 XGMII RX reset"},
		{&clk_data->channels[1].resets[QCE1204_CLK_GMII_TX], "ch1_gmii_tx_reset", "CH1 GMII TX reset"},
		{&clk_data->channels[1].resets[QCE1204_CLK_GMII_RX], "ch1_gmii_rx_reset", "CH1 GMII RX reset"},
		{&clk_data->channels[1].resets[QCE1204_CLK_XGMII_TX], "ch1_xgmii_tx_reset", "CH1 XGMII TX reset"},
		{&clk_data->channels[1].resets[QCE1204_CLK_XGMII_RX], "ch1_xgmii_rx_reset", "CH1 XGMII RX reset"},
		{&clk_data->channels[2].resets[QCE1204_CLK_GMII_TX], "ch2_gmii_tx_reset", "CH2 GMII TX reset"},
		{&clk_data->channels[2].resets[QCE1204_CLK_GMII_RX], "ch2_gmii_rx_reset", "CH2 GMII RX reset"},
		{&clk_data->channels[2].resets[QCE1204_CLK_XGMII_TX], "ch2_xgmii_tx_reset", "CH2 XGMII TX reset"},
		{&clk_data->channels[2].resets[QCE1204_CLK_XGMII_RX], "ch2_xgmii_rx_reset", "CH2 XGMII RX reset"},
		{&clk_data->channels[3].resets[QCE1204_CLK_GMII_TX], "ch3_gmii_tx_reset", "CH3 GMII TX reset"},
		{&clk_data->channels[3].resets[QCE1204_CLK_GMII_RX], "ch3_gmii_rx_reset", "CH3 GMII RX reset"},
		{&clk_data->channels[3].resets[QCE1204_CLK_XGMII_TX], "ch3_xgmii_tx_reset", "CH3 XGMII TX reset"},
		{&clk_data->channels[3].resets[QCE1204_CLK_XGMII_RX], "ch3_xgmii_rx_reset", "CH3 XGMII RX reset"},
		{&clk_data->pcs_sys_reset, "pcs_sys_reset", "PCS SYS reset"},
		{&clk_data->xpcs_reset, "xpcs", "XPCS reset"},
	};
	int ret, i;

	/* Initialize all clocks */
	for (i = 0; i < ARRAY_SIZE(clk_table); i++) {
		ret = qce1204_clk_get(phydev, dev, clk_table[i].clk_ptr,
				clk_table[i].name, clk_table[i].desc);
		if (ret < 0)
			return ret;
	}

	/* Initialize all reset controls */
	for (i = 0; i < ARRAY_SIZE(reset_table); i++) {
		ret = qce1204_reset_get(phydev, dev, reset_table[i].reset_ptr,
					reset_table[i].name, reset_table[i].desc);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * qce1204_shared_clk_probe - Initialize shared clocks and resets from DTS at package level
 * @phydev: PHY device
 * Returns: 0 on success, negative error code on failure
 *
 * Note: XPCS clock and reset are defined at the package level in DTS and shared
 * by all PHYs in the package. This function should only be called once during
 * package initialization. The shared->priv memory is already allocated by
 * devm_of_phy_package_join().
 */
static int qce1204_shared_clk_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.bus->dev;
	struct device_node *old_of_node;
	struct qce1204_shared_clk_data *clk_data;
	int ret;

	if (!phydev->shared || !phydev->shared->np) {
		phydev_err(phydev, "No package device node found\n");
		return -EINVAL;
	}

	clk_data = qce1204_get_shared_clk_data(phydev);
	if (!clk_data) {
		phydev_err(phydev, "No shared clock data found\n");
		return -EINVAL;
	}

	/* Initialize structure to NULL to ensure consistent state */
	memset(clk_data, 0, sizeof(*clk_data));

	/* Access package information through phydev->shared->np */
	old_of_node = dev->of_node;
	dev->of_node = phydev->shared->np;

	/* Initialize clocks and resets using the correct device node */
	ret = qce1204_phy_shared_clk_init(phydev, dev, clk_data);

	/* Restore original of_node */
	dev->of_node = old_of_node;

	if (ret < 0)
		return ret;

	return 0;
}

/**
 * qce1204_phy_package_mode_probe - Parse package mode from DTS
 * @phydev: PHY device
 * Returns: 0 on success, negative error code on failure
 *
 * This function parses the "qcom,package-mode" property from the device tree
 * and stores it in the shared private data structure. This should only be
 * called once during package initialization.
 */
static int qce1204_phy_package_mode_probe(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct qce1204_shared_priv *shared_priv;
	struct device_node *node;
	const char *mode_str;
	int ret;

	if (!shared || !shared->np) {
		phydev_err(phydev, "No package device node found\n");
		return -EINVAL;
	}

	shared_priv = (struct qce1204_shared_priv *)shared->priv;
	if (!shared_priv) {
		phydev_err(phydev, "No shared private data found\n");
		return -EINVAL;
	}

	node = shared->np;

	/* Parse "qcom,package-mode" property */
	ret = of_property_read_string(node, "qcom,package-mode", &mode_str);
	if (ret) {
		/* Property is optional, default to PHY_INTERFACE_MODE_NA */
		phydev_info(phydev, "No package-mode specified, using default\n");
		shared_priv->package_mode = PHY_INTERFACE_MODE_NA;
		return 0;
	}

	if (strcasecmp(mode_str, "qusgmii") == 0) {
		shared_priv->package_mode = PHY_INTERFACE_MODE_QUSGMII;
	} else if (strcasecmp(mode_str, "internal") == 0) {
		shared_priv->package_mode = PHY_INTERFACE_MODE_INTERNAL;
	} else {
		phydev_err(phydev, "Invalid package-mode: %s\n", mode_str);
		return -EINVAL;
	}

	return 0;
}

/**
 * qce1204_phy_clk_init - Initialize PHY clocks and resets
 * @phydev: PHY device
 * @dev: Device structure
 * @clk_data: PHY clock data structure
 * Returns: 0 on success, negative error code on failure
 */
static int qce1204_phy_clk_init(struct phy_device *phydev, struct device *dev,
				struct qce1204_clk_data *clk_data)
{
	struct clk_init_entry clk_table[] = {
		{&clk_data->tx_clk, "tx_clk", "TX clock"},
		{&clk_data->rx_clk, "rx_clk", "RX clock"},
		{&clk_data->sys_clk, "sys_clk", "SYS clock"},
		{&clk_data->tx_src_clk, "tx_src_clk", "TX SRC clock"},
		{&clk_data->rx_src_clk, "rx_src_clk", "RX SRC clock"},
	};
	struct reset_init_entry reset_table[] = {
		{&clk_data->tx_reset, "tx_reset", "TX reset"},
		{&clk_data->rx_reset, "rx_reset", "RX reset"},
		{&clk_data->sys_reset, "sys_reset", "SYS reset"},
	};
	int ret, i;

	/* Initialize all clocks */
	for (i = 0; i < ARRAY_SIZE(clk_table); i++) {
		ret = qce1204_clk_get(phydev, dev, clk_table[i].clk_ptr,
					clk_table[i].name, clk_table[i].desc);
		if (ret < 0)
			return ret;
	}

	/* Initialize all reset controls */
	for (i = 0; i < ARRAY_SIZE(reset_table); i++) {
		ret = qce1204_reset_get(phydev, dev, reset_table[i].reset_ptr,
					reset_table[i].name, reset_table[i].desc);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * qce1204_clk_probe - Initialize clocks and resets from DTS for each PHY
 * @phydev: PHY device
 * Returns: 0 on success, negative error code on failure
 *
 * Note: All clocks and resets are optional. Different PHYs may need different
 * clock configurations, so missing clocks are not treated as errors.
 */
static int qce1204_clk_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct qce1204_priv *priv = (struct qce1204_priv *)phydev->priv;
	struct qce1204_clk_data *clk_data;
	int ret;

	if (!priv)
		return -EINVAL;

	clk_data = &priv->clk_data;

	/* Initialize clocks and resets */
	ret = qce1204_phy_clk_init(phydev, dev, clk_data);
	if (ret < 0)
		return ret;

	return 0;
}

static DEVICE_ATTR(snr, 0444, qca81xx_phy_show_snr, NULL);

/*
|   sku   | ptp | macsec | P3/P4 2.5g |
|---------|-----|--------|------------|
| QCE1204 | yes |   yes  |     yes    |
| QCE2204 | yes |   yes  |     yes    |
| QCE2224 | yes |   yes  |     no     |
| QCE1224 | yes |   no   |     no     |
| NO-SKU  | yes |   yes  |     yes    |
*/
static int qce1204_phy_sku_probe(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct qce1204_shared_priv *priv;
	u32 val;

	if (!shared)
		return -EINVAL;

	priv = (struct qce1204_shared_priv *)shared->priv;
	if (!priv)
		return -EINVAL;

	priv->sku.ptp = true;
	priv->sku.macsec = true;
	priv->sku.p3p4_2p5g = true;
	val = qce1204_soc_read(phydev, QCE1204_SKU_REG) & QCE1204_SKU_ID_MASK;
	switch (val) {
	case QCE1204_SKU_QCE1204:
		priv->sku.name = "QCE1204";
		break;
	case QCE1204_SKU_QCE2204:
		priv->sku.name = "QCE2204";
		break;
	case QCE1204_SKU_QCE2224:
		priv->sku.name = "QCE2224";
		priv->sku.p3p4_2p5g = false;
		break;
	case QCE1204_SKU_QCE1224:
		priv->sku.name = "QCE1224";
		priv->sku.macsec = false;
		priv->sku.p3p4_2p5g = false;
		break;
	default:
		priv->sku.name = "NO-SKU";
		break;
	}
	phydev_info(phydev, "sku:%s, ptp:%s, macsec:%s, p3p4_2p5g:%s\n",
		priv->sku.name,
		priv->sku.ptp ? "enabled" : "disabled",
		priv->sku.macsec ? "enabled" : "disabled",
		priv->sku.p3p4_2p5g ? "enabled" : "disabled"
	);

	return 0;
}

int qce1204_phy_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct qce1204_shared_priv *shared_priv;
	struct qce1204_priv *priv;
	int port_idx, ret;

	/* Join PHY package and allocate shared private data */
	ret = devm_of_phy_package_join(dev, phydev, sizeof(struct qce1204_shared_priv));
	if (ret < 0)
		return ret;

	shared_priv = (struct qce1204_shared_priv *)phydev->shared->priv;

	/* Allocate private data structure for this PHY */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	/* Initialize clocks for this PHY */
	ret = qce1204_clk_probe(phydev);
	if (ret < 0) {
		phydev_err(phydev, "Failed to initialize clocks: %d\n", ret);
		return ret;
	}

	if (phy_package_probe_once(phydev)) {
		mutex_init(&shared_priv->pcs_lock);
		shared_priv->num_active_ports = qce1204_count_package_ports(phydev);

		/* Initialize shared clocks only once for the package */
		ret = qce1204_shared_clk_probe(phydev);
		if (ret < 0) {
			phydev_err(phydev, "Failed to initialize XPCS clocks: %d\n", ret);
			return ret;
		}

		/* Parse package mode from DTS */
		ret = qce1204_phy_package_mode_probe(phydev);
		if (ret < 0) {
			phydev_err(phydev, "Failed to parse package mode: %d\n", ret);
			return ret;
		}

		/* parse SKU information from security control register */
		ret = qce1204_phy_sku_probe(phydev);
		if (ret < 0) {
			phydev_err(phydev, "Failed to parse SKU information: %d\n", ret);
			return ret;
		}
		ret = qce1204_pcs_sys_clk_set_rate(phydev, QCE1204_CLK_RATE_25M);
		if (ret < 0)
			return ret;

		if (qce1204_get_package_mode(phydev) == PHY_INTERFACE_MODE_QUSGMII) {
			ret = qce1204_pcs_sys_clk_set(phydev, true);
			if (ret < 0)
				return ret;
			ret = qce1204_pcs_sys_reset(phydev);
			if (ret < 0)
				return ret;
		}
	}
	/* Enable PHY system clock and reset once at probe time; not repeated in config_init
	 * which may be called on interface changes.
	 */
	ret = qce1204_phy_sys_clk_set(phydev, true);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_sys_reset(phydev);
	if (ret < 0)
		return ret;
#if IS_ENABLED(CONFIG_HWMON)
	qce1204_hwmon_probe(phydev);
#endif
	device_create_file(&phydev->mdio.dev, &dev_attr_snr);

	port_idx = phydev->mdio.addr - phydev->shared->addr;
	if (port_idx >= 0 && port_idx < QCE1204_PORT_NUM) {
		mutex_lock(&shared_priv->pcs_lock);
		shared_priv->port_active |= BIT(port_idx);
		if (!shared_priv->probe_complete &&
		    /* >= rather than == to guard against DTS miscounting */
		    hweight8(shared_priv->port_active) >= shared_priv->num_active_ports)
			shared_priv->probe_complete = true;
		mutex_unlock(&shared_priv->pcs_lock);
	} else {
		phydev_warn(phydev,
			    "port_idx %d out of range [0, %d): probe_complete will never be set\n",
			    port_idx, QCE1204_PORT_NUM);
	}

	return 0;
}

void qce1204_phy_remove(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct qce1204_shared_priv *shared_priv;
	int port_idx;

	device_remove_file(&phydev->mdio.dev, &dev_attr_snr);

	if (!shared)
		return;

	shared_priv = (struct qce1204_shared_priv *)shared->priv;
	if (!shared_priv)
		return;

	port_idx = phydev->mdio.addr - shared->addr;
	if (port_idx < 0 || port_idx >= QCE1204_PORT_NUM)
		return;

	mutex_lock(&shared_priv->pcs_lock);
	shared_priv->port_active &= ~BIT(port_idx);
	shared_priv->port_hibernation[port_idx] = false;
	mutex_unlock(&shared_priv->pcs_lock);
}

static int qce1204_phy_ability_fix_up(struct phy_device *phydev)
{
	linkmode_clear_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
		phydev->supported);
	linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		phydev->supported);
	linkmode_clear_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
		phydev->advertising);
	linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		phydev->advertising);

	return 0;
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

/**
 * qce1204_phy_eee_common_init - Configure EEE timing registers shared by all PHY variants
 * @phydev: PHY device
 *
 * Configures the 2.5G EEE quiet/wake timing and 1G EEE PCS TX/RX timer
 * registers that are common to both QCE1204 and IPQ52xx integrated EPHY.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce1204_phy_eee_common_init(struct phy_device *phydev)
{
	int ret;

	/* Reduce the delay to respond to the quiet signal for 2.5G EEE */
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL0,
		QCE1204_PHY_2P5G_EEE_TX_QUIET_TIME0);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PHY_2P5G_EEE_TX_LPI_QUIET_CTRL1,
		QCE1204_PHY_2P5G_EEE_TX_QUIET_TIME1);
	if (ret < 0)
		return ret;
	/* Reduce the delay to respond to the wake up signal for 2.5G EEE */
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_PHY_2P5G_EEE_TX_LPI_WAKE_CTRL,
		QCE1204_PHY_2P5G_EEE_TX_WAKE_TIME);
	if (ret < 0)
		return ret;

	/* 1G EEE PCS TX/RX timer configuration */
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS, QCE1204_PHY_EEE_1G_TX_CTRL,
		QCE1204_PHY_EEE_1G_TX_CTRL_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS, QCE1204_PHY_EEE_1G_RX_CTRL,
		QCE1204_PHY_EEE_1G_RX_CTRL_VAL);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * qce1204_phy_eee_init - Initialize EEE configuration for QCE1204 PHY
 * @phydev: PHY device
 *
 * Configures EEE timing registers and QCE1204-specific analog tuning
 * registers for 1G and 100M EEE operation.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qce1204_phy_eee_init(struct phy_device *phydev)
{
	int ret;

	ret = qce1204_phy_eee_common_init(phydev);
	if (ret < 0)
		return ret;

	/* 1G EEE analog debug register tuning (QCE1204-specific) */
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_1G_CTRL0,
		QCE1204_DEBUG_ANA_EEE_1G_CTRL0_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_1G_CTRL1,
		QCE1204_DEBUG_ANA_EEE_1G_CTRL1_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_1G_CTRL2,
		QCE1204_DEBUG_ANA_EEE_1G_CTRL2_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_1G_CTRL3,
		QCE1204_DEBUG_ANA_EEE_1G_CTRL3_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_1G_CTRL4,
		QCE1204_DEBUG_ANA_EEE_1G_CTRL4_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_modify(phydev, QCE1204_DEBUG_ANA_EEE_1G_CTRL5,
		QCE1204_DEBUG_ANA_EEE_1G_CTRL5_EN, QCE1204_DEBUG_ANA_EEE_1G_CTRL5_EN);
	if (ret < 0)
		return ret;

	/* 100M EEE analog debug register tuning (QCE1204-specific) */
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_100M_CTRL0,
		QCE1204_DEBUG_ANA_EEE_100M_CTRL0_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_EEE_100M_CTRL1,
		QCE1204_DEBUG_ANA_EEE_100M_CTRL1_VAL);
	if (ret < 0)
		return ret;

	return 0;
}

static int qce1204_phy_10m_dac_init(struct phy_device *phydev)
{
	int ret = 0;

	/* adjust the voltage amplitude for 10BASE-T */
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL0,
		QCE1204_DEBUG_ANA_10M_DAC_CTRL0_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL1,
		QCE1204_DEBUG_ANA_10M_DAC_CTRL1_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL2,
		QCE1204_DEBUG_ANA_10M_DAC_CTRL2_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_10M_DAC_CTRL3,
		QCE1204_DEBUG_ANA_10M_DAC_CTRL3_VAL);

	return ret;
}

static int qce1204_phy_tlmm_init(struct phy_device *phydev)
{
	static const struct {
		u8 gpio_num;
		u8 fun;
		u8 drv;
		u8 led_en;
		u8 pull;
	} pin_cfgs[] = {
#define NC QCE1204_TLMM_NC
		{  0, QCE1204_GPIO0_FUNC_PHY_INT,  NC,                    NC, NC                       },
		{  1, QCE1204_GPIO1_FUNC_P0_LED_0, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  2, QCE1204_GPIO2_FUNC_P1_LED_0, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  3, QCE1204_GPIO3_FUNC_P2_LED_0, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  4, QCE1204_GPIO4_FUNC_P3_LED_0, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  5, QCE1204_GPIO5_FUNC_P0_LED_2, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  6, QCE1204_GPIO6_FUNC_P1_LED_2, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  7, QCE1204_GPIO7_FUNC_P2_LED_2, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  8, QCE1204_GPIO8_FUNC_P3_LED_2, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{  9, QCE1204_GPIO9_FUNC_P0_LED_1, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{ 15, QCE1204_GPIO15_FUNC_P1_LED_1, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{ 16, QCE1204_GPIO16_FUNC_P2_LED_1, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
		{ 17, QCE1204_GPIO17_FUNC_P3_LED_1, QCE1204_TLMM_DRV_16_MA, 1, QCE1204_TLMM_PULL_NO_PULL },
#undef NC
	};
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(pin_cfgs); i++) {
		u32 mask = 0;
		u32 set = 0;

		if (pin_cfgs[i].fun != QCE1204_TLMM_NC) {
			mask |= QCE1204_TLMM_FUNC_MASK;
			set  |= FIELD_PREP(QCE1204_TLMM_FUNC_MASK, pin_cfgs[i].fun);
		}
		if (pin_cfgs[i].drv != QCE1204_TLMM_NC) {
			mask |= QCE1204_TLMM_DRV;
			set  |= FIELD_PREP(QCE1204_TLMM_DRV, pin_cfgs[i].drv);
		}
		if (pin_cfgs[i].led_en != QCE1204_TLMM_NC) {
			mask |= QCE1204_TLMM_LED_MODE;
			if (pin_cfgs[i].led_en)
				set |= QCE1204_TLMM_LED_MODE;
		}
		if (pin_cfgs[i].pull != QCE1204_TLMM_NC) {
			mask |= QCE1204_TLMM_GPIO_PULL;
			set  |= FIELD_PREP(QCE1204_TLMM_GPIO_PULL, pin_cfgs[i].pull);
		}

		if (!mask)
			continue;

		ret = qce1204_soc_modify(phydev, TO_TLMM_CFG_REG(pin_cfgs[i].gpio_num),
			mask, set);
		if (ret < 0)
			return ret;
	}

	return 0;
}


int qce1204_phy_config_init(struct phy_device *phydev)
{
	struct qce1204_shared_priv *shared_priv;
	int ret = 0;
	phy_interface_t package_mode;

	package_mode = qce1204_get_package_mode(phydev);
	if (phy_package_init_once(phydev)) {
		if (package_mode == PHY_INTERFACE_MODE_QUSGMII) {
			/* configure work mode as PHY */
			ret = qce1204_soc_modify(phydev, QCE1204_WORK_MODE_SEL,
				QCE1204_PHY_MODE_MASK, QCE1204_PHY_MODE);
			if (ret < 0)
				return ret;
			ret = qce1204_pcs_qusgmii_mode_set(phydev);
			if (ret < 0)
				return ret;
			/*
			 * _qce1204_pcs_qusgmii_mode_set() calls qce1204_pcs_ana_assert()
			 * directly, bypassing qce1204_pcs_check_hibernate(). Sync
			 * pcs_assert cache so the short-circuit check sees a consistent
			 * value. The sequence ends with assert(false).
			 */
			shared_priv = (struct qce1204_shared_priv *)phydev->shared->priv;
			if (shared_priv) {
				mutex_lock(&shared_priv->pcs_lock);
				shared_priv->pcs_assert = false;
				mutex_unlock(&shared_priv->pcs_lock);
			}
			ret = qce1204_ahb_clk_set_rate(phydev, QCE1204_CLK_RATE_104M);
			if (ret < 0)
				return ret;
		} else if (package_mode == PHY_INTERFACE_MODE_INTERNAL) {
			/* configure work mode as switch */
			ret = qce1204_soc_modify(phydev, QCE1204_WORK_MODE_SEL,
				QCE1204_SWITCH_MODE_MASK, QCE1204_SWITCH_MODE);
			if (ret < 0)
				return ret;
		} else {
			phydev_err(phydev, "Unsupported package mode: 0x%x\n", package_mode);
			return -EINVAL;
		}
		ret = qce1204_phy_tlmm_init(phydev);
		if (ret < 0)
			return ret;
#if IS_ENABLED(CONFIG_HWMON)
		qce1204_hwmon_hw_init_once(phydev);
#endif
		/* enable efuse loading into analog circuit */
		ret = qce1204_soc_modify(phydev, QCE1204_EPHY_CFG,
			QCE1204_EPHY_LDO_CTRL, 0);
		if (ret < 0)
			return ret;
		mdelay(10);
	}

	ret = qce1204_phy_clk_parent_init(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_eee_init(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_10m_dac_init(phydev);
	if (ret < 0)
		return ret;
	/* adjust the tx gain to improve 2.5G performance */
	ret = qca81xx_phy_debug_write(phydev, QCE1204_DEBUG_ANA_2P5G_TX_GAIN_CTRL,
		QCE1204_DEBUG_ANA_2P5G_TX_GAIN_VAL);
	if (ret < 0)
		return ret;
	/* disable force link to improve 2.5G performance */
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, QCE1204_MMD3_FORCE_LINK_CTRL,
		QCE1204_FORCE_LINK_EN, 0);
	if (ret < 0)
		return ret;
	/* force pll0 on to improve traffic performance */
	ret = qca81xx_phy_debug_modify(phydev, QCE1204_DEBUG_PLL_CTRL,
		QCE1204_DEBUG_PLL_EN, QCE1204_DEBUG_PLL_EN);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_cdt_thresh_init(phydev);
	if (ret < 0)
		return ret;
	/* 10M speed also use led0 in default as other speeds */
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, QCE1204_MMD7_LED0_CTRL,
		QCE1204_SPEED_10M_ON, QCE1204_SPEED_10M_ON);
	if (ret < 0)
		return ret;
	 if (package_mode == PHY_INTERFACE_MODE_QUSGMII) {
		ret = qce1204_phy_ability_fix_up(phydev);
		if (ret < 0)
			return ret;
	 }
#if IS_ENABLED(CONFIG_HWMON)
	qce1204_hwmon_hw_init(phydev);
#endif
	ret = qca81xx_phy_stats_enable(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_soft_reset(phydev);
	if (ret < 0)
		return ret;

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
		if (ret < 0)
			return ret;
		mdelay(10);
		clk_en = true;
	}
	ret = qce1204_pcs_clk_set(phydev, channel, clk_en);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s pcs clocks for CH%d, ret: %d\n",
			clk_en ? "enable" : "disable", channel, ret);
		return ret;
	}
	pcs_clk_enabled = clk_en;
	ret = qce1204_phy_clk_set(phydev, clk_en);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s phy clocks for CH%d, ret: %d\n",
			clk_en ? "enable" : "disable", channel, ret);
		goto err_disable_pcs_clk;
	}
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
	/* change IPG from 10 to 11 for 1G speed */
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, QCE1204_PHY_MMD7_IPG_OP,
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

static int qce1204_phy_speed_clock_set(struct phy_device *phydev)
{
	unsigned long clk_rate = 0;
	struct qce1204_clk_data *clk_data = NULL;
	int ret = 0;

	/* Determine clock rates based on speed */
	switch (phydev->speed) {
	case SPEED_2500:
		clk_rate = QCE1204_CLK_RATE_312P5M;
		break;
	case SPEED_1000:
		clk_rate = QCE1204_CLK_RATE_125M;
		break;
	case SPEED_100:
		clk_rate = QCE1204_CLK_RATE_25M;
		break;
	case SPEED_10:
		clk_rate = QCE1204_CLK_RATE_2P5M;
		break;
	default:
		phydev_err(phydev, "Unsupported speed: %d\n", phydev->speed);
		return -EOPNOTSUPP;
	}
	ret = qce1204_validate_clock_rate(clk_rate);
	if (ret < 0) {
		phydev_err(phydev, "Invalid clock rate: %lu Hz\n", clk_rate);
		return ret;
	}

	clk_data = qce1204_get_clk_data(phydev);
	if (!clk_data)
		return -EINVAL;
	if (clk_data->tx_clk) {
		ret = clk_set_rate(clk_data->tx_clk, clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set TX clock rate: %d\n", ret);
			return ret;
		}
	}
	if (clk_data->rx_clk) {
		ret = clk_set_rate(clk_data->rx_clk, clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set RX clock rate: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int qce1204_phy_internal_speed_fix_up(struct phy_device *phydev)
{
	bool clk_en = false;
	int ret;

	if (phydev->link) {
		ret = qce1204_phy_speed_clock_set(phydev);
		if (ret < 0)
			return ret;
		clk_en = true;
	}
	ret = qce1204_phy_clk_set(phydev, clk_en);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s phy clocks, ret: %d\n",
			clk_en ? "enable" : "disable", ret);
		return ret;
	}
	/* reset phy clocks */
	ret = qce1204_phy_clk_reset(phydev);
	if (ret < 0)
		goto err_disable_clks;
	ret = qce1204_phy_fifo_reset(phydev, true);
	if (ret < 0)
		goto err_disable_clks;
	mdelay(1);
	ret = qce1204_phy_fifo_reset(phydev, false);
	if (ret < 0)
		goto err_disable_clks;

	return 0;

err_disable_clks:
	if (clk_en)
		qce1204_phy_clk_set(phydev, false);

	return ret;
}

static int qce_phy_read_status(struct phy_device *phydev)
{
	int ret = 0;

	ret = genphy_c45_read_status(phydev);
	if (ret < 0)
		return ret;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = phy_read_mmd(phydev, MDIO_MMD_VEND2,
			QCE1204_PHY_1000BASET_STATUS);
		if (ret < 0)
			return ret;
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
			phydev->lp_advertising,
			ret & QCE1204_PHY_LP_ADVERTISE_1000FULL);
		/* need to get speed and duplex again with new lp adv */
		phy_resolve_aneg_linkmode(phydev);
	} else {
		ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MII_BMCR);
		if (ret < 0)
			return ret;
		phydev->duplex = (ret & BMCR_FULLDPLX) ? DUPLEX_FULL : DUPLEX_HALF;
	}

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCE1204_PHY_SPEC_STATUS);
	if (ret < 0)
		return ret;
	if (ret & QCE1204_PHY_INTR_DOWNSHIFT) {
		switch (ret & QCE1204_PHY_SS_SPEED_MASK) {
		case QCE1204_PHY_SS_SPEED_2500:
			phydev->speed = SPEED_2500;
			break;
		case QCE1204_PHY_SS_SPEED_1000:
			phydev->speed = SPEED_1000;
			break;
		case QCE1204_PHY_SS_SPEED_100:
			phydev->speed = SPEED_100;
			break;
		case QCE1204_PHY_SS_SPEED_10:
			phydev->speed = SPEED_10;
			break;
		default:
			phydev->speed = SPEED_UNKNOWN;
		}
		if (ret & QCE1204_PHY_SS_DUPLEX_FULL)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;
	}
	phydev->mdix = (ret & QCE1204_PHY_SS_MDIX) ? ETH_TP_MDI_X : ETH_TP_MDI;
	ret = qce1204_phy_mdix_ctrl_get(phydev);
	if (ret < 0)
		return ret;

	ret = qca81xx_phy_master_slave_get(phydev);
	if (ret)
		return ret;

	return 0;
}

int qce1204_phy_read_status(struct phy_device *phydev)
{
	int ret = 0, old_link = 0;

	old_link = phydev->link;

	ret = qce_phy_read_status(phydev);
	if (ret < 0)
		return ret;

	/*
	 * Gate on the package mode, the same criterion config_init() and probe()
	 * use to bring up the PCS.  Keying off the per-port phydev->interface
	 * would let a mismatched "phy-mode" skip this port's hibernation update,
	 * leaving its port_hibernation[] slot false forever and so suppressing
	 * the assertion for the whole package.
	 */
	if (qce1204_get_package_mode(phydev) == PHY_INTERFACE_MODE_QUSGMII) {
		ret = qce1204_pcs_check_hibernate(phydev);
		if (ret < 0) {
			dev_warn_ratelimited(&phydev->mdio.dev,
				"PCS hibernation check failed: %d\n", ret);
			ret = 0;
		}
	}

	if (phydev->link != old_link) {
		if (phydev->interface == PHY_INTERFACE_MODE_QUSGMII) {
			ret = qce1204_phy_qusgmii_speed_fix_up(phydev);
			if (ret < 0)
				return ret;
		}
		if (phydev->interface == PHY_INTERFACE_MODE_INTERNAL) {
			ret = qce1204_phy_internal_speed_fix_up(phydev);
			if (ret < 0)
				return ret;
		}
	}

#if IS_ENABLED(CONFIG_HWMON)
	qce1204_thermal_check(phydev);
#endif

	return 0;
}

/* IPQ52xx TCSR GPHY LDO registers */
#define TCSR_GPHY_LDO_BIAS_EN					0x1961000
#define GPHY_LDO_BIAS_EN					BIT(0)

/* Hermosa built-in PHY requires an adjusted CDT threshold on CTRL14 */
#define IPQ52XX_MMD3_CDT_THRESH_CTRL14_VAL			0x9bb0

/**
 * ipq52xx_phy_clk_probe - Get clocks and resets for IPQ52xx built-in PHY
 * @phydev: PHY device
 *
 * Gets clocks and resets from the MDIO bus device (mdio-ahb), which has
 * the clock and reset definitions in its DTS node.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq52xx_phy_clk_probe(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	struct device *bus_dev = phydev->mdio.bus->parent;

	priv->rx_clk = devm_clk_get_optional(bus_dev, "gephy_rx_clk");
	if (IS_ERR(priv->rx_clk)) {
		phydev_err(phydev, "Failed to get RX clock: %ld\n", PTR_ERR(priv->rx_clk));
		return PTR_ERR(priv->rx_clk);
	}

	priv->tx_clk = devm_clk_get_optional(bus_dev, "gephy_tx_clk");
	if (IS_ERR(priv->tx_clk)) {
		phydev_err(phydev, "Failed to get TX clock: %ld\n", PTR_ERR(priv->tx_clk));
		return PTR_ERR(priv->tx_clk);
	}

	priv->raw_clk = devm_clk_get_optional(bus_dev, "gephy_raw_clk");
	if (IS_ERR(priv->raw_clk)) {
		phydev_err(phydev, "Failed to get RAW clock: %ld\n", PTR_ERR(priv->raw_clk));
		return PTR_ERR(priv->raw_clk);
	}

	priv->rx_reset = devm_reset_control_get_optional(bus_dev, "gephy_rx_reset");
	if (IS_ERR(priv->rx_reset)) {
		phydev_err(phydev, "Failed to get RX reset: %ld\n", PTR_ERR(priv->rx_reset));
		return PTR_ERR(priv->rx_reset);
	}

	priv->tx_reset = devm_reset_control_get_optional(bus_dev, "gephy_tx_reset");
	if (IS_ERR(priv->tx_reset)) {
		phydev_err(phydev, "Failed to get TX reset: %ld\n", PTR_ERR(priv->tx_reset));
		return PTR_ERR(priv->tx_reset);
	}

	priv->sys_reset = devm_reset_control_get_optional(bus_dev, "gephy_sys_reset");
	if (IS_ERR(priv->sys_reset)) {
		phydev_err(phydev, "Failed to get SYS reset: %ld\n", PTR_ERR(priv->sys_reset));
		return PTR_ERR(priv->sys_reset);
	}

	return 0;
}

/**
 * ipq52xx_phy_clk_set - Enable or disable IPQ52xx PHY RX/TX clocks
 * @phydev: PHY device
 * @enable: true to enable clocks, false to disable
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq52xx_phy_clk_set(struct phy_device *phydev, bool enable)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	int ret;

	if (!priv)
		return -EINVAL;

	if (priv->rx_clk) {
		if (enable) {
			ret = clk_prepare_enable(priv->rx_clk);
			if (ret) {
				phydev_err(phydev, "Failed to enable RX clock: %d\n", ret);
				return ret;
			}
		} else {
			clk_disable_unprepare(priv->rx_clk);
		}
	}

	if (priv->tx_clk) {
		if (enable) {
			ret = clk_prepare_enable(priv->tx_clk);
			if (ret) {
				phydev_err(phydev, "Failed to enable TX clock: %d\n", ret);
				if (priv->rx_clk)
					clk_disable_unprepare(priv->rx_clk);
				return ret;
			}
		} else {
			clk_disable_unprepare(priv->tx_clk);
		}
	}

	return 0;
}

/**
 * ipq52xx_phy_clk_reset - Reset IPQ52xx PHY RX/TX using reset controls
 * @phydev: PHY device
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq52xx_phy_clk_reset(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	int ret;

	if (!priv)
		return -EINVAL;

	if (priv->rx_reset) {
		ret = reset_control_assert(priv->rx_reset);
		if (ret) {
			phydev_err(phydev, "Failed to assert RX reset: %d\n", ret);
			return ret;
		}
	}

	if (priv->tx_reset) {
		ret = reset_control_assert(priv->tx_reset);
		if (ret) {
			phydev_err(phydev, "Failed to assert TX reset: %d\n", ret);
			/* Cleanup: deassert rx_reset if it was asserted */
			if (priv->rx_reset)
				reset_control_deassert(priv->rx_reset);
			return ret;
		}
	}

	mdelay(1);

	if (priv->rx_reset) {
		ret = reset_control_deassert(priv->rx_reset);
		if (ret) {
			phydev_err(phydev, "Failed to deassert RX reset: %d\n", ret);
			return ret;
		}
	}

	if (priv->tx_reset) {
		ret = reset_control_deassert(priv->tx_reset);
		if (ret) {
			phydev_err(phydev, "Failed to deassert TX reset: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

/**
 * ipq52xx_phy_sys_clk_reset - Reset IPQ52xx PHY SYS using reset control
 * @phydev: PHY device
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq52xx_phy_sys_clk_reset(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	int ret;

	if (!priv || !priv->sys_reset)
		return 0;

	ret = reset_control_assert(priv->sys_reset);
	if (ret) {
		phydev_err(phydev, "Failed to assert SYS reset: %d\n", ret);
		return ret;
	}

	mdelay(10);

	ret = reset_control_deassert(priv->sys_reset);
	if (ret) {
		phydev_err(phydev, "Failed to deassert SYS reset: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ipq52xx_phy_ldo_loading_enable(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	u32 val;

	if (!priv || !priv->ldo_bias_reg)
		return -EINVAL;

	/* Enable efuse loading into analog circuit (active low: clear bit 0) */
	val = readl(priv->ldo_bias_reg);
	val &= ~GPHY_LDO_BIAS_EN;
	writel(val, priv->ldo_bias_reg);

	return 0;
}

/**
 * ipq52xx_phy_speed_clock_set - Set IPQ52xx PHY clock rate based on speed
 * @phydev: PHY device
 *
 * Sets the RX and TX clock rates according to the link speed:
 * - 2.5G: 312.5 MHz
 * - 1G:   125 MHz
 * - 100M: 25 MHz
 * - 10M:  2.5 MHz
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq52xx_phy_speed_clock_set(struct phy_device *phydev)
{
	struct ipq52xx_phy_priv *priv = phydev->priv;
	unsigned long clk_rate;
	int ret;

	if (!priv)
		return -EINVAL;
	/* Determine clock rate based on speed */
	switch (phydev->speed) {
	case SPEED_2500:
		clk_rate = QCE1204_CLK_RATE_312P5M;  /* 312.5 MHz */
		break;
	case SPEED_1000:
		clk_rate = QCE1204_CLK_RATE_125M;    /* 125 MHz */
		break;
	case SPEED_100:
		clk_rate = QCE1204_CLK_RATE_25M;     /* 25 MHz */
		break;
	case SPEED_10:
		clk_rate = QCE1204_CLK_RATE_2P5M;    /* 2.5 MHz */
		break;
	default:
		phydev_err(phydev, "Unsupported speed: %d\n", phydev->speed);
		return -EOPNOTSUPP;
	}

	/* Validate clock rate */
	ret = qce1204_validate_clock_rate(clk_rate);
	if (ret < 0) {
		phydev_err(phydev, "Invalid clock rate: %lu Hz\n", clk_rate);
		return ret;
	}

	if (priv->raw_clk) {
		ret = clk_set_rate(priv->raw_clk, clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set EPHY RAW clock rate to %lu Hz: %d\n",
				   clk_rate, ret);
			return ret;
		}
	}
	if (priv->rx_clk) {
		ret = clk_set_rate(priv->rx_clk, clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set RX clock rate to %lu Hz: %d\n",
				   clk_rate, ret);
			return ret;
		}
	}
	if (priv->tx_clk) {
		ret = clk_set_rate(priv->tx_clk, clk_rate);
		if (ret < 0) {
			phydev_err(phydev, "Failed to set TX clock rate to %lu Hz: %d\n",
				   clk_rate, ret);
			return ret;
		}
	}

	return 0;
}

static int ipq52xx_phy_internal_speed_fix_up(struct phy_device *phydev)
{
	bool clk_en = false;
	bool ephy_clk_enabled = false;
	int ret;

	if (phydev->link) {
		ret = ipq52xx_phy_speed_clock_set(phydev);
		if (ret < 0)
			return ret;
		clk_en = true;
	}
	ret = ipq52xx_phy_clk_set(phydev, clk_en);
	if (ret < 0) {
		phydev_err(phydev, "Failed to %s EPHY clocks, ret: %d\n",
			   clk_en ? "enable" : "disable", ret);
		return ret;
	}
	ephy_clk_enabled = clk_en;
	/* reset phy clocks */
	ret = ipq52xx_phy_clk_reset(phydev);
	if (ret < 0)
		goto err_disable_clks;
	mdelay(1);
	ret = qce1204_phy_fifo_reset(phydev, true);
	if (ret < 0)
		goto err_disable_clks;
	mdelay(1);
	ret = qce1204_phy_fifo_reset(phydev, false);
	if (ret < 0)
		goto err_disable_clks;

	return 0;

err_disable_clks:
	if (ephy_clk_enabled)
		ipq52xx_phy_clk_set(phydev, false);
	return ret;
}

int ipq52xx_phy_read_status(struct phy_device *phydev)
{
	int ret = 0, old_link = 0;

	old_link = phydev->link;

	ret = qce_phy_read_status(phydev);
	if (ret < 0)
		return ret;
	if (phydev->link != old_link) {
		ret = ipq52xx_phy_internal_speed_fix_up(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * ipq52xx_phy_probe - Probe IPQ52xx built-in PHY, map registers once
 * @phydev: PHY device
 * Returns: 0 on success, negative error code on failure
 *
 * Maps all hardware registers using devm_ioremap so they are available
 * for the lifetime of the driver without repeated ioremap/iounmap calls.
 */
int ipq52xx_phy_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct ipq52xx_phy_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->ldo_bias_reg = devm_ioremap(dev, TCSR_GPHY_LDO_BIAS_EN, 4);
	if (!priv->ldo_bias_reg) {
		phydev_err(phydev, "Failed to map LDO bias register\n");
		return -ENOMEM;
	}

	phydev->priv = priv;

	ret = ipq52xx_phy_clk_probe(phydev);
	if (ret) {
		phydev_err(phydev, "Failed to initialize clocks: %d\n", ret);
		return ret;
	}

	/* enable efuse loading into analog circuit */
	ret = ipq52xx_phy_ldo_loading_enable(phydev);
	if (ret < 0)
		return ret;
	msleep(10);
	ret = ipq52xx_phy_sys_clk_reset(phydev);
	if (ret < 0)
		return ret;

	device_create_file(&phydev->mdio.dev, &dev_attr_snr);

	return 0;
}

void ipq52xx_phy_remove(struct phy_device *phydev)
{
	device_remove_file(&phydev->mdio.dev, &dev_attr_snr);
}

/**
 * ipq52xx_phy_eee_init - Initialize EEE configuration for IPQ52xx integrated PHY
 * @phydev: PHY device
 *
 * Configures only the common PCS timer registers (2.5G EEE quiet/wake timing
 * and 1G EEE TX/RX timers) for the IPQ52xx integrated EPHY. The analog debug
 * registers used in qce1204_phy_eee_init are QCE1204-specific and do not exist
 * on the IPQ52xx built-in PHY.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ipq52xx_phy_eee_init(struct phy_device *phydev)
{
	/*
	 * Only the common PCS timer registers (2.5G EEE quiet/wake timing and
	 * 1G EEE TX/RX timers) are needed for the IPQ52xx integrated EPHY.
	 * The six analog debug registers (QCE1204_DEBUG_ANA_EEE_1G_CTRL0~CTRL5)
	 * used in qce1204_phy_eee_init are QCE1204-specific analog tuning
	 * registers that do not exist on the IPQ52xx built-in PHY and must not
	 * be written here. 100M EEE analog tuning is likewise QCE1204-specific
	 * and is omitted.
	 */
	return qce1204_phy_eee_common_init(phydev);
}

static int ipq52xx_phy_cdt_thresh_init(struct phy_device *phydev)
{
	int ret = qce1204_phy_cdt_thresh_init(phydev);

	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCE1204_MMD3_CDT_THRESH_CTRL14, IPQ52XX_MMD3_CDT_THRESH_CTRL14_VAL);

	return ret;
}

int ipq52xx_phy_config_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = ipq52xx_phy_eee_init(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_10m_dac_init(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_stats_enable(phydev);
	if (ret < 0)
		return ret;
	/* Enable pll to improve traffic performance */
	ret = qca81xx_phy_debug_modify(phydev, QCE1204_DEBUG_PLL_CTRL,
		QCE1204_DEBUG_PLL_EN, QCE1204_DEBUG_PLL_EN);
	if (ret < 0)
		return ret;
	/* 10M speed also use led0 in default as other speeds */
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, QCE1204_MMD7_LED0_CTRL,
		QCE1204_SPEED_10M_ON, QCE1204_SPEED_10M_ON);
	if (ret < 0)
		return ret;
	/* config the led as active high in default */
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, QCE1204_MMD7_LED_POLARITY_CTRL,
		QCE1204_LED_ACTIVE_HIGH, QCE1204_LED_ACTIVE_HIGH);
	if (ret < 0)
		return ret;
	ret = ipq52xx_phy_cdt_thresh_init(phydev);
	if (ret < 0)
		return ret;
	ret = qce1204_phy_soft_reset(phydev);
	if (ret < 0)
		return ret;

	return 0;
}
