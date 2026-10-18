// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <common.h>
#include <command.h>
#include <miiphy.h>
#include <phy.h>
#include <asm/io.h>
#include <errno.h>
#include <div64.h>
#include <malloc.h>
#include <asm/global_data.h>
#include <fdtdec.h>

#include "qti_8x8x.h"

#ifdef DEBUG
#define pr_dbg(fmt, args...) printf(fmt, ##args);
#else
#define pr_dbg(fmt, args...)
#endif

#define DEV_8X8X_PHY_MODE		0x0
#define DEV_8X8X_SWITCH_MODE		0x1
#define DEV_8X8X_BYPASS_MODE		0x2

struct qti_8x8x_device {
	struct mii_dev	*bus;
	unsigned int mode;
	struct qti_8x8x_switch_info * switch_info;
};

/***************************************************************************/
/* mdio_function start */
/***************************************************************************/

/***************************************************************************/
/* mii functions */
/***************************************************************************/

static void qti_8x8x_split_mii_addr(uint32_t regaddr, uint16_t *r1,
		uint16_t *r2, uint16_t *page, uint16_t *switch_phy_id)
{
	*r1 = regaddr & 0x1c;

	regaddr >>= 5;
	*r2 = regaddr & 0x7;

	regaddr >>= 3;
	*page = regaddr & 0xffff;

	regaddr >>= 16;
	*switch_phy_id = regaddr & 0xff;
}

static uint32_t qti_8x8x_mii_read(struct phy_device *phydev, uint32_t reg)
{
	uint16_t r1, r2, page, switch_phy_id;
	uint16_t lo, hi;
	struct phy_device local_phydev;

	qti_8x8x_split_mii_addr((uint32_t) reg, &r1, &r2,
			&page, &switch_phy_id);

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = (0x18 | (switch_phy_id >> 5));

	phy_write(&local_phydev, MDIO_DEVAD_NONE,
			switch_phy_id & 0x1f, page);
	udelay(100);
	local_phydev.addr = (0x10 | r2);
	lo = phy_read(&local_phydev, MDIO_DEVAD_NONE, r1);
	hi = phy_read(&local_phydev, MDIO_DEVAD_NONE, r1 + 2);

	return (hi << 16) | lo;
}

static void qti_8x8x_mii_write(struct phy_device *phydev, uint32_t reg,
		uint32_t val)
{
	uint16_t r1, r2, page, switch_phy_id;
	uint16_t lo, hi;
	struct phy_device local_phydev;

	qti_8x8x_split_mii_addr((uint32_t) reg, &r1, &r2,
			&page, &switch_phy_id);
	lo = val & 0xffff;
	hi = (uint16_t) (val >> 16);

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = (0x18 | (switch_phy_id >> 5));

	phy_write(&local_phydev, MDIO_DEVAD_NONE,
			switch_phy_id & 0x1f, page);
	udelay(100);
	local_phydev.addr = (0x10 | r2);
	phy_write(&local_phydev, MDIO_DEVAD_NONE, r1, lo);
	phy_write(&local_phydev, MDIO_DEVAD_NONE, r1 + 2, hi);
}

static void qti_8x8x_mii_update(struct phy_device *phydev, uint32_t reg,
		uint32_t mask, uint32_t val)
{
	uint32_t new_val = 0, org_val = 0;

	org_val = qti_8x8x_mii_read(phydev, reg);

	new_val = org_val & ~mask;
	new_val |= val & mask;

	if (new_val != org_val)
		qti_8x8x_mii_write(phydev, reg, new_val);
}

/***************************************************************************/
/* mmd functions */
/***************************************************************************/

static u16 qti_8x8x_phy_mmd_read(struct phy_device *phydev, u32 phy_addr,
		u16 mmd_num, u16 reg_id)
{
//	uint32_t reg_id_c45 = QTI_8X8X_REG_C45_ADDRESS(mmd_num, reg_id);
	struct phy_device local_phydev;
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;
	return (u16)phy_read(&local_phydev, mmd_num, reg_id);
}

static int qti_8x8x_phy_mmd_write(struct phy_device *phydev, u32 phy_addr,
		u16 mmd_num, u16 reg_id, u16 value)
{
//	uint32_t reg_id_c45 = QTI_8X8X_REG_C45_ADDRESS(mmd_num, reg_id);
	struct phy_device local_phydev;
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;
	return phy_write(&local_phydev, mmd_num, reg_id, value);
}

static void qti_8x8x_phy_modify_mmd(struct phy_device *phydev,
		uint32_t phy_addr, uint32_t mmd_num,
		uint32_t mmd_reg, uint32_t mask, uint32_t value)
{
	uint16_t phy_data = 0, new_phy_data = 0;

	phy_data = qti_8x8x_phy_mmd_read(phydev, phy_addr,
			mmd_num, mmd_reg);
	new_phy_data = (phy_data & ~mask) | value;
	qti_8x8x_phy_mmd_write(phydev, phy_addr,
			mmd_num, mmd_reg, new_phy_data);

	/* check the mmd register value */
	phy_data = qti_8x8x_phy_mmd_read(phydev, phy_addr,
			mmd_num, mmd_reg);
	pr_dbg("phy_addr:0x%x, mmd_reg:0x%x, phy_data:0x%x\n",
		phy_addr, mmd_reg, phy_data);
}

/***************************************************************************/
/* dev reg access functions */
/***************************************************************************/

static int qti_8x8x_reg_field_get(struct phy_device *phydev, u32 reg_addr,
		u32 bit_offset,	u32 field_len, u8 value[])
{
	u32 reg_val = qti_8x8x_mii_read(phydev, reg_addr);

	*((u32 *) value) = SW_REG_2_FIELD(reg_val, bit_offset, field_len);
	return 0;
}

static int qti_8x8x_reg_field_set(struct phy_device *phydev, u32 reg_addr,
		u32 bit_offset, u32 field_len, const u8 value[])
{
	u32 field_val = *((u32 *) value);
	u32 reg_val = qti_8x8x_mii_read(phydev, reg_addr);

	SW_REG_SET_BY_FIELD_U32(reg_val, field_val, bit_offset, field_len);

	qti_8x8x_mii_write(phydev, reg_addr, reg_val);
	return 0;
}

/***************************************************************************/
/* phy reg access functions */
/***************************************************************************/

static u16 qti_8x8x_phy_reg_read(struct phy_device *phydev,
		u32 phy_addr, u32 reg_id)
{
	struct phy_device local_phydev;
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;
	return phy_read(&local_phydev, MDIO_DEVAD_NONE, reg_id);
}

static int qti_8x8x_phy_reg_write(struct phy_device *phydev,
		u32 phy_addr, u32 reg_id, u16 value)
{
	struct phy_device local_phydev;
	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;
	return phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_id, value);
}

static void qti_8x8x_phy_modify_mii(struct phy_device *phydev,
		uint32_t phy_addr, uint32_t mii_reg,
		uint32_t mask, uint32_t value)
{
	uint16_t phy_data = 0, new_phy_data = 0;

	phy_data = qti_8x8x_phy_reg_read(phydev, phy_addr, mii_reg);
	new_phy_data = (phy_data & ~mask) | value;
	qti_8x8x_phy_reg_write(phydev, phy_addr, mii_reg, new_phy_data);

	/* check the mii register value */
	phy_data = qti_8x8x_phy_reg_read(phydev, phy_addr, mii_reg);
	pr_dbg("phy_addr:0x%x, mii_reg:0x%x, phy_data:0x%x\n",
		phy_addr, mii_reg, phy_data);
}

static void qti_8x8x_phy_reset(struct phy_device *phydev, u32 phy_addr)
{
	u16 phy_data;

	phy_data = qti_8x8x_phy_reg_read(phydev, phy_addr,
			QTI_8X8X_PHY_CONTROL);
	qti_8x8x_phy_reg_write(phydev, phy_addr,
			QTI_8X8X_PHY_CONTROL,
			phy_data | QTI_8X8X_CTRL_SOFTWARE_RESET);
}

/***************************************************************************/
/* mdio_fixup functions */
/***************************************************************************/

static int qti_8x8x_get_core_clk_status(struct phy_device *phydev,
		uint32_t reg)
{
	u32 val;

	val = qti_8x8x_mii_read(phydev, reg);
	return (val & BIT(0));
}

static void qti_8x8x_core_clk_enable(struct phy_device *phydev, uint32_t reg)
{
	u32 val;

	val = qti_8x8x_mii_read(phydev, reg);
	val |= BIT(0);
	qti_8x8x_mii_write(phydev, reg, val);
}

static void qti_8x8x_core_clk_disable(struct phy_device *phydev, uint32_t reg)
{
	u32 val;

	val = qti_8x8x_mii_read(phydev, reg);
	val &= ~BIT(0);
	qti_8x8x_mii_write(phydev, reg, val);
}

static void qti_8x8x_core_clk_reset(struct phy_device *phydev, uint32_t reg)
{
	u32 val;

	val = qti_8x8x_mii_read(phydev, reg);
	val |= BIT(2);
	qti_8x8x_mii_write(phydev, reg, val);

	udelay(21000);

	val &= ~BIT(2);
	qti_8x8x_mii_write(phydev, reg, val);
}

static u16 qti_8x8x_phy_dbg_read(struct phy_device *phydev,
		u32 phy_addr, u32 reg_id)
{
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;

	phy_write(&local_phydev, MDIO_DEVAD_NONE,
			QTI_8X8X_PHY_DEBUG_PORT_ADDR, reg_id);
	return phy_read(&local_phydev, MDIO_DEVAD_NONE,
			QTI_8X8X_PHY_DEBUG_PORT_DATA);
}

static void qti_8x8x_phy_dbg_write(struct phy_device *phydev,
		u32 phy_addr, u32 reg_id, u16 reg_val)
{
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phy_addr;

	phy_write(&local_phydev, MDIO_DEVAD_NONE,
			QTI_8X8X_PHY_DEBUG_PORT_ADDR, reg_id);
	phy_write(&local_phydev, MDIO_DEVAD_NONE,
			QTI_8X8X_PHY_DEBUG_PORT_DATA, reg_val);
}

static void qti_8x8x_efuse_loading(struct phy_device *phydev, u8 ethphy)
{
	u32 val = 0, ldo_efuse = 0, icc_efuse = 0, phy_addr = 0;
	u16 reg_val = 0;

	phy_addr = qti_8x8x_mii_read(phydev, QTI_8X8X_EPHY_CFG) >>
		(ethphy * QTI_8X8X_PHY_ADDR_LENGTH) & GENMASK(4, 0);
	switch(ethphy) {
	case 0:
		val = qti_8x8x_mii_read(phydev,
				QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW4_LSB);
		ldo_efuse = (val & GENMASK(21, 18)) >> 18;
		icc_efuse = (val & GENMASK(26, 22)) >> 22;
		break;
	case 1:
		val = qti_8x8x_mii_read(phydev,
				QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW7_LSB);
		ldo_efuse = (val & GENMASK(26, 23)) >> 23;
		icc_efuse = (val & GENMASK(31, 27)) >> 27;
		break;
	case 2:
		val = qti_8x8x_mii_read(phydev,
				QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW8_LSB);
		ldo_efuse = (val & GENMASK(26, 23)) >> 23;
		icc_efuse = (val & GENMASK(31, 27)) >> 27;
		break;
	case 3:
		val = qti_8x8x_mii_read(phydev,
				QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW6_MSB);
		ldo_efuse = (val & GENMASK(17, 14)) >> 14;
		icc_efuse = (val & GENMASK(22, 18)) >> 18;
		break;
	}

	reg_val = qti_8x8x_phy_dbg_read(phydev, phy_addr,
			QTI_8X8X_PHY_LDO_EFUSE_REG);
	reg_val = (reg_val & ~GENMASK(7, 4)) | (ldo_efuse << 4);
	qti_8x8x_phy_dbg_write(phydev, phy_addr,
			QTI_8X8X_PHY_LDO_EFUSE_REG, reg_val);

	reg_val = qti_8x8x_phy_dbg_read(phydev, phy_addr,
			QTI_8X8X_PHY_ICC_EFUSE_REG);
	reg_val = (reg_val & ~GENMASK(4, 0)) | icc_efuse;
	qti_8x8x_phy_dbg_write(phydev, phy_addr,
			QTI_8X8X_PHY_ICC_EFUSE_REG, reg_val);
}

static void qti_8x8x_core_clock_init(struct phy_device *phydev)
{
	u32 val = 0;
	int i;

	/* Enable serdes */
	qti_8x8x_core_clk_enable(phydev, QTI_8X8X_SRDS0_SYS_CBCR);
	qti_8x8x_core_clk_enable(phydev, QTI_8X8X_SRDS1_SYS_CBCR);

	/* Reset serdes */
	qti_8x8x_core_clk_reset(phydev, QTI_8X8X_SRDS0_SYS_CBCR);
	qti_8x8x_core_clk_reset(phydev, QTI_8X8X_SRDS1_SYS_CBCR);

	/* Disable EPHY GMII clock */
	i = 0;
	while (i < 2 * QTI_8X8X_PHY_ADDR_NUM) {
		qti_8x8x_core_clk_disable(phydev,
				QTI_8X8X_GEPHY0_TX_CBCR + i*0x20);
		i++;
	}

	/* Enable ephy */
	qti_8x8x_core_clk_enable(phydev, QTI_8X8X_EPHY0_SYS_CBCR);
	qti_8x8x_core_clk_enable(phydev, QTI_8X8X_EPHY1_SYS_CBCR);
	qti_8x8x_core_clk_enable(phydev, QTI_8X8X_EPHY2_SYS_CBCR);
	qti_8x8x_core_clk_enable(phydev, QTI_8X8X_EPHY3_SYS_CBCR);

	/* Reset ephy */
	qti_8x8x_core_clk_reset(phydev, QTI_8X8X_EPHY0_SYS_CBCR);
	qti_8x8x_core_clk_reset(phydev, QTI_8X8X_EPHY1_SYS_CBCR);
	qti_8x8x_core_clk_reset(phydev, QTI_8X8X_EPHY2_SYS_CBCR);
	qti_8x8x_core_clk_reset(phydev, QTI_8X8X_EPHY3_SYS_CBCR);

	/* Deassert EPHY DSP */
	val = qti_8x8x_mii_read(phydev, QTI_8X8X_GCC_GEPHY_MISC);
	val &= ~GENMASK(4, 0);
	qti_8x8x_mii_write(phydev, QTI_8X8X_GCC_GEPHY_MISC, val);

	/*for ES chips, need to load efuse manually*/
	val = qti_8x8x_mii_read(phydev, QTI_8X8X_QFPROM_RAW_PTE_ROW2_MSB);
	val = (val & GENMASK(23, 16)) >> 16;
	if(val == 1 || val == 2) {
		for(i = 0; i < 4; i++)
			qti_8x8x_efuse_loading(phydev, i);
	}

	/* Enable efuse loading into analog circuit */
	val = qti_8x8x_mii_read(phydev, QTI_8X8X_EPHY_CFG);
	/* BIT20 for PHY0 and PHY1, BIT21 for PHY2 and PHY3 */
	val &= ~GENMASK(21, 20);
	qti_8x8x_mii_write(phydev, QTI_8X8X_EPHY_CFG, val);

	udelay(11000);
}

static void qti_8x8x_addr_fixup(struct phy_device *phydev)
{
	int phy_index, addr;
	u32 val;
	unsigned long phyaddr_mask = 0;

	val = qti_8x8x_mii_read(phydev, QTI_8X8X_EPHY_CFG);

	for (phy_index = 0, addr = 1; addr <= 4; phy_index++, addr++) {
		phyaddr_mask |= BIT(addr);
		addr &= GENMASK(4, 0);
		val &= ~(GENMASK(4, 0) << (phy_index *
					QTI_8X8X_PHY_ADDR_LENGTH));
		val |= addr << (phy_index * QTI_8X8X_PHY_ADDR_LENGTH);
	}

	pr_dbg("programme EPHY reg 0x%x with 0x%x\n", QTI_8X8X_EPHY_CFG, val);
	qti_8x8x_mii_write(phydev, QTI_8X8X_EPHY_CFG, val);

	/* Programe the UNIPHY address if uniphyaddr_fixup specified.
	 * the UNIPHY address will select three MDIO address from
	 * unoccupied MDIO address space.
	 */
	val = qti_8x8x_mii_read(phydev, QTI_8X8X_UNIPHY_CFG);

	/* For qca8386, the switch occupies the other 16 MDIO address,
	 * for example, if the phy address is in the range of 0 to 15,
	 * the switch will occupy the MDIO address from 16 to 31.
	 */
	phyaddr_mask |= GENMASK(31, 16);

	phy_index = 0;

	for_each_clear_bit_from(addr, &phyaddr_mask, PHY_MAX_ADDR) {
		if (phy_index >= QTI_8X8X_UNIPHY_ADDR_NUM)
			break;
		val &= ~(GENMASK(4, 0) <<
				(phy_index * QTI_8X8X_PHY_ADDR_LENGTH));
		val |= addr << (phy_index * QTI_8X8X_PHY_ADDR_LENGTH);
		phy_index++;
	}

	if (phy_index < QTI_8X8X_UNIPHY_ADDR_NUM) {
		for_each_clear_bit(addr, &phyaddr_mask, PHY_MAX_ADDR) {
			if (phy_index >= QTI_8X8X_UNIPHY_ADDR_NUM)
				break;
			val &= ~(GENMASK(4, 0) <<
				(phy_index * QTI_8X8X_PHY_ADDR_LENGTH));
			val |= addr << (phy_index * QTI_8X8X_PHY_ADDR_LENGTH);
			phy_index++;
		}
	}

	pr_dbg("programme UNIPHY reg 0x%x with 0x%x\n",
			QTI_8X8X_UNIPHY_CFG, val);
	qti_8x8x_mii_write(phydev, QTI_8X8X_UNIPHY_CFG, val);
}

/***************************************************************************/
/* mdio_function end */
/***************************************************************************/

/***************************************************************************/
/* clock_function start */
/***************************************************************************/

/* 2 uniphy with rx and tx */
#define QTI_8X8X_UNIPHY_INSTANCE		2
#define QTI_8X8X_PORT_CLK_CBC_MAX		8

static uint64_t qti_8x8x_uniphy_raw_clock[QTI_8X8X_UNIPHY_INSTANCE * 2] = {0};

static const unsigned long qti_8x8x_switch_core_support_rates[] = {
	UQXGMII_SPEED_2500M_CLK,
};

static const unsigned long qti_8x8x_cpuport_clk_support_rates[] = {
	UQXGMII_SPEED_10M_CLK,
	UQXGMII_SPEED_100M_CLK,
	UQXGMII_SPEED_1000M_CLK,
	UQXGMII_SPEED_2500M_CLK,
};

static const unsigned long qti_8x8x_phyport_clk_support_rates[] = {
	UQXGMII_SPEED_10M_CLK,
	UQXGMII_SPEED_100M_CLK,
	UQXGMII_SPEED_1000M_CLK,
	UQXGMII_SPEED_2500M_CLK,
	UQXGMII_XPCS_SPEED_2500M_CLK,
};

static const unsigned long qti_8x8x_ahb_clk_support_rates[] = {
	QTI_8X8X_AHB_CLK_RATE_104P17M
};

static const unsigned long qti_8x8x_sys_clk_support_rates[] = {
	QTI_8X8X_SYS_CLK_RATE_25M,
};

static const struct qti_8x8x_parent_data qti_8x8x_switch_core_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX312P5M, 1 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac0_tx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 } ,
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY1_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX, 2 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac0_rx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 } ,
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY1_RX, 1 },
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY1_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX, 2 },
};

/* port 1, 2, 3 rx/tx clock have the same parents */
static const struct qti_8x8x_parent_data qti_8x8x_mac1_tx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 } ,
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX312P5M, 6 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_RX312P5M, 7 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac1_rx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX312P5M, 6 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac4_tx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX312P5M, 3 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_RX312P5M, 7 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac4_rx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX312P5M, 3 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac5_tx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY0_TX, 2 },
};

static const struct qti_8x8x_parent_data qti_8x8x_mac5_rx_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_1000M_CLK, QTI_8X8X_P_UNIPHY0_TX, 2 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY0_RX, 1 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY0_TX, 2 },
};

static const struct qti_8x8x_parent_data qti_8x8x_ahb_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
	{ UQXGMII_SPEED_2500M_CLK, QTI_8X8X_P_UNIPHY1_TX312P5M, 2 },
};

static const struct qti_8x8x_parent_data qti_8x8x_sys_clk_pdata[] = {
	{ QTI_8X8X_XO_CLK_RATE_50M, QTI_8X8X_P_XO, 0 },
};

static struct clk_lookup qti_8x8x_clk_lookup_table[] = {
	/* switch core clock */
	CLK_LOOKUP(4, 0, 8, CBCR_CLK_RESET,
			QTI_8X8X_SWITCH_CORE_CLK,
			qti_8x8x_switch_core_support_rates,
			ARRAY_SIZE(qti_8x8x_switch_core_support_rates),
			qti_8x8x_switch_core_pdata,
			ARRAY_SIZE(qti_8x8x_switch_core_pdata)),
	CLK_LOOKUP(4, 0, 0x10, CBCR_CLK_RESET,
			QTI_8X8X_APB_BRIDGE_CLK,
			qti_8x8x_switch_core_support_rates,
			ARRAY_SIZE(qti_8x8x_switch_core_support_rates),
			qti_8x8x_switch_core_pdata,
			ARRAY_SIZE(qti_8x8x_switch_core_pdata)),
	/* port 0 tx clock */
	CLK_LOOKUP(0x18, 0x1c, 0x20, CBCR_CLK_RESET,
			QTI_8X8X_MAC0_TX_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac0_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac0_tx_clk_pdata)),
	CLK_LOOKUP(0x18, 0x1c, 0x24, CBCR_CLK_RESET,
			QTI_8X8X_MAC0_TX_UNIPHY1_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac0_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac0_tx_clk_pdata)),
	/* port 0 rx clock */
	CLK_LOOKUP(0x2c, 0x30, 0x34, CBCR_CLK_RESET,
			QTI_8X8X_MAC0_RX_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac0_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac0_rx_clk_pdata)),
	CLK_LOOKUP(0x2c, 0x30, 0x3c, CBCR_CLK_RESET,
			QTI_8X8X_MAC0_RX_UNIPHY1_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac0_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac0_rx_clk_pdata)),
	/* port 1 tx clock */
	CLK_LOOKUP(0x44, 0x48, 0x50, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_UNIPHY1_CH0_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x44, 0x48, 0x54, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x44, 0x48, 0x58, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_GEPHY0_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x44, 0x4c, 0x5c, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	/* port 1 rx clock */
	CLK_LOOKUP(0x64, 0x68, 0x70, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_UNIPHY1_CH0_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0x64, 0x68, 0x74, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0x64, 0x68, 0x78, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_GEPHY0_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0x64, 0x6c, 0x7c, CBCR_CLK_RESET,
			QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	/* port 2 tx clock */
	CLK_LOOKUP(0x84, 0x88, 0x90, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_UNIPHY1_CH1_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x84, 0x88, 0x94, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x84, 0x88, 0x98, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_GEPHY1_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0x84, 0x8c, 0x9c, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	/* port 2 rx clock */
	CLK_LOOKUP(0xa4, 0xa8, 0xb0, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_UNIPHY1_CH1_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xa4, 0xa8, 0xb4, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xa4, 0xa8, 0xb8, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_GEPHY1_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xa4, 0xac, 0xbc, CBCR_CLK_RESET,
			QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	/* port 3 tx clock */
	CLK_LOOKUP(0xc4, 0xc8, 0xd0, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_UNIPHY1_CH2_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0xc4, 0xc8, 0xd4, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0xc4, 0xc8, 0xd8, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_GEPHY2_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	CLK_LOOKUP(0xc4, 0xcc, 0xdc, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_tx_clk_pdata)),
	/* port 3 rx clock */
	CLK_LOOKUP(0xe4, 0xe8, 0xf0, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_UNIPHY1_CH2_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xe4, 0xe8, 0xf4, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xe4, 0xe8, 0xf8, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_GEPHY2_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	CLK_LOOKUP(0xe4, 0xec, 0xfc, CBCR_CLK_RESET,
			QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac1_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac1_rx_clk_pdata)),
	/* port 4 tx clock */
	CLK_LOOKUP(0x104, 0x108, 0x110, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_UNIPHY1_CH3_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_tx_clk_pdata)),
	CLK_LOOKUP(0x104, 0x108, 0x114, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_tx_clk_pdata)),
	CLK_LOOKUP(0x104, 0x108, 0x118, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_GEPHY3_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_tx_clk_pdata)),
	CLK_LOOKUP(0x104, 0x10c, 0x11c, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_tx_clk_pdata)),
	/* port 4 rx clock */
	CLK_LOOKUP(0x124, 0x128, 0x130, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_UNIPHY1_CH3_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_rx_clk_pdata)),
	CLK_LOOKUP(0x124, 0x128, 0x134, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_rx_clk_pdata)),
	CLK_LOOKUP(0x124, 0x128, 0x138, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_GEPHY3_RX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_rx_clk_pdata)),
	CLK_LOOKUP(0x124, 0x12c, 0x13c, CBCR_CLK_RESET,
			QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_TX_CLK,
			qti_8x8x_phyport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_phyport_clk_support_rates),
			qti_8x8x_mac4_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac4_rx_clk_pdata)),
	/* port 5 tx clock */
	CLK_LOOKUP(0x144, 0x148, 0x14c, CBCR_CLK_RESET,
			QTI_8X8X_MAC5_TX_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac5_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac5_tx_clk_pdata)),
	CLK_LOOKUP(0x144, 0x148, 0x150, CBCR_CLK_RESET,
			QTI_8X8X_MAC5_TX_UNIPHY0_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac5_tx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac5_tx_clk_pdata)),
	/* port 5 rx clock */
	CLK_LOOKUP(0x158, 0x15c, 0x160, CBCR_CLK_RESET,
			QTI_8X8X_MAC5_RX_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac5_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac5_rx_clk_pdata)),
	CLK_LOOKUP(0x158, 0x15c, 0x164, CBCR_CLK_RESET,
			QTI_8X8X_MAC5_RX_UNIPHY0_CLK,
			qti_8x8x_cpuport_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_cpuport_clk_support_rates),
			qti_8x8x_mac5_rx_clk_pdata,
			ARRAY_SIZE(qti_8x8x_mac5_rx_clk_pdata)),
	/* AHB bridge clock */
	CLK_LOOKUP(0x16c, 0, 0x170, CBCR_CLK_RESET,
			QTI_8X8X_AHB_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x174, CBCR_CLK_RESET,
			QTI_8X8X_SEC_CTRL_AHB_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x178, CBCR_CLK_RESET,
			QTI_8X8X_TLMM_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x190, CBCR_CLK_RESET,
			QTI_8X8X_TLMM_AHB_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x194, CBCR_CLK_RESET,
			QTI_8X8X_CNOC_AHB_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x198, CBCR_CLK_RESET,
			QTI_8X8X_MDIO_AHB_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	CLK_LOOKUP(0x16c, 0, 0x19c, CBCR_CLK_RESET,
			QTI_8X8X_MDIO_MASTER_AHB_CLK,
			qti_8x8x_ahb_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_ahb_clk_support_rates),
			qti_8x8x_ahb_clk_pdata,
			ARRAY_SIZE(qti_8x8x_ahb_clk_pdata)),
	/* SYS clock */
	CLK_LOOKUP(0x1a4, 0, 0x1a8, CBCR_CLK_RESET,
			QTI_8X8X_SRDS0_SYS_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1ac, CBCR_CLK_RESET,
			QTI_8X8X_SRDS1_SYS_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1b0, CBCR_CLK_RESET,
			QTI_8X8X_GEPHY0_SYS_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1b4, CBCR_CLK_RESET,
			QTI_8X8X_GEPHY1_SYS_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1b8, CBCR_CLK_RESET,
			QTI_8X8X_GEPHY2_SYS_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),
	CLK_LOOKUP(0x1a4, 0, 0x1bc, CBCR_CLK_RESET,
			QTI_8X8X_GEPHY3_SYS_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),

	/* SEC control clock */
	CLK_LOOKUP(0x1c4, 0, 0x1c8, CBCR_CLK_RESET,
			QTI_8X8X_SEC_CTRL_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),
	CLK_LOOKUP(0x1c4, 0, 0x1d0, CBCR_CLK_RESET,
			QTI_8X8X_SEC_CTRL_SENSE_CLK,
			qti_8x8x_sys_clk_support_rates,
			ARRAY_SIZE(qti_8x8x_sys_clk_support_rates),
			qti_8x8x_sys_clk_pdata,
			ARRAY_SIZE(qti_8x8x_sys_clk_pdata)),

	/* GEPHY reset */
	CLK_LOOKUP(0, 0, 0x304, BIT(0),
			QTI_8X8X_GEPHY_P0_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(1),
			QTI_8X8X_GEPHY_P1_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(2),
			QTI_8X8X_GEPHY_P2_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(3),
			QTI_8X8X_GEPHY_P3_MDC_SW_RST, NULL, 0, NULL, 0),
	CLK_LOOKUP(0, 0, 0x304, BIT(4),
			QTI_8X8X_GEPHY_DSP_HW_RST, NULL, 0, NULL, 0),

	/* Global reset */
	CLK_LOOKUP(0, 0, 0x308, BIT(0),
			QTI_8X8X_GLOBAL_RST, NULL, 0, NULL, 0),

	/* XPCS reset */
	CLK_LOOKUP(0, 0, 0x30c, BIT(0),
			QTI_8X8X_UNIPHY_XPCS_RST, NULL, 0, NULL, 0),
};

static struct clk_lookup *qti_8x8x_clk_find(const char *clock_id)
{
	int i;
	struct clk_lookup *clk;

	for (i = 0; i < ARRAY_SIZE(qti_8x8x_clk_lookup_table); i++) {
		clk = &qti_8x8x_clk_lookup_table[i];
		if (!strncmp(clock_id, clk->clk_name, strlen(clock_id)))
			return clk;
	}

	return NULL;
}

static void qti_8x8x_clk_update(struct phy_device *phydev, uint32_t cmd_reg)
{
	uint32_t i, reg_val;

	/* update RCG to the new programmed configuration */
	reg_val = qti_8x8x_mii_read(phydev, cmd_reg);
	reg_val |= RCGR_CMD_UPDATE;
	qti_8x8x_mii_write(phydev, cmd_reg, reg_val);

	for (i = 1000; i > 0; i--) {
		reg_val = qti_8x8x_mii_read(phydev, cmd_reg);
		if (!(reg_val & RCGR_CMD_UPDATE))
			return;

		udelay(1);
	}

	pr_dbg("CLK cmd reg 0x%x fails updating to "
			"new configurations\n", cmd_reg);
	return;
}

static void qti_8x8x_clk_assert(struct phy_device *phydev,
		const char *clock_id)
{
	struct clk_lookup *clk;
	uint32_t cbc_reg = 0;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return;
	}

	cbc_reg = QTI_8X8X_CLK_BASE_REG + clk->cbc;

	qti_8x8x_mii_update(phydev, cbc_reg, clk->rst_bit, clk->rst_bit);
	return;
}

static void qti_8x8x_clk_deassert(struct phy_device *phydev,
		const char *clock_id)
{
	struct clk_lookup *clk;
	uint32_t cbc_reg = 0;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return;
	}

	cbc_reg = QTI_8X8X_CLK_BASE_REG + clk->cbc;

	qti_8x8x_mii_update(phydev, cbc_reg, clk->rst_bit, 0);
	return;
}

static void qti_8x8x_clk_reset(struct phy_device *phydev,
		const char *clock_id)
{
	qti_8x8x_clk_assert(phydev, clock_id);

	/* Time required by HW to complete assert */
	udelay(10);

	qti_8x8x_clk_deassert(phydev, clock_id);
	return;
}

static uint8_t qti_8x8x_clk_is_enabled(struct phy_device *phydev,
		const char *clock_id)
{
	struct clk_lookup *clk;
	uint32_t cbc_reg = 0, reg_val = 0;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return 0;
	}

	cbc_reg = QTI_8X8X_CLK_BASE_REG + clk->cbc;
	reg_val = qti_8x8x_mii_read(phydev, cbc_reg);
	return ((reg_val & CBCR_CLK_OFF) ? 0 : 1);
}

static void qti_8x8x_clk_enable(struct phy_device *phydev,
		const char *clock_id)
{
	struct clk_lookup *clk;
	uint32_t cbc_reg = 0, reg_val = 0;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return;
	}

	cbc_reg = QTI_8X8X_CLK_BASE_REG + clk->cbc;
	qti_8x8x_mii_update(phydev, cbc_reg,
			CBCR_CLK_ENABLE, CBCR_CLK_ENABLE);

	udelay(1);
	reg_val = qti_8x8x_mii_read(phydev, cbc_reg);
	if (reg_val & CBCR_CLK_OFF) {
		pr_dbg("CLK %s is not enabled!\n", clock_id);
		return;
	}

	return;
}

static void qti_8x8x_clk_disable(struct phy_device *phydev,
		const char *clock_id)
{
	struct clk_lookup *clk;
	uint32_t cbc_reg = 0;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return;
	}

	cbc_reg = QTI_8X8X_CLK_BASE_REG + clk->cbc;

	qti_8x8x_mii_update(phydev, cbc_reg, CBCR_CLK_ENABLE, 0);
	return;
}

static void qti_8x8x_clk_parent_set(struct phy_device *phydev,
		const char *clock_id, qti_8x8x_clk_parent_t parent)
{
	struct clk_lookup *clk;
	uint32_t i, reg_val;
	uint32_t rcg_reg = 0, cmd_reg = 0, cfg = 0, cur_cfg = 0;
	const struct qti_8x8x_parent_data *pdata = NULL;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return;
	}

	for (i = 0; i < clk->num_parent; i++) {
		pdata = &(clk->pdata[i]);
		if (pdata->parent == parent)
			break;
	}

	if (i == clk->num_parent) {
		pr_dbg("CLK %s is configured as incorrect parent %d\n",
				clock_id, parent);
		return;
	}

	rcg_reg = QTI_8X8X_CLK_BASE_REG + clk->rcg;
	cmd_reg = QTI_8X8X_CLK_BASE_REG + clk->rcg - 4;

	reg_val = qti_8x8x_mii_read(phydev, rcg_reg);
	cur_cfg = (reg_val & RCGR_SRC_SEL) >> RCGR_SRC_SEL_SHIFT;
	cfg = pdata->cfg;

	if (cfg == cur_cfg) {
		pr_dbg("CLK %s parent %d is already configured correctly\n",
				clock_id, parent);
		return;
	}

	/* update clock parent */
	reg_val &= ~RCGR_SRC_SEL;
	reg_val |= cfg << RCGR_SRC_SEL_SHIFT;
	qti_8x8x_mii_write(phydev, rcg_reg, reg_val);

	/* update RCG to the new programmed configuration */
	qti_8x8x_clk_update(phydev, cmd_reg);
}

static void qti_8x8x_uniphy_raw_clock_set(qti_8x8x_clk_parent_t uniphy_clk,
		uint64_t rate)
{
	switch (uniphy_clk) {
		case QTI_8X8X_P_UNIPHY0_RX:
		case QTI_8X8X_P_UNIPHY0_TX:
		case QTI_8X8X_P_UNIPHY1_RX:
		case QTI_8X8X_P_UNIPHY1_TX:
			break;
		default:
			pr_dbg("Invalid uniphy_clk %d\n", uniphy_clk);
			return;
	}

	qti_8x8x_uniphy_raw_clock[uniphy_clk - QTI_8X8X_P_UNIPHY0_RX] = rate;
	return;
}

static uint64_t qti_8x8x_uniphy_raw_clock_get(qti_8x8x_clk_parent_t
		uniphy_clk)
{
	switch (uniphy_clk) {
		case QTI_8X8X_P_UNIPHY0_RX:
		case QTI_8X8X_P_UNIPHY0_TX:
		case QTI_8X8X_P_UNIPHY1_RX:
		case QTI_8X8X_P_UNIPHY1_TX:
			break;
		default:
			pr_dbg("Invalid uniphy_clk %d\n", uniphy_clk);
			return QTI_8X8X_XO_CLK_RATE_50M;
	}

	return qti_8x8x_uniphy_raw_clock[uniphy_clk - QTI_8X8X_P_UNIPHY0_RX];
}

static void qti_8x8x_clk_rate_set(struct phy_device *phydev,
		const char *clock_id, uint32_t rate)
{
	struct clk_lookup *clk;
	uint64_t div, prate = 0;
	uint32_t i, reg_val, parent_index = 0;
	uint32_t rcg_reg = 0, cmd_reg = 0, cdiv_reg = 0, cdiv_val = 0;
	const struct qti_8x8x_parent_data *pdata = NULL;

	clk = qti_8x8x_clk_find(clock_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clock_id);
		return;
	}

	for (i = 0; i < clk->num_rate; i++)
		if (rate == clk->support_rate[i])
			break;

	if (i == clk->num_rate) {
		pr_dbg("CLK %s does not support to configure rate %d\n",
				clock_id, rate);
		return;
	}

	rcg_reg = QTI_8X8X_CLK_BASE_REG + clk->rcg;
	cmd_reg = QTI_8X8X_CLK_BASE_REG + clk->rcg - 4;
	if (clk->cdiv != 0)
		cdiv_reg = QTI_8X8X_CLK_BASE_REG + clk->cdiv;

	reg_val = qti_8x8x_mii_read(phydev, rcg_reg);

	/* get the parent rate of clock */
	parent_index = (reg_val & RCGR_SRC_SEL) >> RCGR_SRC_SEL_SHIFT;
	for (i = 0; i < clk->num_parent; i++) {
		pdata = &(clk->pdata[i]);
		if (pdata->cfg == parent_index) {
			/* uniphy0 rx, tx and unphy1 rx, tx clock can be 125M
			 * or 312.5M, which depends on the current link speed,
			 * the clock rate needs to be acquired dynamically.
			 */
			switch (pdata->parent) {
			case QTI_8X8X_P_UNIPHY0_RX:
			case QTI_8X8X_P_UNIPHY0_TX:
			case QTI_8X8X_P_UNIPHY1_RX:
			case QTI_8X8X_P_UNIPHY1_TX:
				prate = qti_8x8x_uniphy_raw_clock_get(
						pdata->parent);
				break;
			default:
				/* XO 50M or 315P5M fix clock rate */
				prate = pdata->prate;
				break;
			}
			/* find the parent clock rate */
			break;
		}
	}

	if (i == clk->num_parent || prate == 0) {
		pr_dbg("CLK %s is configured as unsupported "
				"parent value %d\n", clock_id, parent_index);
		return;
	}

	/* when configuring XPSC clock to UQXGMII_XPCS_SPEED_2500M_CLK,
	 * the RCGR divider need to be bypassed, since there are two
	 * dividers from the same RCGR, one is for XPCS clock, the other
	 * is for EPHY port clock.
	 */
	if (rate == UQXGMII_XPCS_SPEED_2500M_CLK) {
		if (prate != UQXGMII_SPEED_2500M_CLK) {
			pr_dbg("CLK %s parent(%lld) needs to be "
					"updated to %d\n", clock_id,
					prate, UQXGMII_SPEED_2500M_CLK);
			return;
		}
		div = RCGR_DIV_BYPASS;
		cdiv_val = (UQXGMII_SPEED_2500M_CLK /
				UQXGMII_XPCS_SPEED_2500M_CLK) - 1;
	} else {

		/* calculate the RCGR divider
		 * prate/rate = (rcg_divider + 1)/2 */
		div = prate * 2;
		do_div(div, rate);
		div--;

		/* if the RCG divider can't meet the requirement,
		 * the CDIV reg can be simply divided by 10 to satisfy
		 * the required clock rate.
		 */
		if (div > RCGR_DIV_MAX) {
			/* update CDIV Reg to be divided by 10(N+1) */
			cdiv_val = CDIVR_DIVIDER_10;

			/* caculate the new RCG divider */
			do_div(prate, CDIVR_DIVIDER_10 + 1);
			div = prate * 2;
			do_div(div, rate);
			div--;
		}
	}

	/* update CDIV Reg to be divided by N(N-1 for reg value) */
	if (cdiv_reg != 0)
		qti_8x8x_mii_update(phydev, cdiv_reg,
				CDIVR_DIVIDER,
				cdiv_val << CDIVR_DIVIDER_SHIFT);

	if (cdiv_reg == 0 && cdiv_val > 0) {
		pr_dbg("CLK %s needs CDIVR to generate "
				"rate %d from prate %lld\n",
				clock_id, rate, prate);
		return;
	}

	/* update RCGR */
	reg_val &= ~RCGR_HDIV;
	reg_val |= div << RCGR_HDIV_SHIFT;
	qti_8x8x_mii_write(phydev, rcg_reg, reg_val);

	/* update RCG to the new programmed configuration */
	qti_8x8x_clk_update(phydev, cmd_reg);
}

#if 0
void qti_8x8x_port5_uniphy0_clk_src_get(uint8_t *bypass_en)
{
	uint32_t reg_val = 0;

	/* In switch mode, uniphy0 rx clock is from mac5 rx, uniphy0 tx clock is from mac5 tx;
	 * In bypass mode, uniphy0 rx clock is from mac4 tx, uniphy0 tx clock is from mac4 rx;
	 */
	reg_val = qti_8x8x_mii_read(phydev, QTI_8X8X_CLK_BASE_REG + QTI_8X8X_CLK_MUX_SEL);
	*bypass_en = (reg_val & QTI_8X8X_UNIPHY0_SEL_MAC5) ? 0 : 1;

	return;
}

int qti_8x8x_clk_rate_get(const char *clock_id,
			  struct qti_8x8x_clk_data *clk_data)
{
	struct clk_lookup *clk;
	uint64_t div, prate = 0;
	uint32_t i, reg_val, parent_index = 0;
	const struct qti_8x8x_parent_data *pdata = NULL;
	char clk_id[64] = {0};
	uint8_t bypass_en = 0;

	strlcpy(clk_id, clock_id, sizeof(clk_id));

	qti_8x8x_port5_uniphy0_clk_src_get(&bypass_en);
	if (bypass_en == 1) {
		if (strncasecmp(clock_id, QTI_8X8X_MAC5_TX_UNIPHY0_CLK,
					strlen(QTI_8X8X_MAC5_TX_UNIPHY0_CLK)) == 0)
			strlcpy(clk_id, QTI_8X8X_MAC4_RX_CLK, sizeof(clk_id));
		else if (strncasecmp(clock_id, QTI_8X8X_MAC5_RX_UNIPHY0_CLK,
					strlen(QTI_8X8X_MAC5_RX_UNIPHY0_CLK)) == 0)
			strlcpy(clk_id, QTI_8X8X_MAC4_TX_CLK, sizeof(clk_id));
	}

	clk = qti_8x8x_clk_find(clk_id);
	if (!clk) {
		pr_dbg("CLK %s is not found!\n", clk_id);
		return -1;
	}

	reg_val = qti_8x8x_mii_read(phydev, QTI_8X8X_CLK_BASE_REG + clk->rcg);

	/* get the parent rate of clock */
	parent_index = (reg_val & RCGR_SRC_SEL) >> RCGR_SRC_SEL_SHIFT;
	for (i = 0; i < clk->num_parent; i++) {
		pdata = &(clk->pdata[i]);
		if (pdata->cfg == parent_index) {
			/* uniphy0 rx, tx and unphy1 rx, tx clock can be 125M or 312.5M, which
			 * depends on the current link speed, the clock rate needs to be acquired
			 * dynamically.
			 */
			switch (pdata->parent) {
				case QTI_8X8X_P_UNIPHY0_RX:
				case QTI_8X8X_P_UNIPHY0_TX:
				case QTI_8X8X_P_UNIPHY1_RX:
				case QTI_8X8X_P_UNIPHY1_TX:
					prate = qti_8x8x_uniphy_raw_clock_get(pdata->parent);
					break;
				default:
					/* XO 50M or 315P5M fix clock rate */
					prate = pdata->prate;
					break;
			}
			/* find the parent clock rate */
			break;
		}
	}

	if (i == clk->num_parent || prate == 0) {
		pr_dbg("CLK %s is configured as unsupported parent value %d\n",
				clk_id, parent_index);
		return -1;
	}

	/* calculate the current clock rate */
	div = (reg_val >> RCGR_HDIV_SHIFT) & RCGR_HDIV;
	if (div != 0) {
		/* RCG divider is bypassed if the div value is 0 */
		prate *= 2;
		do_div(prate, div + 1);
	}

	clk_data->rcg_val = reg_val;

	reg_val = qti_8x8x_mii_read(phydev, QTI_8X8X_CLK_BASE_REG + clk->cbc);
	clk_data->cbc_val = reg_val;

	if (clk->cdiv != 0) {
		reg_val = qti_8x8x_mii_read(phydev, QTI_8X8X_CLK_BASE_REG + clk->cdiv);
		clk_data->cdiv_val = reg_val;
		do_div(prate, ((reg_val >> CDIVR_DIVIDER_SHIFT) & CDIVR_DIVIDER) + 1);
	}

	clk_data->rate = prate;

	return 0;
}

void qti_8x8x_clk_dump(void)
{
	uint32_t i;
	struct clk_lookup *clk;
	struct qti_8x8x_clk_data clk_data;
	int ret;

	printf("%-31s  Frequency  RCG_VAL  CDIV_VAL  CBC_VAL\n", "Clock Name");

	for (i = 0; i < ARRAY_SIZE(qti_8x8x_clk_lookup_table); i++) {
		clk = &qti_8x8x_clk_lookup_table[i];
		if (clk->rcg != 0) {
			ret = qti_8x8x_clk_rate_get(clk->clk_name, &clk_data);
			if (ret != 0)
				continue;
			printf("%-31s  %-9ld  0x%-5x  0x%-6x  0x%-5x\n",
				clk->clk_name, clk_data.rate,
				clk_data.rcg_val, clk_data.cdiv_val, clk_data.cbc_val);
		}
	}
}

static int do_qti_8x8x_clk_dump(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	qti_8x8x_clk_dump();

	return 0;
}

U_BOOT_CMD(
	qti_8x8x_clk_dump, 1, 1, do_qti_8x8x_clk_dump,
	"QTI_8X8X utility command to dump clocks\n",
	"qti_8x8x_clk_dump	- dump all the qti_8x8x clocks\n"
	"This command can be used to check if clocks are all as expected\n"
);
#endif

static void qti_8x8x_port5_uniphy0_clk_src_set(struct phy_device *phydev,
		uint8_t bypass_en)
{
	uint32_t mux_sel = 0;

	/* In switch mode,
	 *	uniphy0 rx clock is from mac5 rx,
	 *	uniphy0 tx clock is from mac5 tx;
	 *
	 * In bypass mode,
	 *	uniphy0 rx clock is from mac4 tx,
	 *	uniphy0 tx clock is from mac4 rx;
	 */

	if (bypass_en)
		mux_sel = QTI_8X8X_UNIPHY0_SEL_MAC4;
	else
		mux_sel = QTI_8X8X_UNIPHY0_SEL_MAC5;

	qti_8x8x_mii_update(phydev,
			QTI_8X8X_CLK_BASE_REG + QTI_8X8X_CLK_MUX_SEL,
			QTI_8X8X_UNIPHY0_MUX_SEL_MASK, mux_sel);
	return;
}

static void qti_8x8x_port_clk_rate_set(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, uint32_t rate)
{
	char *mac_rx_clk = NULL, *mac_tx_clk = NULL;
	char *xgmii_tx_clk = NULL, *xgmii_rx_clk = NULL;

	switch (qti_8x8x_port_id) {
	case DEV_8X8X_PORT_0:
		mac_rx_clk = QTI_8X8X_MAC0_RX_CLK;
		mac_tx_clk = QTI_8X8X_MAC0_TX_CLK;
		break;
	case DEV_8X8X_PORT_1:
		mac_rx_clk = QTI_8X8X_MAC1_RX_CLK;
		mac_tx_clk = QTI_8X8X_MAC1_TX_CLK;
		xgmii_rx_clk = QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_RX_CLK;
		xgmii_tx_clk = QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_TX_CLK;
		break;
	case DEV_8X8X_PORT_2:
		mac_rx_clk = QTI_8X8X_MAC2_RX_CLK;
		mac_tx_clk = QTI_8X8X_MAC2_TX_CLK;
		xgmii_rx_clk = QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_RX_CLK;
		xgmii_tx_clk = QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_TX_CLK;
		break;
	case DEV_8X8X_PORT_3:
		mac_rx_clk = QTI_8X8X_MAC3_RX_CLK;
		mac_tx_clk = QTI_8X8X_MAC3_TX_CLK;
		xgmii_rx_clk = QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_RX_CLK;
		xgmii_tx_clk = QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_TX_CLK;
		break;
	case DEV_8X8X_PORT_4:
		mac_rx_clk = QTI_8X8X_MAC4_RX_CLK;
		mac_tx_clk = QTI_8X8X_MAC4_TX_CLK;
		xgmii_rx_clk = QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_RX_CLK;
		xgmii_tx_clk = QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_TX_CLK;
		break;
	case DEV_8X8X_PORT_5:
		mac_rx_clk = QTI_8X8X_MAC5_RX_CLK;
		mac_tx_clk = QTI_8X8X_MAC5_TX_CLK;
		break;
	default:
		pr_dbg("Unsupported qti_8x8x_port_id %d\n",
				qti_8x8x_port_id);
		return;
	}

	qti_8x8x_clk_rate_set(phydev, mac_rx_clk, rate);
	qti_8x8x_clk_rate_set(phydev, mac_tx_clk, rate);

	if (xgmii_rx_clk != NULL && xgmii_tx_clk != NULL) {
		/* XGMII take the different clock rate
		 * from MAC clock when the link speed is 2.5G.
		 */
		if (rate == UQXGMII_SPEED_2500M_CLK)
			rate = UQXGMII_XPCS_SPEED_2500M_CLK;
		qti_8x8x_clk_rate_set(phydev, xgmii_rx_clk, rate);
		qti_8x8x_clk_rate_set(phydev, xgmii_tx_clk, rate);
	}

	return;
}

static void qti_8x8x_clk_ids_get(uint32_t qti_8x8x_port_id,
		uint8_t mask, char **clk_ids)
{
	switch (qti_8x8x_port_id) {
	case DEV_8X8X_PORT_0:
		if (mask & QTI_8X8X_CLK_TYPE_MAC) {
			*clk_ids++ = QTI_8X8X_MAC0_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC0_RX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_UNIPHY) {
			*clk_ids++ = QTI_8X8X_MAC0_TX_UNIPHY1_CLK;
			*clk_ids++ = QTI_8X8X_MAC0_RX_UNIPHY1_CLK;
		}
		break;
	case DEV_8X8X_PORT_1:
		if (mask & QTI_8X8X_CLK_TYPE_MAC) {
			*clk_ids++ = QTI_8X8X_MAC1_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC1_RX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_UNIPHY) {
			*clk_ids++ = QTI_8X8X_MAC1_UNIPHY1_CH0_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC1_UNIPHY1_CH0_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_TX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_EPHY) {
			*clk_ids++ = QTI_8X8X_MAC1_GEPHY0_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC1_GEPHY0_RX_CLK;
		}
		break;
	case DEV_8X8X_PORT_2:
		if (mask & QTI_8X8X_CLK_TYPE_MAC) {
			*clk_ids++ = QTI_8X8X_MAC2_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC2_RX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_UNIPHY) {
			*clk_ids++ = QTI_8X8X_MAC2_UNIPHY1_CH1_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC2_UNIPHY1_CH1_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_TX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_EPHY) {
			*clk_ids++ = QTI_8X8X_MAC2_GEPHY1_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC2_GEPHY1_RX_CLK;
		}
		break;
	case DEV_8X8X_PORT_3:
		if (mask & QTI_8X8X_CLK_TYPE_MAC) {
			*clk_ids++ = QTI_8X8X_MAC3_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC3_RX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_UNIPHY) {
			*clk_ids++ = QTI_8X8X_MAC3_UNIPHY1_CH2_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC3_UNIPHY1_CH2_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_TX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_EPHY) {
			*clk_ids++ = QTI_8X8X_MAC3_GEPHY2_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC3_GEPHY2_RX_CLK;
		}
		break;
	case DEV_8X8X_PORT_4:
		if (mask & QTI_8X8X_CLK_TYPE_MAC) {
			*clk_ids++ = QTI_8X8X_MAC4_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC4_RX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_UNIPHY) {
			*clk_ids++ = QTI_8X8X_MAC4_UNIPHY1_CH3_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC4_UNIPHY1_CH3_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_RX_CLK;
			*clk_ids++ = QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_TX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_EPHY) {
			*clk_ids++ = QTI_8X8X_MAC4_GEPHY3_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC4_GEPHY3_RX_CLK;
		}
		break;
	case DEV_8X8X_PORT_5:
		if (mask & QTI_8X8X_CLK_TYPE_MAC) {
			*clk_ids++ = QTI_8X8X_MAC5_TX_CLK;
			*clk_ids++ = QTI_8X8X_MAC5_RX_CLK;
		}

		if (mask & QTI_8X8X_CLK_TYPE_UNIPHY) {
			*clk_ids++ = QTI_8X8X_MAC5_TX_UNIPHY0_CLK;
			*clk_ids++ = QTI_8X8X_MAC5_RX_UNIPHY0_CLK;
		}
		break;
	default:
		pr_dbg("Unsupported qti_8x8x_port_id %d\n",
				qti_8x8x_port_id);
		return;
	}

	return;
}

static void qti_8x8x_port_clk_reset(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, uint8_t mask)
{
	char *clk_ids[QTI_8X8X_PORT_CLK_CBC_MAX + 1] = {NULL};
	uint32_t i = 0;

	qti_8x8x_clk_ids_get(qti_8x8x_port_id, mask, clk_ids);

	while(clk_ids[i] != NULL) {
		qti_8x8x_clk_reset(phydev, clk_ids[i]);
		i++;
	}

	return;
}

static void qti_8x8x_port_clk_en_set(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, uint8_t mask, uint8_t enable)
{
	char *clk_ids[QTI_8X8X_PORT_CLK_CBC_MAX + 1] = {NULL};
	uint32_t i = 0;

	qti_8x8x_clk_ids_get(qti_8x8x_port_id, mask, clk_ids);

	while(clk_ids[i] != NULL) {
		if (enable)
			qti_8x8x_clk_enable(phydev, clk_ids[i]);
		else
			qti_8x8x_clk_disable(phydev, clk_ids[i]);
		i++;
	}

	return;
}

static void qti_8x8x_gcc_common_clk_parent_enable(struct phy_device *phydev,
		qti_8x8x_work_mode_t clk_mode)
{
	/* Switch core */
	qti_8x8x_clk_parent_set(phydev, QTI_8X8X_SWITCH_CORE_CLK,
			QTI_8X8X_P_UNIPHY1_TX312P5M);
	qti_8x8x_clk_rate_set(phydev, QTI_8X8X_SWITCH_CORE_CLK,
			UQXGMII_SPEED_2500M_CLK);

	/* Disable switch core clock to save power in phy mode */
	if (QTI_8X8X_PHY_UQXGMII_MODE == clk_mode ||
			QTI_8X8X_PHY_SGMII_UQXGMII_MODE == clk_mode)
		qti_8x8x_clk_disable(phydev, QTI_8X8X_SWITCH_CORE_CLK);
	else
		qti_8x8x_clk_enable(phydev, QTI_8X8X_SWITCH_CORE_CLK);

	qti_8x8x_clk_enable(phydev, QTI_8X8X_APB_BRIDGE_CLK);

	/* AHB bridge */
	qti_8x8x_clk_parent_set(phydev, QTI_8X8X_AHB_CLK,
			QTI_8X8X_P_UNIPHY1_TX312P5M);
	qti_8x8x_clk_rate_set(phydev, QTI_8X8X_AHB_CLK,
			QTI_8X8X_AHB_CLK_RATE_104P17M);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_AHB_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_SEC_CTRL_AHB_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_TLMM_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_TLMM_AHB_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_CNOC_AHB_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_MDIO_AHB_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_MDIO_MASTER_AHB_CLK);

	/* System */
	qti_8x8x_clk_parent_set(phydev, QTI_8X8X_SRDS0_SYS_CLK,
			QTI_8X8X_P_XO);
	qti_8x8x_clk_rate_set(phydev, QTI_8X8X_SRDS0_SYS_CLK,
			QTI_8X8X_SYS_CLK_RATE_25M);

	/* Disable serdes0 clock to save power in phy mode */
	if (QTI_8X8X_PHY_UQXGMII_MODE == clk_mode ||
			QTI_8X8X_PHY_SGMII_UQXGMII_MODE == clk_mode)
		qti_8x8x_clk_disable(phydev, QTI_8X8X_SRDS0_SYS_CLK);
	else
		qti_8x8x_clk_enable(phydev, QTI_8X8X_SRDS0_SYS_CLK);

	qti_8x8x_clk_enable(phydev, QTI_8X8X_SRDS1_SYS_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_GEPHY0_SYS_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_GEPHY1_SYS_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_GEPHY2_SYS_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_GEPHY3_SYS_CLK);

	/* Sec control */
	qti_8x8x_clk_parent_set(phydev, QTI_8X8X_SEC_CTRL_CLK,
			QTI_8X8X_P_XO);
	qti_8x8x_clk_rate_set(phydev, QTI_8X8X_SEC_CTRL_CLK,
			QTI_8X8X_SYS_CLK_RATE_25M);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_SEC_CTRL_CLK);
	qti_8x8x_clk_enable(phydev, QTI_8X8X_SEC_CTRL_SENSE_CLK);
}

static void qti_8x8x_gcc_port_clk_parent_set(struct phy_device *phydev,
		qti_8x8x_work_mode_t clk_mode, uint32_t qti_8x8x_port_id)
{
	qti_8x8x_clk_parent_t port_tx_parent, port_rx_parent;
	char *tx_clk_id, *rx_clk_id;

	/* Initialize the clock parent with port 1, 2, 3,
	 * clock parent is same for these ports;
	 * the clock parent will be updated for port 0, 4, 5.
	 */
	switch(clk_mode) {
	case QTI_8X8X_SWITCH_MODE:
	case QTI_8X8X_SWITCH_BYPASS_PORT5_MODE:
		port_tx_parent = QTI_8X8X_P_UNIPHY1_TX312P5M;
		break;
	case QTI_8X8X_PHY_UQXGMII_MODE:
	case QTI_8X8X_PHY_SGMII_UQXGMII_MODE:
		port_tx_parent = QTI_8X8X_P_UNIPHY1_RX312P5M;
		break;
	default:
		pr_dbg("Unsupported clock mode %d\n", clk_mode);
		return;
	}
	port_rx_parent = QTI_8X8X_P_UNIPHY1_TX312P5M;

	switch (qti_8x8x_port_id) {
	case DEV_8X8X_PORT_0:
		port_tx_parent = QTI_8X8X_P_UNIPHY1_TX;
		port_rx_parent = QTI_8X8X_P_UNIPHY1_RX;
		tx_clk_id = QTI_8X8X_MAC0_TX_CLK;
		rx_clk_id = QTI_8X8X_MAC0_RX_CLK;
		break;
	case DEV_8X8X_PORT_1:
		tx_clk_id = QTI_8X8X_MAC1_TX_CLK;
		rx_clk_id = QTI_8X8X_MAC1_RX_CLK;
		break;
	case DEV_8X8X_PORT_2:
		tx_clk_id = QTI_8X8X_MAC2_TX_CLK;
		rx_clk_id = QTI_8X8X_MAC2_RX_CLK;
		break;
	case DEV_8X8X_PORT_3:
		tx_clk_id = QTI_8X8X_MAC3_TX_CLK;
		rx_clk_id = QTI_8X8X_MAC3_RX_CLK;
		break;
	case DEV_8X8X_PORT_4:
		switch(clk_mode) {
		case QTI_8X8X_SWITCH_BYPASS_PORT5_MODE:
		case QTI_8X8X_PHY_SGMII_UQXGMII_MODE:
			port_tx_parent = QTI_8X8X_P_UNIPHY0_RX;
			port_rx_parent = QTI_8X8X_P_UNIPHY0_TX;
			break;
		case QTI_8X8X_SWITCH_MODE:
			port_tx_parent = QTI_8X8X_P_UNIPHY1_TX312P5M;
			port_rx_parent = QTI_8X8X_P_UNIPHY1_TX312P5M;
			break;
		case QTI_8X8X_PHY_UQXGMII_MODE:
			port_tx_parent = QTI_8X8X_P_UNIPHY1_RX312P5M;
			port_rx_parent = QTI_8X8X_P_UNIPHY1_TX312P5M;
			break;
		default:
			pr_dbg("Unsupported clock mode %d\n", clk_mode);
			return;
		}
		tx_clk_id = QTI_8X8X_MAC4_TX_CLK;
		rx_clk_id = QTI_8X8X_MAC4_RX_CLK;
		break;
	case DEV_8X8X_PORT_5:
		port_tx_parent = QTI_8X8X_P_UNIPHY0_TX;
		port_rx_parent = QTI_8X8X_P_UNIPHY0_RX;
		tx_clk_id = QTI_8X8X_MAC5_TX_CLK;
		rx_clk_id = QTI_8X8X_MAC5_RX_CLK;
		switch (clk_mode) {
		case QTI_8X8X_SWITCH_BYPASS_PORT5_MODE:
		case QTI_8X8X_PHY_SGMII_UQXGMII_MODE:
			qti_8x8x_port5_uniphy0_clk_src_set(phydev, 1);
			break;
		case QTI_8X8X_SWITCH_MODE:
		case QTI_8X8X_PHY_UQXGMII_MODE:
			qti_8x8x_port5_uniphy0_clk_src_set(phydev, 0);
			break;
		default:
			pr_dbg("Unsupported clock mode %d\n", clk_mode);
			return;
		}
		break;
	default:
		pr_dbg("Unsupported qti_8x8x_port_id %d\n",
				qti_8x8x_port_id);
		return;
	}

	qti_8x8x_clk_parent_set(phydev, tx_clk_id, port_tx_parent);
	qti_8x8x_clk_parent_set(phydev, rx_clk_id, port_rx_parent);
}

static int qti_8x8x_gcc_clock_init(struct phy_device *phydev,
		qti_8x8x_work_mode_t clk_mode, u32 pbmp)
{
	uint32_t qti_8x8x_port_id = 0;
	/* clock type mask value for 6 manhattan ports */
	uint8_t clk_mask[DEV_8X8X_PORT_5 + 1] = {0};
	uint8_t switch_flag = 0;
	qti_8x8x_clk_parent_t uniphy_index = QTI_8X8X_P_UNIPHY0_RX;

	switch (clk_mode) {
	case QTI_8X8X_SWITCH_MODE:
	case QTI_8X8X_SWITCH_BYPASS_PORT5_MODE:
		while (pbmp) {
			if (pbmp & 1) {
				if (qti_8x8x_port_id == DEV_8X8X_PORT_0 ||
					qti_8x8x_port_id == DEV_8X8X_PORT_5) {
					clk_mask[qti_8x8x_port_id] =
						QTI_8X8X_CLK_TYPE_MAC |
					QTI_8X8X_CLK_TYPE_UNIPHY;
				} else {
					clk_mask[qti_8x8x_port_id] =
						QTI_8X8X_CLK_TYPE_MAC |
						QTI_8X8X_CLK_TYPE_EPHY;
				}
			}
			pbmp >>= 1;
			qti_8x8x_port_id++;
		}

		if (clk_mode == QTI_8X8X_SWITCH_BYPASS_PORT5_MODE) {
			/* For phy port 4 in switch bypass mode */
			clk_mask[DEV_8X8X_PORT_4] = QTI_8X8X_CLK_TYPE_EPHY;
			clk_mask[DEV_8X8X_PORT_5] = QTI_8X8X_CLK_TYPE_UNIPHY;
		}

		switch_flag = 1;
		break;
	case QTI_8X8X_PHY_UQXGMII_MODE:
	case QTI_8X8X_PHY_SGMII_UQXGMII_MODE:
		clk_mask[DEV_8X8X_PORT_1] = QTI_8X8X_CLK_TYPE_UNIPHY |
			QTI_8X8X_CLK_TYPE_EPHY;
		clk_mask[DEV_8X8X_PORT_2] = QTI_8X8X_CLK_TYPE_UNIPHY |
			QTI_8X8X_CLK_TYPE_EPHY;
		clk_mask[DEV_8X8X_PORT_3] = QTI_8X8X_CLK_TYPE_UNIPHY |
			QTI_8X8X_CLK_TYPE_EPHY;
		clk_mask[DEV_8X8X_PORT_4] = QTI_8X8X_CLK_TYPE_UNIPHY |
			QTI_8X8X_CLK_TYPE_EPHY;
		if (clk_mode == QTI_8X8X_PHY_SGMII_UQXGMII_MODE) {
			/* For phy port4 in PHY bypass mode */
			clk_mask[DEV_8X8X_PORT_4] = QTI_8X8X_CLK_TYPE_EPHY;
			clk_mask[DEV_8X8X_PORT_5] = QTI_8X8X_CLK_TYPE_UNIPHY;
		}
		break;
	default:
		pr_dbg("Unsupported clock mode %d\n", clk_mode);
		return -EINVAL;
	}

	qti_8x8x_gcc_common_clk_parent_enable(phydev, clk_mode);

	/* Initialize the uniphy raw clock,
	 * if the port4 is in bypass mode, the uniphy0 raw clock
	 * need to be dynamically updated between
	 * UQXGMII_SPEED_2500M_CLK and UQXGMII_SPEED_1000M_CLK
	 * according to the realtime link speed.
	 */
	uniphy_index = QTI_8X8X_P_UNIPHY0_RX;
	while (uniphy_index <= QTI_8X8X_P_UNIPHY1_TX) {
		/* the uniphy raw clock may be already initialized. */
		if (0 == qti_8x8x_uniphy_raw_clock_get(uniphy_index))
			qti_8x8x_uniphy_raw_clock_set(uniphy_index,
					UQXGMII_SPEED_2500M_CLK);
		uniphy_index++;
	}

	qti_8x8x_port_id = 0;
	while (qti_8x8x_port_id < ARRAY_SIZE(clk_mask)) {
		if (clk_mask[qti_8x8x_port_id] != 0) {
			qti_8x8x_gcc_port_clk_parent_set(phydev,
					clk_mode, qti_8x8x_port_id);
			if (clk_mask[qti_8x8x_port_id] &
					QTI_8X8X_CLK_TYPE_MAC)
				qti_8x8x_port_clk_en_set(phydev,
						qti_8x8x_port_id,
						QTI_8X8X_CLK_TYPE_MAC, 1);
			if (clk_mask[qti_8x8x_port_id] &
					QTI_8X8X_CLK_TYPE_UNIPHY
					&& switch_flag == 1)
				qti_8x8x_port_clk_en_set(phydev,
						qti_8x8x_port_id,
						QTI_8X8X_CLK_TYPE_UNIPHY, 1);
			pbmp |= BIT(qti_8x8x_port_id);
		}
		qti_8x8x_port_id++;
	}

	pr_dbg("QTI_8X8X GCC CLK initialization with clock mode %d "
			"on port bmp 0x%x\n", clk_mode, pbmp);
	return 0;
}

/***************************************************************************/
/* clock_function end */
/***************************************************************************/

/***************************************************************************/
/* itf_function start */
/***************************************************************************/

static void qti_8x8x_serdes_addr_get(struct phy_device *phydev,
		uint32_t serdes_id, uint32_t *address)
{
	uint32_t data = 0;

	data = qti_8x8x_mii_read(phydev, SERDES_CFG_OFFSET);
	switch(serdes_id)
	{
		case QTI_8X8X_UNIPHY_SGMII_0:
			*address = (data >> SERDES_CFG_S0_ADDR_BOFFSET)
				& 0x1f;
			break;
		case QTI_8X8X_UNIPHY_SGMII_1:
			*address = (data >> SERDES_CFG_S1_ADDR_BOFFSET)
				& 0x1f;
			break;
		case QTI_8X8X_UNIPHY_XPCS:
			*address = (data >> SERDES_CFG_S1_XPCS_ADDR_BOFFSET)
				& 0x1f;
			break;
		default:
			pr_dbg("Serdes id not matching\n");
			break;
	}
}

static void qti_8x8x_uniphy_calibration(struct phy_device *phydev,
		uint32_t uniphy_addr)
{
	uint16_t uniphy_data = 0;
	uint32_t retries = 100, calibration_done = 0;

	/* wait calibration done to uniphy*/
	while (calibration_done != QTI_8X8X_UNIPHY_MMD1_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0)
			pr_dbg("uniphy callibration time out!\n");
		uniphy_data = qti_8x8x_phy_mmd_read(phydev,
				uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
				QTI_8X8X_UNIPHY_MMD1_CALIBRATION4);

		calibration_done = (uniphy_data &
				QTI_8X8X_UNIPHY_MMD1_CALIBRATION_DONE);
	}
}

static void qti_8x8x_port_speed_clock_set(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, u32 speed)
{
	uint32_t clk_rate = 0;

	switch(speed) {
	case SPEED_2500:
		clk_rate = UQXGMII_SPEED_2500M_CLK;
		break;
	case SPEED_1000:
		clk_rate = UQXGMII_SPEED_1000M_CLK;
		break;
	case SPEED_100:
		clk_rate = UQXGMII_SPEED_100M_CLK;
		break;
	case SPEED_10:
		clk_rate = UQXGMII_SPEED_10M_CLK;
		break;
	default:
		pr_dbg("Unknown speed\n");
		return;
	}

	qti_8x8x_port_clk_rate_set(phydev, qti_8x8x_port_id, clk_rate);
}

static void qti_8x8x_ephy_addr_get(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, uint32_t *phy_addr)
{
	uint32_t data = 0;

	data = qti_8x8x_mii_read(phydev, EPHY_CFG_OFFSET);
	switch(qti_8x8x_port_id) {
	case DEV_8X8X_PORT_1:
		*phy_addr = (data >> EPHY_CFG_EPHY0_ADDR_BOFFSET) & 0x1f;
		break;
	case DEV_8X8X_PORT_2:
		*phy_addr = (data >> EPHY_CFG_EPHY1_ADDR_BOFFSET) & 0x1f;
		break;
	case DEV_8X8X_PORT_3:
		*phy_addr = (data >> EPHY_CFG_EPHY2_ADDR_BOFFSET) & 0x1f;
		break;
	case DEV_8X8X_PORT_4:
		*phy_addr = (data >> EPHY_CFG_EPHY3_ADDR_BOFFSET) & 0x1f;
		break;
	default:
		pr_dbg("qti_8x8x_port_id not matching\n");
		break;
	}
}

static uint16_t qti_8x8x_uniphy_xpcs_mmd_read(struct phy_device *phydev,
		uint16_t mmd_num, uint16_t mmd_reg)
{
	uint32_t uniphy_xpcs_addr = 0;

	qti_8x8x_serdes_addr_get(phydev,
			QTI_8X8X_UNIPHY_XPCS, &uniphy_xpcs_addr);
	return qti_8x8x_phy_mmd_read(phydev,
			uniphy_xpcs_addr, mmd_num, mmd_reg);
}

static void qti_8x8x_uniphy_xpcs_mmd_write(struct phy_device *phydev,
		uint16_t mmd_num, uint16_t mmd_reg, uint16_t reg_val)
{
	uint32_t uniphy_xpcs_addr = 0;
#ifdef DEBUG
	uint16_t phy_data = 0;
#endif

	qti_8x8x_serdes_addr_get(phydev,
			QTI_8X8X_UNIPHY_XPCS, &uniphy_xpcs_addr);

	qti_8x8x_phy_mmd_write(phydev,
			uniphy_xpcs_addr, mmd_num, mmd_reg, reg_val);
	/*check the mmd register value*/
#ifdef DEBUG
	phy_data =
#endif
		qti_8x8x_uniphy_xpcs_mmd_read(phydev, mmd_num, mmd_reg);

	pr_dbg("phy_addr:0x%x, mmd_num:0x%x, mmd_reg:0x%x, phy_data:0x%x\n",
		uniphy_xpcs_addr, mmd_num, mmd_reg, phy_data);
}

static void qti_8x8x_uniphy_xpcs_modify_mmd(struct phy_device *phydev,
		uint32_t mmd_num, uint32_t mmd_reg,
		uint32_t mask, uint32_t value)
{
	uint16_t phy_data = 0, new_phy_data = 0;

	phy_data = qti_8x8x_uniphy_xpcs_mmd_read(phydev, mmd_num, mmd_reg);
	new_phy_data = (phy_data & ~mask) | value;
	qti_8x8x_uniphy_xpcs_mmd_write(phydev, mmd_num,
			mmd_reg, new_phy_data);
}

static uint32_t qti_8x8x_uniphy_xpcs_port_to_mmd(uint32_t qti_8x8x_port_id)
{
	uint32_t mmd_id = 0;

	switch(qti_8x8x_port_id) {
	case DEV_8X8X_PORT_1:
		mmd_id = QTI_8X8X_UNIPHY_MMD31;
		break;
	case DEV_8X8X_PORT_2:
		mmd_id = QTI_8X8X_UNIPHY_MMD26;
		break;
	case DEV_8X8X_PORT_3:
		mmd_id = QTI_8X8X_UNIPHY_MMD27;
		break;
	case DEV_8X8X_PORT_4:
		mmd_id = QTI_8X8X_UNIPHY_MMD28;
		break;
	default:
		pr_dbg("Port not matching qti_8x8x ports\n");
	}

	return mmd_id;
}

static void qti_8x8x_uniphy_xpcs_modify_port_mmd(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, uint32_t mmd_reg,
		uint32_t mask, uint32_t value)
{
	uint32_t mmd_id = 0;

	mmd_id = qti_8x8x_uniphy_xpcs_port_to_mmd(qti_8x8x_port_id);
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, mmd_id,
			mmd_reg, mask, value);
}

static void qti_8x8x_uniphy_xpcs_8023az_enable(struct phy_device *phydev)
{
	uint16_t uniphy_data = 0;

	uniphy_data = qti_8x8x_uniphy_xpcs_mmd_read(phydev,
			QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_AN_LP_BASE_ABL2);
	if(!(uniphy_data & QTI_8X8X_UNIPHY_MMD3_XPCS_EEE_CAP))
		return;

	/*Configure the EEE related timer*/
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev,	QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_EEE_MODE_CTRL, 0x0f40,
			QTI_8X8X_UNIPHY_MMD3_EEE_RES_REGS |
			QTI_8X8X_UNIPHY_MMD3_EEE_SIGN_BIT_REGS);

	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_EEE_TX_TIMER, 0x1fff,
			QTI_8X8X_UNIPHY_MMD3_EEE_TSL_REGS |
			QTI_8X8X_UNIPHY_MMD3_EEE_TLU_REGS |
			QTI_8X8X_UNIPHY_MMD3_EEE_TWL_REGS);

	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_EEE_RX_TIMER, 0x1fff,
			QTI_8X8X_UNIPHY_MMD3_EEE_100US_REG_REGS |
			QTI_8X8X_UNIPHY_MMD3_EEE_RWR_REG_REGS);

	/*enable TRN_LPI*/
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_EEE_MODE_CTRL1, 0x101,
			QTI_8X8X_UNIPHY_MMD3_EEE_TRANS_LPI_MODE |
			QTI_8X8X_UNIPHY_MMD3_EEE_TRANS_RX_LPI_MODE);

	/*enable TX/RX LPI pattern*/
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_EEE_MODE_CTRL, 0x3,
			QTI_8X8X_UNIPHY_MMD3_EEE_EN);
}

static void qti_8x8x_uniphy_xpcs_10g_r_linkup(struct phy_device *phydev)
{
	uint16_t uniphy_data = 0;
	uint32_t retries = 100, linkup = 0;

	/* wait 10G_R link up */
	while (linkup != QTI_8X8X_UNIPHY_MMD3_10GBASE_R_UP) {
		mdelay(1);
		if (retries-- == 0)
			pr_dbg("10g_r link up timeout\n");
		uniphy_data = qti_8x8x_uniphy_xpcs_mmd_read(phydev,
				QTI_8X8X_UNIPHY_MMD3,
				QTI_8X8X_UNIPHY_MMD3_10GBASE_R_PCS_STATUS1);

		linkup = (uniphy_data & QTI_8X8X_UNIPHY_MMD3_10GBASE_R_UP);
	}
}

static void qti_8x8x_uniphy_xpcs_soft_reset(struct phy_device *phydev)
{
	uint16_t uniphy_data = 0;
	uint32_t retries = 100,
		 reset_done = QTI_8X8X_UNIPHY_MMD3_XPCS_SOFT_RESET;

	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD3_DIG_CTRL1, 0x8000,
			QTI_8X8X_UNIPHY_MMD3_XPCS_SOFT_RESET);

	while (reset_done) {
		mdelay(1);
		if (retries-- == 0)
			pr_dbg("xpcs soft reset timeout\n");
		uniphy_data = qti_8x8x_uniphy_xpcs_mmd_read(phydev,
				QTI_8X8X_UNIPHY_MMD3,
				QTI_8X8X_UNIPHY_MMD3_DIG_CTRL1);

		reset_done = (uniphy_data &
				QTI_8X8X_UNIPHY_MMD3_XPCS_SOFT_RESET);
	}
}

static void qti_8x8x_uniphy_xpcs_speed_set(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id, u32 speed)
{
	uint32_t xpcs_speed = 0;

	switch(speed) {
	case SPEED_2500:
		xpcs_speed = QTI_8X8X_UNIPHY_MMD_XPC_SPEED_2500;
		break;
	case SPEED_1000:
		xpcs_speed = QTI_8X8X_UNIPHY_MMD_XPC_SPEED_1000;
		break;
	case SPEED_100:
		xpcs_speed = QTI_8X8X_UNIPHY_MMD_XPC_SPEED_100;
		break;
	case SPEED_10:
		xpcs_speed = QTI_8X8X_UNIPHY_MMD_XPC_SPEED_10;
		break;
	default:
		pr_dbg("Unknown speed\n");
		return;
	}
	qti_8x8x_uniphy_xpcs_modify_port_mmd(phydev, qti_8x8x_port_id,
			QTI_8X8X_UNIPHY_MMD_MII_CTRL,
			QTI_8X8X_UNIPHY_MMD_XPC_SPEED_MASK,
			xpcs_speed);
}

static void qti_8x8x_uniphy_uqxgmii_function_reset(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id)
{
	uint32_t uniphy_addr = 0;

	qti_8x8x_serdes_addr_get(phydev, QTI_8X8X_UNIPHY_SGMII_1,
			&uniphy_addr);

	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_USXGMII_RESET,
		BIT(qti_8x8x_port_id-1), 0);
	mdelay(1);
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_USXGMII_RESET, BIT(qti_8x8x_port_id-1),
		BIT(qti_8x8x_port_id-1));
	if(qti_8x8x_port_id == DEV_8X8X_PORT_1)
		qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
			QTI_8X8X_UNIPHY_MMD_MII_DIG_CTRL,
			0x400, QTI_8X8X_UNIPHY_MMD3_USXG_FIFO_RESET);
	else
		qti_8x8x_uniphy_xpcs_modify_port_mmd(phydev, qti_8x8x_port_id,
			QTI_8X8X_UNIPHY_MMD_MII_DIG_CTRL,
			0x20, QTI_8X8X_UNIPHY_MMD_USXG_FIFO_RESET);
}

static void qti_8x8x_uniphy_xpcs_autoneg_restart(struct phy_device *phydev,
		uint32_t qti_8x8x_port_id)
{
	uint32_t retries = 500, uniphy_data = 0, mmd_id = 0;

	mmd_id = qti_8x8x_uniphy_xpcs_port_to_mmd(qti_8x8x_port_id);
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, mmd_id,
			QTI_8X8X_UNIPHY_MMD_MII_CTRL,
			QTI_8X8X_UNIPHY_MMD_MII_AN_RESTART,
			QTI_8X8X_UNIPHY_MMD_MII_AN_RESTART);
	mdelay(1);
	uniphy_data = qti_8x8x_uniphy_xpcs_mmd_read(phydev, mmd_id,
			QTI_8X8X_UNIPHY_MMD_MII_ERR_SEL);
	while(!(uniphy_data & QTI_8X8X_UNIPHY_MMD_MII_AN_COMPLETE_INT))
	{
		mdelay(1);
		if (retries-- == 0)
		{
			pr_dbg("xpcs uniphy autoneg restart timeout\n");
		}
		uniphy_data = qti_8x8x_uniphy_xpcs_mmd_read(phydev, mmd_id,
			QTI_8X8X_UNIPHY_MMD_MII_ERR_SEL);
	}
}

static void _qti_8x8x_interface_uqxgmii_mode_set(struct phy_device *phydev,
		uint32_t uniphy_addr)
{
	uint32_t qti_8x8x_port_id = 0, phy_addr = 0;

	/*reset xpcs*/
	pr_dbg("reset xpcs\n");
	qti_8x8x_clk_assert(phydev, QTI_8X8X_UNIPHY_XPCS_RST);
	/*select xpcs mode*/
	pr_dbg("select xpcs mode\n");
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_MODE_CTRL, 0x1f00,
		QTI_8X8X_UNIPHY_MMD1_XPCS_MODE);
	/*config dapa pass as usxgmii*/
	pr_dbg("config dapa pass as usxgmii\n");
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_GMII_DATAPASS_SEL,
		QTI_8X8X_UNIPHY_MMD1_DATAPASS_MASK,
		QTI_8X8X_UNIPHY_MMD1_DATAPASS_USXGMII);
	/*reset and release uniphy GMII/XGMII and ethphy GMII*/
	pr_dbg("reset and release uniphy GMII/XGMII and ethphy GMII\n");
	for(qti_8x8x_port_id = DEV_8X8X_PORT_1;
			qti_8x8x_port_id <= DEV_8X8X_PORT_4;
			qti_8x8x_port_id++)
	{
		qti_8x8x_port_clk_reset(phydev, qti_8x8x_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY|QTI_8X8X_CLK_TYPE_EPHY);
	}

	/*ana sw reset and release*/
	pr_dbg("ana sw reset and release\n");
	qti_8x8x_phy_modify_mii(phydev, uniphy_addr,
		QTI_8X8X_UNIPHY_PLL_POWER_ON_AND_RESET, 0x40,
		QTI_8X8X_UNIPHY_ANA_SOFT_RESET);
	mdelay(10);
	qti_8x8x_phy_modify_mii(phydev, uniphy_addr,
		QTI_8X8X_UNIPHY_PLL_POWER_ON_AND_RESET, 0x40,
		QTI_8X8X_UNIPHY_ANA_SOFT_RELEASE);

	/*Wait calibration done*/
	pr_dbg("Wait calibration done\n");
	qti_8x8x_uniphy_calibration(phydev, uniphy_addr);
	/*Enable SSCG(Spread Spectrum Clock Generator)*/
	pr_dbg("enable uniphy sscg\n");
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_CDA_CONTROL1, 0x8,
		QTI_8X8X_UNIPHY_MMD1_SSCG_ENABLE);
	/*release XPCS*/
	pr_dbg("release XPCS\n");
	qti_8x8x_clk_deassert(phydev, QTI_8X8X_UNIPHY_XPCS_RST);
	/*ethphy software reset*/
	pr_dbg("ethphy software reset\n");
	for(qti_8x8x_port_id = DEV_8X8X_PORT_1;
			qti_8x8x_port_id <= DEV_8X8X_PORT_4;
			qti_8x8x_port_id++)
	{
		qti_8x8x_ephy_addr_get(phydev, qti_8x8x_port_id, &phy_addr);
		qti_8x8x_phy_reset(phydev, phy_addr);
	}
	/*Set BaseR mode*/
	pr_dbg("Set BaseR mode\n");
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
		QTI_8X8X_UNIPHY_MMD3_PCS_CTRL2, 0xf,
		QTI_8X8X_UNIPHY_MMD3_PCS_TYPE_10GBASE_R);
	/*wait 10G base_r link up*/
	pr_dbg("wait 10G base_r link up\n");
	qti_8x8x_uniphy_xpcs_10g_r_linkup(phydev);
	/*enable UQXGMII mode*/
	pr_dbg("enable UQSXGMII mode\n");
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
		QTI_8X8X_UNIPHY_MMD3_DIG_CTRL1, 0x200,
		QTI_8X8X_UNIPHY_MMD3_USXGMII_EN);
	/*set UQXGMII mode*/
	pr_dbg("set QXGMII mode\n");
	qti_8x8x_uniphy_xpcs_modify_mmd(phydev, QTI_8X8X_UNIPHY_MMD3,
		QTI_8X8X_UNIPHY_MMD3_VR_RPCS_TPC, 0x1c00,
		QTI_8X8X_UNIPHY_MMD3_QXGMII_EN);
	/*set AM interval*/
	pr_dbg("set AM interval\n");
	qti_8x8x_uniphy_xpcs_mmd_write(phydev, QTI_8X8X_UNIPHY_MMD3,
		QTI_8X8X_UNIPHY_MMD3_MII_AM_INTERVAL,
		QTI_8X8X_UNIPHY_MMD3_MII_AM_INTERVAL_VAL);
	/*xpcs software reset*/
	pr_dbg("xpcs software reset\n");
	qti_8x8x_uniphy_xpcs_soft_reset(phydev);
}

static void qti_8x8x_interface_uqxgmii_mode_set(struct phy_device *phydev)
{
	uint32_t uniphy_addr = 0, qti_8x8x_port_id = 0;

	qti_8x8x_serdes_addr_get(phydev,
			QTI_8X8X_UNIPHY_SGMII_1, &uniphy_addr);

	/*disable IPG_tuning bypass*/
	pr_dbg("disable IPG_tuning bypass\n");
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr,
			QTI_8X8X_UNIPHY_MMD1,
			QTI_8X8X_UNIPHY_MMD1_BYPASS_TUNING_IPG,
			QTI_8X8X_UNIPHY_MMD1_BYPASS_TUNING_IPG_EN, 0);
	/*disable uniphy GMII/XGMII clock and disable ethphy GMII clock*/
	pr_dbg("disable uniphy GMII/XGMII clock and ethphy GMII clock\n");
	for(qti_8x8x_port_id = DEV_8X8X_PORT_1;
			qti_8x8x_port_id <= DEV_8X8X_PORT_4;
			qti_8x8x_port_id++)
	{
		qti_8x8x_port_clk_en_set(phydev, qti_8x8x_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY|QTI_8X8X_CLK_TYPE_EPHY, 0);
	}
	/*configure uqxgmii mode*/
	pr_dbg("configure uqxgmii mode\n");
	_qti_8x8x_interface_uqxgmii_mode_set(phydev, uniphy_addr);
	/*enable auto-neg complete interrupt,Mii using mii-4bits,
		configure as PHY mode, enable autoneg ability*/
	pr_dbg("enable auto-neg complete interrupt, "
		"Mii using mii-4bits, configure as PHY mode, "
		"enable autoneg ability, disable TICD\n");
	for (qti_8x8x_port_id = DEV_8X8X_PORT_1;
			qti_8x8x_port_id <= DEV_8X8X_PORT_4;
			qti_8x8x_port_id++)
	{
		/* enable auto-neg complete interrupt,
		 * Mii using mii-4bits,configure as PHY mode*/
		qti_8x8x_uniphy_xpcs_modify_port_mmd(phydev,
			qti_8x8x_port_id,
			QTI_8X8X_UNIPHY_MMD_MII_AN_INT_MSK, 0x109,
			QTI_8X8X_UNIPHY_MMD_AN_COMPLETE_INT |
			QTI_8X8X_UNIPHY_MMD_MII_4BITS_CTRL |
			QTI_8X8X_UNIPHY_MMD_TX_CONFIG_CTRL);

		/*enable autoneg ability*/
		qti_8x8x_uniphy_xpcs_modify_port_mmd(phydev,
			qti_8x8x_port_id,
			QTI_8X8X_UNIPHY_MMD_MII_CTRL, 0x3060,
			QTI_8X8X_UNIPHY_MMD_MII_AN_ENABLE |
			QTI_8X8X_UNIPHY_MMD_XPC_SPEED_1000);

		/*disable TICD*/
		qti_8x8x_uniphy_xpcs_modify_port_mmd(phydev,
			qti_8x8x_port_id,
			QTI_8X8X_UNIPHY_MMD_MII_XAUI_MODE_CTRL, 0x1,
			QTI_8X8X_UNIPHY_MMD_TX_IPG_CHECK_DISABLE);
	}

	/*enable EEE for xpcs*/
	pr_dbg("enable EEE for xpcs\n");
	qti_8x8x_uniphy_xpcs_8023az_enable(phydev);
}

static void qti_8x8x_uniphy_sgmii_function_reset(struct phy_device *phydev,
		u32 uniphy_index)
{
	u32 uniphy_addr = 0;

	qti_8x8x_serdes_addr_get(phydev, uniphy_index, &uniphy_addr);

	/*sgmii channel0 adpt reset*/
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_CHANNEL0_CFG,
		QTI_8X8X_UNIPHY_MMD1_SGMII_ADPT_RESET, 0);
	mdelay(1);
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_CHANNEL0_CFG,
		QTI_8X8X_UNIPHY_MMD1_SGMII_ADPT_RESET,
		QTI_8X8X_UNIPHY_MMD1_SGMII_ADPT_RESET);
	/*ipg tune reset*/
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_USXGMII_RESET,
		QTI_8X8X_UNIPHY_MMD1_SGMII_FUNC_RESET, 0);
	mdelay(1);
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_USXGMII_RESET,
		QTI_8X8X_UNIPHY_MMD1_SGMII_FUNC_RESET,
		QTI_8X8X_UNIPHY_MMD1_SGMII_FUNC_RESET);
}

static void qti_8x8x_interface_sgmii_mode_set(struct phy_device *phydev,
		u32 uniphy_index, u32 qti_8x8x_port_id, mac_config_t *config)
{
	u32 uniphy_addr = 0, mode_ctrl = 0, speed_mode = 0;
	u32 uniphy_port_id = 0, ethphy_clk_mask = 0;
	u64 raw_clk = 0;

	/*get the uniphy address*/
	qti_8x8x_serdes_addr_get(phydev, uniphy_index, &uniphy_addr);
	if(config->mac_mode == QTI_8X8X_MAC_MODE_SGMII)
	{
		mode_ctrl = QTI_8X8X_UNIPHY_MMD1_SGMII_MODE;
		raw_clk = UNIPHY_CLK_RATE_125M;
	}
	else
	{
		mode_ctrl = QTI_8X8X_UNIPHY_MMD1_SGMII_PLUS_MODE;
		raw_clk = UNIPHY_CLK_RATE_312M;
	}

	if(config->clock_mode == QTI_8X8X_INTERFACE_CLOCK_MAC_MODE)
		mode_ctrl |= QTI_8X8X_UNIPHY_MMD1_SGMII_MAC_MODE;
	else
	{
		mode_ctrl |= QTI_8X8X_UNIPHY_MMD1_SGMII_PHY_MODE;
		/*eththy clock should be accessed for phy mode*/
		ethphy_clk_mask = QTI_8X8X_CLK_TYPE_EPHY;
	}

	pr_dbg("uniphy:%d,mode:%s,autoneg_en:%d,speed:%d,clk_mask:0x%x\n",
		uniphy_index,
		(config->mac_mode == QTI_8X8X_MAC_MODE_SGMII) ?
		"sgmii":"sgmii plus",
		config->auto_neg, config->speed,
		ethphy_clk_mask);

	/* GMII interface clock disable */
	pr_dbg("GMII interface clock disable\n");
	qti_8x8x_port_clk_en_set(phydev, qti_8x8x_port_id,
			ethphy_clk_mask, 0);

	/* when access uniphy0 clock, port5 should be used,
	 * but for phy mode, the port 4 connect to uniphy0,
	 * so need to change the port id
	 */
	if(uniphy_index == QTI_8X8X_UNIPHY_SGMII_0)
		uniphy_port_id = DEV_8X8X_PORT_5;
	else
		uniphy_port_id = qti_8x8x_port_id;

	qti_8x8x_port_clk_en_set(phydev, uniphy_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY, 0);

	/*uniphy1 xpcs reset, and configure raw clk*/
	if(uniphy_index == QTI_8X8X_UNIPHY_SGMII_1)
	{
		pr_dbg("uniphy1 xpcs reset, confiugre raw clock as:%lld\n",
			raw_clk);
		qti_8x8x_clk_assert(phydev, QTI_8X8X_UNIPHY_XPCS_RST);
		qti_8x8x_uniphy_raw_clock_set(
				QTI_8X8X_P_UNIPHY1_RX, raw_clk);
		qti_8x8x_uniphy_raw_clock_set(
				QTI_8X8X_P_UNIPHY1_TX, raw_clk);
	}
	else
	{
		pr_dbg("uniphy0 configure raw clock as %lld\n", raw_clk);
		qti_8x8x_uniphy_raw_clock_set(
				QTI_8X8X_P_UNIPHY0_RX, raw_clk);
		qti_8x8x_uniphy_raw_clock_set(
				QTI_8X8X_P_UNIPHY0_TX, raw_clk);
	}

	/*configure SGMII mode or SGMII+ mode*/
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_MODE_CTRL,
		QTI_8X8X_UNIPHY_MMD1_SGMII_MODE_CTRL_MASK,
		mode_ctrl);

	/*GMII datapass selection, 0 is for SGMII, 1 is for USXGMII*/
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_GMII_DATAPASS_SEL,
		QTI_8X8X_UNIPHY_MMD1_DATAPASS_MASK,
		QTI_8X8X_UNIPHY_MMD1_DATAPASS_SGMII);
	/*configue force or autoneg*/
	if(!config->auto_neg)
	{
		qti_8x8x_port_speed_clock_set(phydev, qti_8x8x_port_id,
			config->speed);
		switch (config->speed) {
		case SPEED_10:
			speed_mode = QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_ENABLE |
				QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_10M;
			break;
		case SPEED_100:
			speed_mode = QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_ENABLE |
				QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_100M;
			break;
		case SPEED_1000:
		case SPEED_2500:
			speed_mode = QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_ENABLE |
				QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_1G;
			break;
		default:
			break;
		}
	}
	else
	{
		speed_mode = QTI_8X8X_UNIPHY_MMD1_CH0_AUTONEG_ENABLE;
	}
	qti_8x8x_phy_modify_mmd(phydev, uniphy_addr, QTI_8X8X_UNIPHY_MMD1,
		QTI_8X8X_UNIPHY_MMD1_CHANNEL0_CFG,
		QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_MASK, speed_mode);

	/*GMII interface clock reset and release\n*/
	pr_dbg("GMII interface clock reset and release\n");
	qti_8x8x_port_clk_reset(phydev, qti_8x8x_port_id, ethphy_clk_mask);
	qti_8x8x_port_clk_reset(phydev, uniphy_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY);

	/*analog software reset and release*/
	pr_dbg("analog software reset and release\n");
	qti_8x8x_phy_modify_mii(phydev, uniphy_addr,
		QTI_8X8X_UNIPHY_PLL_POWER_ON_AND_RESET, 0x40,
		QTI_8X8X_UNIPHY_ANA_SOFT_RESET);
	mdelay(1);
	qti_8x8x_phy_modify_mii(phydev, uniphy_addr,
		QTI_8X8X_UNIPHY_PLL_POWER_ON_AND_RESET, 0x40,
		QTI_8X8X_UNIPHY_ANA_SOFT_RELEASE);

	/*wait uniphy calibration done*/
	pr_dbg("wait uniphy calibration done\n");
	qti_8x8x_uniphy_calibration(phydev, uniphy_addr);

	/*GMII interface clock enable*/
	pr_dbg("GMII interface clock enable\n");
	qti_8x8x_port_clk_en_set(phydev, qti_8x8x_port_id,
			ethphy_clk_mask, 1);
	qti_8x8x_port_clk_en_set(phydev, uniphy_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY, 1);

	return;
}

static int8_t qti_8x8x_uniphy_mode_check(struct phy_device *phydev,
		uint32_t uniphy_index, qti_8x8x_uniphy_mode_t uniphy_mode)
{
	uint32_t uniphy_addr = 0;
	uint16_t uniphy_mode_ctrl_data = 0;

	qti_8x8x_serdes_addr_get(phydev, uniphy_index, &uniphy_addr);

	uniphy_mode_ctrl_data = qti_8x8x_phy_mmd_read(phydev, uniphy_addr,
		QTI_8X8X_UNIPHY_MMD1, QTI_8X8X_UNIPHY_MMD1_MODE_CTRL);
	if(uniphy_mode_ctrl_data == PHY_INVALID_DATA)
		return 0;

	if(!(uniphy_mode & uniphy_mode_ctrl_data))
		return 0;

	return 1;
}

/***************************************************************************/
/* itf_function end */
/***************************************************************************/

/***************************************************************************/
/* core_function start */
/***************************************************************************/

bool qti_8x8x_port_txfc_forcemode[DEV_8X8X_MAX_PORTS] = {};
bool qti_8x8x_port_rxfc_forcemode[DEV_8X8X_MAX_PORTS] = {};

static void qti_8x8x_phy_function_reset(struct phy_device *phydev,
		uint32_t phy_id)
{
	uint16_t phy_data = 0;

	phy_data = qti_8x8x_phy_reg_read(phydev, phy_id,
			QTI_8X8X_PHY_FIFO_CONTROL);

	qti_8x8x_phy_reg_write(phydev, phy_id,
			QTI_8X8X_PHY_FIFO_CONTROL,
			phy_data & (~QTI_8X8X_PHY_FIFO_RESET));

	mdelay(50);

	qti_8x8x_phy_reg_write(phydev, phy_id,
			QTI_8X8X_PHY_FIFO_CONTROL,
			phy_data | QTI_8X8X_PHY_FIFO_RESET);
}

/**************************** QTI_8X8X Pinctrl APIs *************************/
/****************************************************************************
 * 1) PINs default Setting
 ***************************************************************************/
#ifdef IN_PINCTRL_DEF_CONFIG
static u64 pin_configs[] = {
	QTI_8X8X_PIN_CONFIG_OUTPUT_ENABLE,
	QTI_8X8X_PIN_CONFIG_BIAS_PULL_DOWN,
};
#endif

static struct qti_8x8x_pinctrl_setting qti_8x8x_pin_settings[] = {
	/*PINs default MUX Setting*/
	QTI_8X8X_PIN_SETTING_MUX(0,  QTI_8X8X_PIN_FUNC_INTN_WOL),
	QTI_8X8X_PIN_SETTING_MUX(1,  QTI_8X8X_PIN_FUNC_INTN),
	QTI_8X8X_PIN_SETTING_MUX(2,  QTI_8X8X_PIN_FUNC_P0_LED_0),
	QTI_8X8X_PIN_SETTING_MUX(3,  QTI_8X8X_PIN_FUNC_P1_LED_0),
	QTI_8X8X_PIN_SETTING_MUX(4,  QTI_8X8X_PIN_FUNC_P2_LED_0),
	QTI_8X8X_PIN_SETTING_MUX(5,  QTI_8X8X_PIN_FUNC_P3_LED_0),
	QTI_8X8X_PIN_SETTING_MUX(6,  QTI_8X8X_PIN_FUNC_PPS_IN),
	QTI_8X8X_PIN_SETTING_MUX(7,  QTI_8X8X_PIN_FUNC_TOD_IN),
	QTI_8X8X_PIN_SETTING_MUX(8,  QTI_8X8X_PIN_FUNC_RTC_REFCLK_IN),
	QTI_8X8X_PIN_SETTING_MUX(9,  QTI_8X8X_PIN_FUNC_P0_PPS_OUT),
	QTI_8X8X_PIN_SETTING_MUX(10, QTI_8X8X_PIN_FUNC_P1_PPS_OUT),
	QTI_8X8X_PIN_SETTING_MUX(11, QTI_8X8X_PIN_FUNC_P2_PPS_OUT),
	QTI_8X8X_PIN_SETTING_MUX(12, QTI_8X8X_PIN_FUNC_P3_PPS_OUT),
	QTI_8X8X_PIN_SETTING_MUX(13, QTI_8X8X_PIN_FUNC_P0_TOD_OUT),
	QTI_8X8X_PIN_SETTING_MUX(14, QTI_8X8X_PIN_FUNC_P0_CLK125_TDI),
	QTI_8X8X_PIN_SETTING_MUX(15, QTI_8X8X_PIN_FUNC_P0_SYNC_CLKO_PTP),
	QTI_8X8X_PIN_SETTING_MUX(16, QTI_8X8X_PIN_FUNC_P0_LED_1),
	QTI_8X8X_PIN_SETTING_MUX(17, QTI_8X8X_PIN_FUNC_P1_LED_1),
	QTI_8X8X_PIN_SETTING_MUX(18, QTI_8X8X_PIN_FUNC_P2_LED_1),
	QTI_8X8X_PIN_SETTING_MUX(19, QTI_8X8X_PIN_FUNC_P3_LED_1),
	QTI_8X8X_PIN_SETTING_MUX(20, QTI_8X8X_PIN_FUNC_MDC_M),
	QTI_8X8X_PIN_SETTING_MUX(21, QTI_8X8X_PIN_FUNC_MDC_M),

#ifdef IN_PINCTRL_DEF_CONFIG
	/*PINs default Config Setting*/
	QTI_8X8X_PIN_SETTING_CONFIG(0,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(1,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(2,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(3,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(4,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(5,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(6,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(7,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(8,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(9,  pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(10, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(11, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(12, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(13, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(14, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(15, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(16, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(17, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(18, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(19, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(20, pin_configs),
	QTI_8X8X_PIN_SETTING_CONFIG(21, pin_configs),
#endif
};

/****************************************************************************
 * 2) PINs Operations
 ****************************************************************************/
static int qti_8x8x_gpio_set_bit(struct phy_device *phydev,
		u32 pin, u32 value)
{
	int rv = 0;

	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_GPIO_IN_OUTN,
			pin, GPIO_OUTE, (u8 *) (&value));
	pr_dbg("[%s] select pin:%d value:%d\n", __func__, pin, value);
	return rv;
}

static int qti_8x8x_gpio_pin_mux_set(struct phy_device *phydev,
		u32 pin, u32 func)
{
	int rv = 0;

	pr_dbg("[%s] select pin:%d func:%d\n", __func__, pin, func);
	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_GPIO_CFGN,
			pin, FUNC_SEL, (u8 *) (&func));
	return rv;
}

static int qti_8x8x_gpio_pin_cfg_set_bias(struct phy_device *phydev,
		u32 pin, enum qti_8x8x_pin_config_param bias)
{
	int rv = 0;
	u32 data = 0;

	switch (bias) {
	case QTI_8X8X_PIN_CONFIG_BIAS_DISABLE:
		data = QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_DISABLE;
		break;
	case QTI_8X8X_PIN_CONFIG_BIAS_PULL_DOWN:
		data = QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_DOWN;
		break;
	case QTI_8X8X_PIN_CONFIG_BIAS_BUS_HOLD:
		data = QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_BUS_HOLD;
		break;
	case QTI_8X8X_PIN_CONFIG_BIAS_PULL_UP:
		data = QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_UP;
		break;
	default:
		printf("[%s] doesn't support bias:%d\n", __func__, bias);
		return -1;
	}

	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_GPIO_CFGN, pin, GPIO_PULL,
			(u8 *) (&data));
	pr_dbg("[%s]pin:%d bias:%d", __func__, pin, bias);
	return rv;
}

static int qti_8x8x_gpio_pin_cfg_set_drvs(struct phy_device *phydev,
		u32 pin, u32 drvs)
{
	int rv = 0;

	if((drvs < QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_2_MA) ||
			(drvs > QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_16_MA))
	{
		printf("[%s] doesn't support drvs:%d\n", __func__, drvs);
		return -1;
	}

	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_GPIO_CFGN, pin, DRV_STRENGTH,
			(u8 *) (&drvs));
	pr_dbg("[%s]%d", __func__, pin);
	return rv;
}

static int qti_8x8x_gpio_pin_cfg_set_oe(struct phy_device *phydev,
		u32 pin, bool oe)
{
	int rv = 0;

	pr_dbg("[%s]%d oe:%d", __func__, pin, oe);
	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_GPIO_CFGN, pin, GPIO_OEA,
			(u8 *) (&oe));
	return rv;
}

static enum qti_8x8x_pin_config_param pinconf_to_config_param(
		unsigned long config)
{
	return (enum qti_8x8x_pin_config_param) (config & 0xffUL);
}

static u32 pinconf_to_config_argument(unsigned long config)
{
	return (u32) ((config >> 8) & 0xffffffUL);
}

static int qti_8x8x_gpio_pin_cfg_set(struct phy_device *phydev, u32 pin,
		u64 *configs, u32 num_configs)
{
	enum qti_8x8x_pin_config_param param;
	u32 i, arg;
	int rv = 0;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case QTI_8X8X_PIN_CONFIG_BIAS_BUS_HOLD:
		case QTI_8X8X_PIN_CONFIG_BIAS_DISABLE:
		case QTI_8X8X_PIN_CONFIG_BIAS_PULL_DOWN:
		case QTI_8X8X_PIN_CONFIG_BIAS_PULL_UP:
			rv = qti_8x8x_gpio_pin_cfg_set_bias(phydev,
					pin, param);
			break;

		case QTI_8X8X_PIN_CONFIG_DRIVE_STRENGTH:
			rv = qti_8x8x_gpio_pin_cfg_set_drvs(phydev,
					pin, arg);
			break;

		case QTI_8X8X_PIN_CONFIG_OUTPUT:
			rv = qti_8x8x_gpio_pin_cfg_set_oe(phydev, pin, true);
			rv = qti_8x8x_gpio_set_bit(phydev, pin, arg);
			break;

		case QTI_8X8X_PIN_CONFIG_INPUT_ENABLE:
			rv = qti_8x8x_gpio_pin_cfg_set_oe(phydev, pin, false);
			break;

		case QTI_8X8X_PIN_CONFIG_OUTPUT_ENABLE:
			rv = qti_8x8x_gpio_pin_cfg_set_oe(phydev, pin, true);
			break;

		default:
			printf("%s %d doesn't support:%d \n",
					__func__, __LINE__, param);
			return -EINVAL;
		}
	}

	return rv;
}


/****************************************************************************
 * 3) PINs Init
 ****************************************************************************/
static int qti_8x8x_pinctrl_clk_gate_set(struct phy_device *phydev,
		bool gate_en)
{
	int rv = 0;

	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_CLK_GATE_EN, 0, AHB_HCLK_EN,
			(u8 *) (&gate_en));
	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_CLK_GATE_EN, 0, SUMMARY_INTR_EN,
			(u8 *) (&gate_en));
	QTI_8X8X_REG_FIELD_SET(phydev, TLMM_CLK_GATE_EN, 0, CRIF_READ_EN,
			(u8 *) (&gate_en));

	pr_dbg("[%s] gate_en:%d", __func__, gate_en);

	return rv;
}

static int qti_8x8x_pinctrl_rev_check(struct phy_device *phydev)
{
	int rv = 0;
	u32 version_id = 0, mfg_id = 0, start_bit = 0;

	QTI_8X8X_REG_FIELD_GET(phydev, TLMM_HW_REVISION_NUMBER, 0, VERSION_ID,
			(u8 *) (&version_id));
	QTI_8X8X_REG_FIELD_GET(phydev, TLMM_HW_REVISION_NUMBER, 0, MFG_ID,
			(u8 *) (&mfg_id));
	QTI_8X8X_REG_FIELD_GET(phydev, TLMM_HW_REVISION_NUMBER, 0, START_BIT,
			(u8 *) (&start_bit));

	pr_dbg("[%s] version_id:0x%x mfg_id:0x%x start_bit:0x%x",
			__func__, version_id, mfg_id, start_bit);

	if((version_id == 0x0) && (mfg_id == 0x70) && (start_bit == 0x1)) {
		pr_dbg(" Pinctrl Version Check Pass\n");
	} else {
		printf("Error: Pinctrl Version Check Fail\n");
		rv = -ENXIO;
	}

	return rv;
}

static int qti_8x8x_pinctrl_hw_init(struct phy_device *phydev)
{
	int rv = 0;

	rv = qti_8x8x_pinctrl_clk_gate_set(phydev, true);
	rv = qti_8x8x_pinctrl_rev_check(phydev);

	return rv;
}

static int qti_8x8x_pinctrl_setting_init(struct phy_device *phydev,
		const struct qti_8x8x_pinctrl_setting *pin_settings,
		u32 num_setting)
{
	int rv = 0;
	u32 i;

	for(i = 0; i < num_setting; i++) {
		const struct qti_8x8x_pinctrl_setting *setting =
			&pin_settings[i];
		if (setting->type == QTI_8X8X_PIN_MAP_TYPE_MUX_GROUP)
		{
			rv = qti_8x8x_gpio_pin_mux_set(phydev,
					setting->data.mux.pin,
					setting->data.mux.func);
		}
		else if (setting->type == QTI_8X8X_PIN_MAP_TYPE_CONFIGS_PIN)
		{
			rv = qti_8x8x_gpio_pin_cfg_set(phydev,
					setting->data.configs.pin,
					setting->data.configs.configs,
					setting->data.configs.num_configs);
		}
		if (rv)
			break;
	}

	return rv;
}

static int qti_8x8x_pinctrl_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = qti_8x8x_pinctrl_hw_init(phydev);
	if (ret)
		return ret;

	ret = qti_8x8x_pinctrl_setting_init(phydev,
			qti_8x8x_pin_settings,
			ARRAY_SIZE(qti_8x8x_pin_settings));
	if (ret)
		return ret;

	return ret;
}

static void qti_8x8x_phy_ipg_config(struct phy_device *phydev,
		uint32_t phy_id, u32 speed)
{
	uint16_t phy_data = 0;

	phy_data = qti_8x8x_phy_mmd_read(phydev, phy_id,
			QTI_8X8X_PHY_MMD7_NUM,
			QTI_8X8X_PHY_MMD7_IPG_10_11_ENABLE);

	phy_data &= ~QTI_8X8X_PHY_MMD7_IPG_11_EN;

	/*If speed is 1G, enable 11 ipg tuning*/
	pr_dbg("if speed is 1G, enable 11 ipg tuning\n");
	if (speed == SPEED_1000)
		phy_data |= QTI_8X8X_PHY_MMD7_IPG_11_EN;

	qti_8x8x_phy_mmd_write(phydev, phy_id,
			QTI_8X8X_PHY_MMD7_NUM,
			QTI_8X8X_PHY_MMD7_IPG_10_11_ENABLE, phy_data);
}

static void qti_8x8x_phy_sgmii_speed_fixup(struct phy_device *phydev,
		u32 phy_addr, u32 link, u32 new_speed)
{
	/*disable ethphy3 and uniphy0 clock*/
	pr_dbg("disable ethphy3 and uniphy0 clock\n");
	qti_8x8x_port_clk_en_set(phydev, DEV_8X8X_PORT_4,
			QTI_8X8X_CLK_TYPE_EPHY, false);
	qti_8x8x_port_clk_en_set(phydev, DEV_8X8X_PORT_5,
			QTI_8X8X_CLK_TYPE_UNIPHY, false);

	/*set gmii clock for ethphy3 and uniphy0*/
	pr_dbg("set speed clock for eth3 and uniphy0\n");
	qti_8x8x_port_speed_clock_set(phydev, DEV_8X8X_PORT_4, new_speed);

	/*uniphy and ethphy gmii clock enable/disable*/
	pr_dbg("uniphy and ethphy GMII clock enable/disable\n");
	if(link)
	{
		pr_dbg("enable ethphy3 and uniphy0 clock\n");
		qti_8x8x_port_clk_en_set(phydev, DEV_8X8X_PORT_4,
				QTI_8X8X_CLK_TYPE_EPHY, true);
		qti_8x8x_port_clk_en_set(phydev, DEV_8X8X_PORT_5,
				QTI_8X8X_CLK_TYPE_UNIPHY, true);
	}
	/*uniphy and ethphy gmii reset and release*/
	pr_dbg("uniphy and ethphy GMII interface reset and release\n");
	qti_8x8x_port_clk_reset(phydev, DEV_8X8X_PORT_4,
			QTI_8X8X_CLK_TYPE_EPHY);
	qti_8x8x_port_clk_reset(phydev, DEV_8X8X_PORT_5,
			QTI_8X8X_CLK_TYPE_UNIPHY);

	/*uniphy and ethphy ipg_tune reset, function reset*/
	pr_dbg("uniphy and ethphy ipg_tune reset, function reset\n");
	qti_8x8x_uniphy_sgmii_function_reset(phydev,
			QTI_8X8X_UNIPHY_SGMII_0);

	/*do ethphy function reset*/
	pr_dbg("do ethphy function reset\n");
	qti_8x8x_phy_function_reset(phydev, phy_addr);
	return;
}

static void qti_8x8x_cdt_thresh_init(struct phy_device *phydev)
{
	u32 phy_id = phydev->addr;
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL3,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL3_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL4,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL4_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL5,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL5_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL6,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL6_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL7,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL7_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL9,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL9_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL13,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL13_VAL);
	qti_8x8x_phy_mmd_write(phydev, phy_id, QTI_8X8X_PHY_MMD3_NUM,
				QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL14,
				QTI_8X8X_PHY_MMD3_NEAR_ECHO_THRESH_VAL);
}

static void qti_8x8x_phy_modify_debug(struct phy_device *phydev, u32 phy_addr,
		u32 debug_reg, u32 mask, u32 value)
{
	u16 phy_data = 0, new_phy_data = 0;

	qti_8x8x_phy_reg_write(phydev, phy_addr,
			QTI_8X8X_DEBUG_PORT_ADDRESS, debug_reg);
	phy_data = qti_8x8x_phy_reg_read(phydev, phy_addr,
			QTI_8X8X_DEBUG_PORT_DATA);
	if (phy_data == PHY_INVALID_DATA)
		pr_dbg("qti_8x8x_phy_reg_read failed\n");

	new_phy_data = (phy_data & ~mask) | value;
	qti_8x8x_phy_reg_write(phydev, phy_addr,
			QTI_8X8X_DEBUG_PORT_ADDRESS, debug_reg);
	qti_8x8x_phy_reg_write(phydev, phy_addr,
			QTI_8X8X_DEBUG_PORT_DATA, new_phy_data);

	/* check debug register value */
	qti_8x8x_phy_reg_write(phydev, phy_addr,
			QTI_8X8X_DEBUG_PORT_ADDRESS, debug_reg);
	phy_data = qti_8x8x_phy_reg_read(phydev, phy_addr,
			QTI_8X8X_DEBUG_PORT_DATA);
	pr_dbg("phy_addr:0x%x, debug_reg:0x%x, phy_data:0x%x\n",
		phy_addr, debug_reg, phy_data);
}

static void qti_8x8x_phy_adc_edge_set(struct phy_device *phydev, u32 adc_edge)
{
	u32 phy_addr = phydev->addr;
	qti_8x8x_phy_modify_debug(phydev, phy_addr,
		QTI_8X8X_PHY_DEBUG_ANA_INTERFACE_CLK_SEL, 0xf0, adc_edge);
	qti_8x8x_phy_reset(phydev, phy_addr);
}

static void qti_8x8x_switch_reset(struct phy_device *phydev)
{
	/* Reset switch core */
	qti_8x8x_clk_reset(phydev, QTI_8X8X_SWITCH_CORE_CLK);

	/* Reset MAC ports */
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC0_TX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC0_RX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC1_TX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC1_RX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC2_TX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC2_RX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC3_TX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC3_RX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC4_TX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC4_RX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC5_TX_CLK);
	qti_8x8x_clk_reset(phydev, QTI_8X8X_MAC5_RX_CLK);
	return;
}

static void qti_8x8x_work_mode_set(struct phy_device *phydev,
		qti_8x8x_work_mode_t work_mode)
{
	u32 data = 0;

	data = qti_8x8x_mii_read(phydev, WORK_MODE_OFFSET);
	data &= ~QTI_8X8X_WORK_MODE_MASK;
	data |= work_mode;

	qti_8x8x_mii_write(phydev, WORK_MODE_OFFSET, data);
	return;
}

static void qti_8x8x_work_mode_get(struct phy_device *phydev,
		qti_8x8x_work_mode_t *work_mode)
{
	u32 data = 0;

	data = qti_8x8x_mii_read(phydev, WORK_MODE_OFFSET);
	pr_dbg("work mode reg is 0x%x\n", data);

	*work_mode = data & QTI_8X8X_WORK_MODE_MASK;
	return;
}

static int qti_8x8x_work_mode_init(struct phy_device *phydev,
		phy_interface_t mode0, phy_interface_t mode1)
{
	int ret = 0;

	switch (mode0) {
	case PHY_INTERFACE_MODE_SGMII_2500:
	case PHY_INTERFACE_MODE_SGMII:
		break;
	default:
		/** not supported */
		printf("%s %d Error: Unsupported mac_mode0 \n",
				__func__, __LINE__);
		return -EINVAL;
	}

	if (qti_8x8x_uniphy_mode_check(phydev, QTI_8X8X_UNIPHY_SGMII_0,
				QTI_8X8X_UNIPHY_PHY)){
		pr_dbg("%s %d QTI_8X8X Uniphy 0 is in SGMII Mode \n",
				__func__, __LINE__);
		qti_8x8x_work_mode_set(phydev,
				QTI_8X8X_SWITCH_BYPASS_PORT5_MODE);
		return ret;
	}

	switch (mode1) {
	case PHY_INTERFACE_MODE_SGMII_2500:
	case PHY_INTERFACE_MODE_MAX:
	case PHY_INTERFACE_MODE_NA:
		qti_8x8x_work_mode_set(phydev, QTI_8X8X_SWITCH_MODE);
		break;
	default:
		printf("%s %d Error: Unsupported mac_mode1 \n",
				__func__, __LINE__);
		return -EINVAL;
	}

	return ret;
}

static int chip_verify(struct phy_device *phydev)
{
	int ret = 0;
	u8 chip_ver;
	u32 reg_val = qti_8x8x_mii_read(phydev, 0);

	chip_ver = (reg_val & 0xFF00) >> 8;
	switch (chip_ver) {
	case QCA_VER_QTI_8X8X:
		break;
	default:
		printf("Error: Unsupported chip \n");
		ret = -ENXIO;
		break;
	}

	return ret;
}

static bool qti_8x8x_port_phy_connected(struct phy_device *phydev,
		u32 port_id, u32 cpu_bmp)
{
	if ((cpu_bmp & BIT(port_id)) || (port_id == DEV_8X8X_PORT_0) ||
		(port_id == DEV_8X8X_PORT_5))
		return false;

	return true;
}


static int qti_8x8x_port_txmac_status_set(struct phy_device *phydev,
		u32 port_id, bool enable, u32 cpu_bmp)
{
	int ret = 0;
	u32 reg, force, val = 0, tmp;

	QTI_8X8X_REG_ENTRY_GET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));

	if (true == enable)
	{
		val = 1;
	}
	else if (false == enable)
	{
		val = 0;
	}
	tmp = reg;

	/* for those ports without PHY device we set MAC register */
	if (false == qti_8x8x_port_phy_connected(phydev, port_id, cpu_bmp))
	{
		SW_SET_REG_BY_FIELD(PORT_STATUS, LINK_EN,  0, reg);
		SW_SET_REG_BY_FIELD(PORT_STATUS, TXMAC_EN, val, reg);
	}
	else
	{
		SW_GET_FIELD_BY_REG(PORT_STATUS, LINK_EN,  force, reg);
		if (force)
		{
			/* link isn't in force mode so can't set */
			printf("%s %d Error: SW disable \n",
					__func__, __LINE__);
			return -EACCES;
		}
		else
		{
			SW_SET_REG_BY_FIELD(PORT_STATUS, TXMAC_EN, val, reg);
		}
	}

	if (tmp == reg)
		return ret;

	QTI_8X8X_REG_ENTRY_SET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));
	return ret;
}

static int qti_8x8x_port_rxmac_status_set(struct phy_device *phydev,
		u32 port_id, bool enable, u32 cpu_bmp)
{
	int ret = 0;
	u32 reg = 0, force, val = 0, tmp;

	QTI_8X8X_REG_ENTRY_GET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));

	if (true == enable)
	{
		val = 1;
	}
	else if (false == enable)
	{
		val = 0;
	}
	tmp = reg;

	/* for those ports without PHY device we set MAC register */
	if (false == qti_8x8x_port_phy_connected(phydev, port_id, cpu_bmp))
	{
		SW_SET_REG_BY_FIELD(PORT_STATUS, LINK_EN,  0, reg);
		SW_SET_REG_BY_FIELD(PORT_STATUS, RXMAC_EN, val, reg);
	}
	else
	{
		SW_GET_FIELD_BY_REG(PORT_STATUS, LINK_EN,  force, reg);
		if (force)
		{
			/* link isn't in force mode so can't set */
			printf("%s %d Error: SW disable \n",
					__func__, __LINE__);
			return -EACCES;
		}
		else
		{
			SW_SET_REG_BY_FIELD(PORT_STATUS, RXMAC_EN, val, reg);
		}
	}
	if (tmp == reg)
		return ret;
	QTI_8X8X_REG_ENTRY_SET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));
	return ret;
}

static void qti_8x8x_port_rxfc_status_set(struct phy_device *phydev,
		u32 port_id, bool enable)
{
	u32 val = 0, reg, tmp;

	if (true == enable)
	{
	    val = 1;
	}
	else if (false == enable)
	{
	    val = 0;
	}

	QTI_8X8X_REG_ENTRY_GET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));
	tmp = reg;

	SW_SET_REG_BY_FIELD(PORT_STATUS, RX_FLOW_EN, val, reg);

	if ( tmp == reg)
		return;

	QTI_8X8X_REG_ENTRY_SET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));
	return;
}

static void qti_8x8x_port_txfc_status_set(struct phy_device *phydev,
		u32 port_id, bool enable)
{
	u32 val = 0, reg = 0, tmp;

	if (true == enable)
	{
	    val = 1;
	}
	else if (false == enable)
	{
	    val = 0;
	}

	QTI_8X8X_REG_ENTRY_GET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));
	tmp = reg;

	SW_SET_REG_BY_FIELD(PORT_STATUS, TX_FLOW_EN, val, reg);
	SW_SET_REG_BY_FIELD(PORT_STATUS, TX_HALF_FLOW_EN, val, reg);

	if (tmp == reg)
		return;
	QTI_8X8X_REG_ENTRY_SET(phydev, PORT_STATUS, port_id, (u8 *) (&reg));

	return;
}

static void qti_8x8x_port_flowctrl_set(struct phy_device *phydev,
		u32 port_id, bool enable)
{
	qti_8x8x_port_txfc_status_set(phydev, port_id, enable);
	qti_8x8x_port_rxfc_status_set(phydev, port_id, enable);
	return;
}

static void qti_8x8x_port_flowctrl_forcemode_set(struct phy_device *phydev,
		u32 port_id, bool enable)
{
	qti_8x8x_port_txfc_forcemode[port_id] = enable;
	qti_8x8x_port_rxfc_forcemode[port_id] = enable;
	return;
}

static void header_type_set(struct phy_device *phydev, bool enable, u32 type)
{
	u32 reg = 0;

	QTI_8X8X_REG_ENTRY_GET(phydev, HEADER_CTL, 0, (u8 *) (&reg));

	if (true == enable)
	{
		if (0xffff < type)
		{
			printf("%s %d Error: Bad param \n",
					__func__, __LINE__);
			return;
		}
		SW_SET_REG_BY_FIELD(HEADER_CTL, TYPE_LEN, 1, reg);
		SW_SET_REG_BY_FIELD(HEADER_CTL, TYPE_VAL, type, reg);
	}
	else if (false == enable)
	{
		SW_SET_REG_BY_FIELD(HEADER_CTL, TYPE_LEN, 0, reg);
		SW_SET_REG_BY_FIELD(HEADER_CTL, TYPE_VAL, 0, reg);
	}

	QTI_8X8X_REG_ENTRY_SET(phydev, HEADER_CTL, 0, (u8 *) (&reg));
	return;
}

static void port_rxhdr_mode_set(struct phy_device *phydev,
		u32 port_id, port_header_mode_t mode)
{
	u32 val = 0;
	if (FAL_NO_HEADER_EN == mode)
	{
		val = 0;
	}
	else if (FAL_ONLY_MANAGE_FRAME_EN == mode)
	{
		val = 1;
	}
	else if (FAL_ALL_TYPE_FRAME_EN == mode)
	{
		val = 2;
	}
	else
	{
		printf("%s %d Error: Bad param \n", __func__, __LINE__);
		return;
	}

	QTI_8X8X_REG_FIELD_SET(phydev, PORT_HDR_CTL,
			port_id, RXHDR_MODE,(u8 *) (&val));
	return;
}

static void port_txhdr_mode_set(struct phy_device *phydev,
		u32 port_id, port_header_mode_t mode)
{
	u32 val = 0;
	if (FAL_NO_HEADER_EN == mode)
	{
		val = 0;
	}
	else if (FAL_ONLY_MANAGE_FRAME_EN == mode)
	{
		val = 1;
	}
	else if (FAL_ALL_TYPE_FRAME_EN == mode)
	{
		val = 2;
	}
	else
	{
		printf("%s %d Error: Bad param \n", __func__, __LINE__);
		return;
	}

	QTI_8X8X_REG_FIELD_SET(phydev, PORT_HDR_CTL,
			port_id, TXHDR_MODE, (u8 *) (&val));
	return;
}


static int qti_8x8x_phy_get_status(struct phy_device *phydev, u32 phy_id,
		struct port_phy_status *phy_status)
{
	u16 phy_data;

	phy_data = qti_8x8x_phy_reg_read(phydev, phy_id,
			QTI_8X8X_PHY_SPEC_STATUS);

	/*get phy link status*/
	if (phy_data & QTI_8X8X_STATUS_LINK_PASS) {
		phy_status->link_status = true;
	}
	else {
		phy_status->link_status = false;

		/*when link down, phy speed is set as 10M*/
		phy_status->speed = SPEED_10;
		return 0;
	}

	/*get phy speed*/
	switch (phy_data & QTI_8X8X_STATUS_SPEED_MASK) {
	case QTI_8X8X_STATUS_SPEED_2500MBS:
		phy_status->speed = SPEED_2500;
		break;
	case QTI_8X8X_STATUS_SPEED_1000MBS:
		phy_status->speed = SPEED_1000;
		break;
	case QTI_8X8X_STATUS_SPEED_100MBS:
		phy_status->speed = SPEED_100;
		break;
	case QTI_8X8X_STATUS_SPEED_10MBS:
		phy_status->speed = SPEED_10;
		break;
	default:
		return -EINVAL;
	}

	/*get phy duplex*/
	if (phy_data & QTI_8X8X_STATUS_FULL_DUPLEX) {
		phy_status->duplex = DUPLEX_FULL;
	} else {
		phy_status->duplex = DUPLEX_HALF;
	}

	/* get phy flowctrl resolution status */
	if (phy_data & QTI_8X8X_PHY_RX_FLOWCTRL_STATUS) {
		phy_status->rx_flowctrl = true;
	} else {
		phy_status->rx_flowctrl = false;
	}

	if (phy_data & QTI_8X8X_PHY_TX_FLOWCTRL_STATUS) {
		phy_status->tx_flowctrl = true;
	} else {
		phy_status->tx_flowctrl = false;
	}

	return 0;
}


static void qti_8x8x_port_mac_dupex_set(struct phy_device *phydev,
		u32 port_id, u32 duplex)
{
	u32 reg_val = 0, tmp;
	u32 duplex_val;

	QTI_8X8X_REG_ENTRY_GET(phydev, PORT_STATUS,
			port_id, (u8 *) (&reg_val));
	tmp = reg_val;

	if (DUPLEX_HALF == duplex) {
		duplex_val = QTI_8X8X_PORT_HALF_DUPLEX;
	} else {
		duplex_val = QTI_8X8X_PORT_FULL_DUPLEX;
	}
	SW_SET_REG_BY_FIELD(PORT_STATUS, DUPLEX_MODE, duplex_val, reg_val);

	if (tmp == reg_val)
		return;

	QTI_8X8X_REG_ENTRY_SET(phydev, PORT_STATUS,
			port_id, (u8 *) (&reg_val));
	return;
}

static int qti_8x8x_port_duplex_set(struct phy_device *phydev,
		u32 port_id, u32 duplex, u32 cpu_bmp)
{
	/* for those ports without PHY device we set MAC register */
	if (false == qti_8x8x_port_phy_connected(phydev, port_id, cpu_bmp))
	{
		qti_8x8x_port_mac_dupex_set(phydev, port_id, duplex);
	}
	else
	{
		printf("%s %d Error: Duplex/Speed set for QTI_8X8X "
		       	"DEV_8X8X_PORT_1-4 is not implemented \n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static void qti_8x8x_port_mac_speed_set(struct phy_device *phydev,
		u32 port_id, u32 speed)
{
	u32 reg_val = 0, tmp;
	u32 speed_val;

	QTI_8X8X_REG_ENTRY_GET(phydev, PORT_STATUS,
			port_id, (u8 *) (&reg_val));
	tmp = reg_val;

	if (SPEED_10 == speed) {
		speed_val = QTI_8X8X_PORT_SPEED_10M;
	} else if (SPEED_100 == speed) {
		speed_val = QTI_8X8X_PORT_SPEED_100M;
	} else if (SPEED_1000 == speed) {
		speed_val = QTI_8X8X_PORT_SPEED_1000M;
	} else if (SPEED_2500 == speed) {
		speed_val = QTI_8X8X_PORT_SPEED_2500M;
	} else {
		printf("%s %d Bad param \n",__func__, __LINE__);
		return;
	}
	SW_SET_REG_BY_FIELD(PORT_STATUS, SPEED_MODE, speed_val, reg_val);

	if (tmp == reg_val)
		return;

	QTI_8X8X_REG_ENTRY_SET(phydev, PORT_STATUS,
			port_id, (u8 *) (&reg_val));
	return;
}

static int qti_8x8x_port_speed_set(struct phy_device *phydev,
		u32 port_id, u32 speed, u32 cpu_bmp)
{
	/* for those ports without PHY device we set MAC register */
	if (false == qti_8x8x_port_phy_connected(phydev, port_id, cpu_bmp))
	{
		qti_8x8x_port_mac_speed_set(phydev, port_id, speed);
	}
	else
	{
		printf("%s %d Error: Duplex/Speed set for QTI_8X8X "
			"DEV_8X8X_PORT_1-4 is not implemented \n",
			__func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int _qti_8x8x_interface_mode_init(struct phy_device *phydev,
		u32 port_id, u32 cpu_bmp,
		struct qti_8x8x_port_info * port_info)
{
	int ret = 0;
	u32 uniphy_index = 0;
	mac_config_t config;
	u32 speed = 0;
	u32 mac_mode = port_info->mode;
	qti_8x8x_work_mode_t work_mode = QTI_8X8X_SWITCH_MODE;

	if (port_info->speed) {
		speed = port_info->speed;
		ret = qti_8x8x_port_speed_set(phydev, port_id,
				speed, cpu_bmp);
		if (ret)
			return ret;
		ret = qti_8x8x_port_duplex_set(phydev, port_id,
				port_info->duplex, cpu_bmp);
		if (ret)
			return ret;

		/* The clock parent need to be configured
		 * before initializing the interface mode. */
		qti_8x8x_work_mode_get(phydev, &work_mode);
		qti_8x8x_gcc_port_clk_parent_set(phydev,
				work_mode, port_id);
	}

	if(mac_mode == PHY_INTERFACE_MODE_SGMII_2500)
		config.mac_mode = QTI_8X8X_MAC_MODE_SGMII_PLUS;
	else if (mac_mode == PHY_INTERFACE_MODE_SGMII)
		config.mac_mode = QTI_8X8X_MAC_MODE_SGMII;
	else if (mac_mode == PHY_INTERFACE_MODE_MAX)
		config.mac_mode = QTI_8X8X_MAC_MODE_MAX;
	else {
		printf("%s %d Unsupported mac mode \n",
			       __func__, __LINE__);
		return -EINVAL;
	}

	/*get uniphy index*/
	if(port_id == DEV_8X8X_PORT_0)
		uniphy_index = QTI_8X8X_UNIPHY_SGMII_1;
	else if(port_id == DEV_8X8X_PORT_5)
		uniphy_index = QTI_8X8X_UNIPHY_SGMII_0;
	else {
		printf("%s %d Unsupported mac_mode \n",
				__func__, __LINE__);
		return -EINVAL;
	}

	config.clock_mode = QTI_8X8X_INTERFACE_CLOCK_MAC_MODE;
	config.auto_neg = !(port_info->speed);
	config.speed = speed;

	if (port_id == DEV_8X8X_PORT_5) {
		if (qti_8x8x_uniphy_mode_check(phydev,
					QTI_8X8X_UNIPHY_SGMII_0,
					QTI_8X8X_UNIPHY_PHY)) {
			pr_dbg("%s %d QTI_8X8X Uniphy 0 is in SGMII Mode\n",
					__func__, __LINE__);
		} else {
			if (config.mac_mode == QTI_8X8X_MAC_MODE_MAX) {
				pr_dbg("%s %d QTI_8X8X Port 5 clk disable\n",
						__func__, __LINE__);
				qti_8x8x_clk_disable(phydev,
						QTI_8X8X_SRDS0_SYS_CLK);
			}
		}
	} else {
		qti_8x8x_interface_sgmii_mode_set(phydev,
				uniphy_index, port_id, &config);

		/*do sgmii function reset*/
		pr_dbg("ipg_tune reset and function reset\n");
		qti_8x8x_uniphy_sgmii_function_reset(phydev, uniphy_index);
	}
	return ret;
}

static int qti_8x8x_interface_mode_init(struct phy_device *phydev,
		struct qti_8x8x_switch_info * swt_info)
{
	int ret = 0;

	ret = _qti_8x8x_interface_mode_init(phydev, DEV_8X8X_PORT_0,
			swt_info->cpu_bmp, &swt_info->port[DEV_8X8X_PORT_0]);
	if (ret < 0)
		return ret;

	ret = _qti_8x8x_interface_mode_init(phydev, DEV_8X8X_PORT_5,
			swt_info->cpu_bmp, &swt_info->port[DEV_8X8X_PORT_5]);
	if (ret < 0)
		return ret;

	if (swt_info->port[DEV_8X8X_PORT_0].speed) {
		ret = qti_8x8x_port_txmac_status_set(phydev, DEV_8X8X_PORT_0,
				swt_info->cpu_bmp, true);
		if (ret < 0)
			return ret;
		ret = qti_8x8x_port_rxmac_status_set(phydev, DEV_8X8X_PORT_0,
				swt_info->cpu_bmp, true);
		if (ret < 0)
			return ret;
	}

	if (swt_info->port[DEV_8X8X_PORT_5].speed) {
		ret = qti_8x8x_port_txmac_status_set(phydev, DEV_8X8X_PORT_5,
				swt_info->cpu_bmp, true);
		if (ret < 0)
			return ret;
		ret = qti_8x8x_port_rxmac_status_set(phydev, DEV_8X8X_PORT_5,
				swt_info->cpu_bmp, true);
		if (ret < 0)
			return ret;
	}
	return ret;
}


static void port_link_update(struct phy_device *phydev, u32 port_id,
		struct port_phy_status phy_status)
{
	/* configure gcc uniphy and mac speed frequency*/
	qti_8x8x_port_speed_clock_set(phydev, port_id, phy_status.speed);

	/* configure mac speed and duplex */
	qti_8x8x_port_mac_speed_set(phydev, port_id, phy_status.speed);
	qti_8x8x_port_mac_dupex_set(phydev, port_id, phy_status.duplex);
	pr_dbg("8x8x port %d link %d update speed %d duplex %d\n",
			port_id, phy_status.speed,
			phy_status.speed, phy_status.duplex);
	if (phy_status.link_status)
	{
		/* sync mac flowctrl */
		if (qti_8x8x_port_txfc_forcemode[port_id] != true) {
			qti_8x8x_port_txfc_status_set(phydev,
					port_id, phy_status.tx_flowctrl);
			pr_dbg("8x8x port %d link up update txfc %d\n",
			port_id, phy_status.tx_flowctrl);
		}
		if (qti_8x8x_port_rxfc_forcemode[port_id] != true) {
			qti_8x8x_port_rxfc_status_set(phydev,
					port_id, phy_status.rx_flowctrl);
			pr_dbg("8x8x port %d link up update rxfc %d\n",
			port_id, phy_status.rx_flowctrl);
		}
		if (port_id != DEV_8X8X_PORT_5) {
			/* enable eth phy clock */
			qti_8x8x_port_clk_en_set(phydev, port_id,
					QTI_8X8X_CLK_TYPE_EPHY, true);
		}
	}
	if (port_id != DEV_8X8X_PORT_5) {
		if (!phy_status.link_status) {
			/* disable eth phy clock */
			qti_8x8x_port_clk_en_set(phydev, port_id,
					QTI_8X8X_CLK_TYPE_EPHY, false);
		}
		/* reset eth phy clock */
		qti_8x8x_port_clk_reset(phydev,
				port_id, QTI_8X8X_CLK_TYPE_EPHY);
		/* reset eth phy fifo */
		qti_8x8x_phy_function_reset(phydev, port_id);
	}

	return;
}

static void port_3az_status_set(struct phy_device *phydev,
		u32 port_id, bool enable)
{
	u32 reg = 0, field = 0, offset, device_id, reverse = 0;
	u32 eee_mask = 0;

	QTI_8X8X_REG_ENTRY_GET(phydev, MASK_CTL, 0, (u8 *) (&reg));

	SW_GET_FIELD_BY_REG(MASK_CTL, DEVICE_ID, device_id, reg);
	switch (device_id) {
	case QCA_VER_QTI_8X8X:
		eee_mask = 3;
		reverse = 0;
		break;
	default:
		printf("%s %d Unsupported DEV_ID \n",
				__func__, __LINE__);
		return;
	}

	QTI_8X8X_REG_ENTRY_GET(phydev, EEE_CTL, 0, (u8 *) (&reg));

	if (true == enable)
	{
		field  = eee_mask;
	}
	else if (false == enable)
	{
		field  = 0;
	}

	if (reverse)
	{
		field = (~field) & eee_mask;
	}

	offset = (port_id - 1) * ISISC_LPI_BIT_STEP + ISISC_LPI_PORT1_OFFSET;
	reg &= (~(eee_mask << offset));
	reg |= (field << offset);

	QTI_8X8X_REG_ENTRY_SET(phydev, EEE_CTL, 0, (u8 *) (&reg));
	return;
}

static void qos_port_tx_buf_nr_set(struct phy_device *phydev,
		u32 port_id, u32 * number)
{
	u32 val = 0;
	if (ISISC_QOS_PORT_TX_BUFFER_MAX < *number)
	{
		printf("%s %d Bad param \n", __func__, __LINE__);
		return;
	}

	val = *number / ISISC_QOS_HOL_STEP;
	*number = val << ISISC_QOS_HOL_MOD;
	QTI_8X8X_REG_FIELD_SET(phydev, PORT_HOL_CTL0, port_id, PORT_DESC_NR,
			(u8 *) (&val));
	return;
}

static void qos_port_rx_buf_nr_set(struct phy_device *phydev,
		u32 port_id, u32 * number)
{
	u32 val = 0;
	if (ISISC_QOS_PORT_RX_BUFFER_MAX < *number)
	{
		printf("%s %d Bad param \n", __func__, __LINE__);
		return;
	}

	val = *number / ISISC_QOS_HOL_STEP;
	*number = val << ISISC_QOS_HOL_MOD;
	QTI_8X8X_REG_FIELD_SET(phydev, PORT_HOL_CTL1, port_id,
			PORT_IN_DESC_EN, (u8 *) (&val));
	return;
}

static void qti_8x8x_switch_config(struct phy_device *phydev,
		struct qti_8x8x_switch_info * swt_info)
{
	u32 temp, i = 0, port_hol_ctrl[2] = {0};
	u32 port_bmp = swt_info->lan_bmp | swt_info->wan_bmp;

	port_bmp |= swt_info->cpu_bmp;
	while (port_bmp) {
		pr_dbg("configuring port: %d \n", i);
		if (port_bmp & 1) {
			temp = 0;
			QTI_8X8X_REG_FIELD_GET(phydev, FORWARD_CTL1, 0,
					BC_FLOOD_DP, (u8 *) (&temp));
			temp |= (0x1 << i);
			QTI_8X8X_REG_FIELD_SET(phydev, FORWARD_CTL1, 0,
					BC_FLOOD_DP, (u8 *) (&temp));

			qti_8x8x_port_txmac_status_set(phydev, i, false,
					swt_info->cpu_bmp);
			qti_8x8x_port_rxmac_status_set(phydev, i, false,
					swt_info->cpu_bmp);

			if (swt_info->cpu_bmp & BIT(i)) {
				qti_8x8x_port_flowctrl_set(phydev, i, false);
				qti_8x8x_port_flowctrl_forcemode_set(phydev,
						i, true);

				header_type_set(phydev, true,
						QTI_8X8X_HEADER_TYPE_VAL);
				port_rxhdr_mode_set(phydev, i,
						FAL_ONLY_MANAGE_FRAME_EN);
				port_txhdr_mode_set(phydev, i,
						FAL_NO_HEADER_EN);

				/* port tx buf number */
				port_hol_ctrl[0] = 600;
				/* port rx buf number */
				port_hol_ctrl[1] = 48;
			} else {
				qti_8x8x_port_flowctrl_set(phydev,
						i, true);
				qti_8x8x_port_flowctrl_forcemode_set(phydev,
						i, false);
			}

			port_3az_status_set(phydev, i, false);

			temp=1;
			QTI_8X8X_REG_FIELD_SET(phydev, PORT_HOL_CTL1,
					i, PORT_RED_EN, (u8 *) (&temp));

			qos_port_tx_buf_nr_set(phydev, i, &port_hol_ctrl[0]);
			qos_port_rx_buf_nr_set(phydev, i, &port_hol_ctrl[1]);

		}
		port_bmp >>=1;
		i++;
	}

	return;
}

/***************************************************************************/
/* core_function end */
/***************************************************************************/

/***************************************************************************/
/* exported_function start */
/***************************************************************************/
static void qti_8x8x_phy_uqxgmii_speed_fixup(struct phy_device *phydev)
{
	uint32_t phy_addr = phydev->addr, qti_8x8x_port_id = phydev->addr;
	uint32_t status = phydev->link, new_speed = phydev->speed;
	uint32_t port_clock_en = 0;

	/*Restart the auto-neg of uniphy*/
	pr_dbg("Restart the auto-neg of uniphy\n");
	qti_8x8x_uniphy_xpcs_autoneg_restart(phydev, qti_8x8x_port_id);

	/*set gmii+ clock to uniphy1 and ethphy*/
	pr_dbg("set gmii,xgmii clock to uniphy and gmii to ethphy\n");
	qti_8x8x_port_speed_clock_set(phydev, qti_8x8x_port_id, new_speed);

	/*set xpcs speed*/
	pr_dbg("set xpcs speed\n");
	qti_8x8x_uniphy_xpcs_speed_set(phydev, qti_8x8x_port_id, new_speed);

	/*GMII/XGMII clock and ETHPHY GMII clock enable/disable*/
	pr_dbg("GMII/XGMII clock and ETHPHY GMII clock enable/disable\n");
	if (status)
		port_clock_en = 1;
	qti_8x8x_port_clk_en_set(phydev, qti_8x8x_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY | QTI_8X8X_CLK_TYPE_EPHY,
			port_clock_en);

	pr_dbg("UNIPHY GMII/XGMII interface and "
			"ETHPHY GMII interface reset and release\n");
	qti_8x8x_port_clk_reset(phydev, qti_8x8x_port_id,
			QTI_8X8X_CLK_TYPE_UNIPHY | QTI_8X8X_CLK_TYPE_EPHY);

	pr_dbg("ipg_tune and xgmii2gmii reset for uniphy "
			"and ETHPHY, function reset\n");
	qti_8x8x_uniphy_uqxgmii_function_reset(phydev, qti_8x8x_port_id);

	/*do ethphy function reset: PHY_FIFO_RESET*/
	pr_dbg("do ethphy function reset\n");
	qti_8x8x_phy_function_reset(phydev, phy_addr);

	/*change IPG from 10 to 11 for 1G speed*/
	qti_8x8x_phy_ipg_config(phydev, phy_addr, new_speed);
}

static int qti_8x8x_phy_interface_mode_set(struct phy_device *phydev)
{
	int ret = 0;

	if (!qti_8x8x_clk_is_enabled(phydev, QTI_8X8X_SWITCH_CORE_CLK)) {
		pr_dbg("QTI_8X8X already in PORT_UQXGMII mode \n");
		return ret;
	}

	pr_dbg("Configure QTI_8X8X as PORT_UQXGMII..\n");
	/*the work mode is PORT_UQXGMII in default*/
	qti_8x8x_interface_uqxgmii_mode_set(phydev);

	/*init clock for PORT_UQXGMII*/
	qti_8x8x_gcc_clock_init(phydev, QTI_8X8X_PHY_UQXGMII_MODE, 0);

	/*init pinctrl for phy mode to be added later*/
	ret = qti_8x8x_pinctrl_init(phydev);
	if (ret < 0)
		return ret;

	return ret;
}

static void qti_8x8x_phy_init(struct phy_device *phydev)
{
	/* adjust CDT threshold */
	qti_8x8x_cdt_thresh_init(phydev);

	/* invert ADC clock edge as falling edge to fix link issue */
	qti_8x8x_phy_adc_edge_set(phydev, ADC_FALLING);
}

static int qti_8x8x_switch_link_update(struct phy_device *phydev,
		struct qti_8x8x_switch_info *switch_info)
{
	struct port_phy_status phy_status = {0};
	struct qti_8x8x_port_info *port_info = switch_info->port;
	int rv, port_id, status = 1;

	printf("QTI_8X8X-switch status:\n");
	for (int i=DEV_8X8X_PORT_1; i<DEV_8X8X_PORT_5; i++) {
		port_id = port_info[i].addr;
		if (port_info[i].addr == -1)
			continue;

		rv = qti_8x8x_phy_get_status(phydev, port_id, &phy_status);
		if (rv < 0) {
			printf("%s %d failed get phy status of idx %d \n",
				__func__, __LINE__, port_id);
			return status;
		}

		printf("PORT%d %s Speed :%d %s duplex\n", port_id,
			(phy_status.link_status?"Up":"Down"),
			phy_status.speed, (phy_status.duplex?"Full":"Half"));

		if (!phy_status.link_status) {
			/* enable mac rx function */
			qti_8x8x_port_rxmac_status_set(phydev, port_id,
					false, switch_info->cpu_bmp);
			/* enable mac tx function */
			qti_8x8x_port_txmac_status_set(phydev, port_id,
					false, switch_info->cpu_bmp);
			/* update gcc, mac speed, mac duplex and phy stauts */
			port_link_update(phydev, port_id, phy_status);
		}

		if (phy_status.link_status) {
			/* update gcc, mac speed, mac duplex and phy stauts */
			port_link_update(phydev, port_id, phy_status);
			/* enable mac tx function */
			qti_8x8x_port_txmac_status_set(phydev, port_id,
					true, switch_info->cpu_bmp);
			/* enable mac rx function */
			qti_8x8x_port_rxmac_status_set(phydev, port_id,
					true, switch_info->cpu_bmp);

			status = 0;
		}
	}

	return status;
}

static int qti_8x8x_switch_init(struct phy_device *phydev,
		struct qti_8x8x_switch_info * swt_info)
{
	int ret = 0;
	qti_8x8x_work_mode_t work_mode;
	u32 mode0 = swt_info->port[DEV_8X8X_PORT_0].mode,
	    mode1 = swt_info->port[DEV_8X8X_PORT_5].mode;
	u32 port_bmp = swt_info->lan_bmp | swt_info->wan_bmp;

	ret = chip_verify(phydev);
	if (ret < 0)
		return ret;

	qti_8x8x_switch_reset(phydev);

	ret = qti_8x8x_work_mode_init(phydev, mode0, mode1);
	if (ret < 0)
		return ret;

	qti_8x8x_switch_config(phydev, swt_info);

	ret = qti_8x8x_interface_mode_init(phydev, swt_info);
	if (ret < 0)
		return ret;

	port_bmp |= swt_info->cpu_bmp;

	qti_8x8x_work_mode_get(phydev, &work_mode);
	ret = qti_8x8x_gcc_clock_init(phydev, work_mode, port_bmp);
	if (ret < 0)
		return ret;

	ret = qti_8x8x_pinctrl_init(phydev);
	if (ret < 0)
		return ret;

	return ret;
}

static int qti_8x8x_phy_sgmii_mode_set(struct phy_device *phydev,
		u32 link, u32 speed)
{
	uint32_t phy_addr_tmp = 0;
	mac_config_t config = {0};

	if ((speed <= SPEED_1000) && (speed >= SPEED_10))
		config.mac_mode = QTI_8X8X_MAC_MODE_SGMII;
	else if (speed == SPEED_2500)
		config.mac_mode = QTI_8X8X_MAC_MODE_SGMII_PLUS;
	else {
		printf("Unsupported interface speed %d\n", speed);
		return -EINVAL;
	}

	config.clock_mode = QTI_8X8X_INTERFACE_CLOCK_PHY_MODE;
	config.auto_neg = 1;

	qti_8x8x_ephy_addr_get(phydev, DEV_8X8X_PORT_4, &phy_addr_tmp);
	if(phy_addr_tmp != phydev->addr)
	{
		printf("phy_addr:0x%x is not matched with "
			"port4 phy addr:0x%x\n",
			phydev->addr, phy_addr_tmp);
		return -ENXIO;
	}

	qti_8x8x_interface_sgmii_mode_set(phydev, QTI_8X8X_UNIPHY_SGMII_0,
			DEV_8X8X_PORT_4, &config);

	qti_8x8x_phy_sgmii_speed_fixup(phydev, phydev->addr, link, speed);
	return 0;
}

static int qti_8x8x_bypass_interface_mode_set(struct phy_device *phydev)
{
	int ret = 0;
	qti_8x8x_work_mode_set(phydev, QTI_8X8X_PHY_SGMII_UQXGMII_MODE);
	ret = qti_8x8x_phy_sgmii_mode_set(phydev, 0, SPEED_1000);
	if (ret < 0)
		return ret;

	pr_dbg("ethphy3 software reset\n");
	qti_8x8x_phy_reset(phydev, DEV_8X8X_PORT_4);

	/*init pinctrl for phy mode to be added later*/
	return ret;
}

/***************************************************************************/
/* exported_function end */
/***************************************************************************/

static int qti_8x8x_probe(struct phy_device *phydev)
{
	struct qti_8x8x_device *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	memset(dev, 0, sizeof(*dev));

	phydev->priv = dev;
	dev->bus = phydev->bus;

	/*
	 * Taken care from the MAC
	 * try to access the ipq addr
	 * if fails,
	 * do the hw reset
	 *	mdio addr fixup
	 *	clock init
	 * else
	 */

	return 0;
}

static int qti_8x8x_config(struct phy_device *phydev)
{
	struct qti_8x8x_device *dev = phydev->priv;
	struct qti_8x8x_switch_info * switch_cfg;
	int ret = 0;
	unsigned int i;

	/*
	 * parse all the details from the ofnode
	 * number of ports
	 * port
	 *	phy-mode
	 *	addr
	 *	speed
	 *	duplex
	 *
	 */

	dev->mode = ofnode_read_u32_default(phydev->node, "dev-mode",
			DEV_8X8X_PHY_MODE);

	qti_8x8x_work_mode_t work_mode;
	qti_8x8x_work_mode_get(phydev, &work_mode);

	if (work_mode == QTI_8X8X_SWITCH_BYPASS_PORT5_MODE) {
		dev->mode = DEV_8X8X_BYPASS_MODE;
		return ret;
	}

	if ((dev->mode != DEV_8X8X_SWITCH_MODE) &&
			(dev->mode != DEV_8X8X_BYPASS_MODE)) {
		qti_8x8x_phy_interface_mode_set(phydev);
		qti_8x8x_phy_init(phydev);
	}
	else {
		if (dev->mode == DEV_8X8X_BYPASS_MODE) {
			struct phy_device local_phydev;

			memcpy(&local_phydev, phydev, sizeof(
						struct phy_device));
			local_phydev.addr = DEV_8X8X_PORT_4;

			qti_8x8x_phy_init(&local_phydev);
			ret = qti_8x8x_bypass_interface_mode_set(
					&local_phydev);
			if (ret)
				return ret;
		}

		dev->mode = DEV_8X8X_SWITCH_MODE;
		switch_cfg = malloc(sizeof(struct qti_8x8x_switch_info));
		if (!switch_cfg)
			return -ENOMEM;

		memset(switch_cfg, 0, sizeof(struct qti_8x8x_switch_info));

		switch_cfg->port = malloc(sizeof(struct qti_8x8x_port_info) \
				* DEV_8X8X_MAX_PORTS);
		if (!switch_cfg->port)
			return -ENOMEM;

		memset(switch_cfg->port, 0,
				sizeof(struct qti_8x8x_port_info)
				* DEV_8X8X_MAX_PORTS);

		for (i = 0; i < DEV_8X8X_MAX_PORTS; i++) {
			char port_no[10];
			ofnode switch_node, port_node, fixed_node;

			switch_node = ofnode_find_subnode(phydev->node,
					"ports");
			if (!ofnode_valid(switch_node)) {
				debug("%s ports node of switch not found!\n",
						__func__);
				return -EINVAL;
			}

			snprintf(port_no, sizeof(port_no), "port@%d", i);
			port_node = ofnode_find_subnode(switch_node, port_no);
			if (!ofnode_valid(port_node)) {
				debug("%s node not found ! \n", port_no);
				switch_cfg->port[i].addr = -1;
				switch_cfg->port[i].mode =
					PHY_INTERFACE_MODE_MAX;
				continue;
			}

			switch_cfg->port[i].mode = ofnode_read_phy_mode(
					port_node);
			switch_cfg->port[i].addr = ofnode_read_u32_default(
					port_node, "reg", -1);
			switch_cfg->port[i].label = ofnode_read_string(
					port_node, "label");

			if (switch_cfg->port[i].label) {
				if (!strcmp(switch_cfg->port[i].label, "lan"))
					switch_cfg->lan_bmp |= (1<<i);
				else if (!strcmp(switch_cfg->port[i].label,
							"cpu"))
					switch_cfg->cpu_bmp |= (1<<i);
				else if (!strcmp(switch_cfg->port[i].label,
							"wan"))
					switch_cfg->wan_bmp |= (1<<i);
			}

			fixed_node = ofnode_find_subnode(port_node,
				"fixed-link");
			if (ofnode_valid(fixed_node)) {
				switch_cfg->port[i].speed =
					ofnode_read_u32_default(fixed_node,
							"speed", 0);
				switch_cfg->port[i].duplex =
					ofnode_read_bool(fixed_node,
							"full-duplex");
			}
		}

		dev->switch_info = switch_cfg;
		ret = qti_8x8x_switch_init(phydev, dev->switch_info);
		if (ret)
			return ret;
	}

	return ret;
}

static int qti_8x8x_parse_status(struct phy_device *phydev)
{
	unsigned int speed;
	unsigned int mii_reg;

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, QTI_8X8X_PHY_SPEC_STATUS);

	// Waiting for PHY realtime link code, not support in 8x8x

	if (mii_reg & QTI_8X8X_STATUS_LINK_PASS)
		phydev->link = 1;
	else
		phydev->link = 0;

	if (mii_reg & QTI_8X8X_STATUS_FULL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	speed = mii_reg & QTI_8X8X_STATUS_SPEED_MASK;

	switch (speed) {
	case QTI_8X8X_STATUS_SPEED_2500MBS:
		phydev->speed = SPEED_2500;
		break;
	case QTI_8X8X_STATUS_SPEED_1000MBS:
		phydev->speed = SPEED_1000;
		break;
	case QTI_8X8X_STATUS_SPEED_100MBS:
		phydev->speed = SPEED_100;
		break;
	case QTI_8X8X_STATUS_SPEED_10MBS:
		phydev->speed = SPEED_10;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qti_8x8x_startup(struct phy_device *phydev)
{
	struct qti_8x8x_device *dev = phydev->priv;
	int ret = 0;

	if (dev->mode != DEV_8X8X_SWITCH_MODE) {
		ret = qti_8x8x_parse_status(phydev);
		if (ret)
			return ret;

		if (dev->mode == DEV_8X8X_BYPASS_MODE) {
			ret = qti_8x8x_phy_sgmii_mode_set(phydev,
					phydev->link, phydev->speed);
			if (ret)
				return ret;
		} else {
			qti_8x8x_phy_uqxgmii_speed_fixup(phydev);
		}

		return ret;
	}

	if (!qti_8x8x_switch_link_update(phydev, dev->switch_info)) {
		phydev->speed = dev->switch_info->port[DEV_8X8X_PORT_0].speed;
		phydev->duplex = 1;
		phydev->link = 1;
	}

	return ret;
}

static struct phy_driver qti_8x8x_driver = {
	.name = "Qualcomm QCA808x/QCA838x",
	.uid = 0x004dd180,
	.mask = 0xfffffff0,
	.features = PHY_GBIT_FEATURES,
	.probe = &qti_8x8x_probe,
	.config = &qti_8x8x_config,
	.startup = &qti_8x8x_startup,
	.shutdown = &genphy_shutdown,
};

void qti_8x8x_pre_init(struct phy_device *phydev)
{
	if (qti_8x8x_get_core_clk_status(phydev, QTI_8X8X_SRDS0_SYS_CBCR)) {
		debug("%s %d addr fixup and clk init already done!!!\n",
				__func__, __LINE__);
		return;
	}

	qti_8x8x_addr_fixup(phydev);
	qti_8x8x_core_clock_init(phydev);
	return;
}

int phy_8x8x_init(void)
{
	phy_register(&qti_8x8x_driver);
	return 0;
}
