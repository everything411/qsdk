/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/phy.h>

#define QCA8084_PHY_ID				0x004dd180

#define QCA8084_SPECIFIC_FUNCTION_CONTROL	0x10
#define QCA8084_SPECIFIC_STATUS			0x11
#define QCA8084_SS_SPEED_MASK			GENMASK(9, 7)
#define QCA8084_SS_SPEED_2500			4
#define QCA8084_SS_SPEED_1000			2
#define QCA8084_SS_SPEED_100			1
#define QCA8084_SS_SPEED_10			0
#define QCA8084_SS_DUPLEX			BIT(13)
#define QCA8084_SS_SPEED_DUPLEX_RESOLVED		BIT(11)
#define QCA8084_SS_MDIX				BIT(6)
#define QCA8084_SFC_MDI_CROSSOVER_MODE_M		GENMASK(6, 5)

#define QCA8084_PHY_FIFO_CONTROL			0x19
#define QCA8084_PHY_FIFO_RESET		0x3

#define QCA8084_DEBUG_ADDR			0x1D
#define QCA8084_DEBUG_DATA			0x1E
#define QCA8084_PHY_DEBUG_ANA_ICC		0x280
#define QCA8084_PHY_DEBUG_ANA_ICC_MASK		0x1f

/* QCA8084 ADC clock edge */
#define QCA8084_ADC_CLK_SEL			0x8b80
#define QCA8084_ADC_CLK_SEL_ACLK		GENMASK(7, 4)
#define QCA8084_ADC_CLK_SEL_ACLK_FALL		0xf
#define QCA8084_ADC_CLK_SEL_ACLK_RISE		0x0

#define QCA8084_MSE_THRESHOLD			0x800a
#define QCA8084_MSE_THRESHOLD_2P5G_VAL		0x51c6

/* QCA8084 FIFO reset control */
#define QCA8084_FIFO_CONTROL			0x19
#define QCA8084_FIFO_MAC_2_PHY			BIT(1)
#define QCA8084_FIFO_PHY_2_MAC			BIT(0)

#define QCA8084_MMD7_IPG_OP			0x901d
#define QCA8084_IPG_10_TO_11_EN			BIT(0)

#define QCA8084_HIGH_ADDR_PREFIX		0x18
#define QCA8084_LOW_ADDR_PREFIX			0x10

/* Bottom two bits of REG must be zero */
#define QCA8084_MII_REG_MASK			GENMASK(4, 0)
#define QCA8084_MII_PHY_ADDR_MASK		GENMASK(7, 5)
#define QCA8084_MII_PAGE_MASK			GENMASK(23, 8)
#define QCA8084_MII_SW_ADDR_MASK		GENMASK(31, 24)
#define QCA8084_MII_REG_DATA_UPPER_16_BITS	BIT(1)

#define QCA8084_EPHY_CFG			0xc90f018
#define QCA8084_EPHY_ADDR0_MASK			GENMASK(4, 0)
#define QCA8084_EPHY_ADDR1_MASK			GENMASK(9, 5)
#define QCA8084_EPHY_ADDR2_MASK			GENMASK(14, 10)
#define QCA8084_EPHY_ADDR3_MASK			GENMASK(19, 15)
#define QCA8084_EPHY_LDO_EN			GENMASK(21, 20)

#define QCA8084_WORK_MODE_CFG			0xc90f030
#define QCA8084_WORK_MODE_MASK			GENMASK(5, 0)
#define QCA8084_WORK_MODE_QXGMII		(BIT(5) | GENMASK(3, 0))
#define QCA8084_WORK_MODE_QXGMII_PORT4_SGMII	(BIT(5) | GENMASK(2, 0))
#define QCA8084_WORK_MODE_SWITCH		BIT(4)
#define QCA8084_WORK_MODE_SWITCH_PORT4_SGMII	BIT(5)

#define QCA8084_DEBUG_AFE25_CMN_2_MII		0x180
#define QCA8084_DEBUG_AFE25_LDO_EN		BIT(13)

#define QCA8084_CALIBRATION_PHY1_EFUSE	0xC900048
#define QCA8084_CALIBRATION_PHY2_EFUSE		0xC90005C
#define QCA8084_CALIBRATION_PHY3_EFUSE		0xC900060
#define QCA8084_CALIBRATION_PHY4_EFUSE		0xC900068
#define QCA8084_PTE_EFUSE		0xC900014

struct qca8084_shared_priv {
	int work_mode;
	int (*phy_qusgmii_mode_set)(u32 dev_id);
	int (*phy_sgmii_mode_set)(u32 dev_id, u32 mode);
	void (*phy_clk_init)(u32 dev_id, u32 clk_mode, u32 pbmp);
	int (*phy_xpcs_autoneg_restart)(u32 dev_id, u32 phy_index);
	int (*phy_speed_clk_set)(u32 dev_id, u32 phy_index, u32 speed);
	int (*phy_clk_en_set)(u32 dev_id, u32 phy_index, u8 mask, bool enable);
	int (*phy_clk_reset)(u32 dev_id, u32 phy_index, u8 mask);
	int (*phy_xpcs_function_reset)(u32 dev_id, u32 phy_index);
	int (*phy_sgmii_function_reset)(u32 dev_id, u32 uphy_index);
};

struct qca8084_priv {
	u32 icc_value;
};

static int qca8084_debug_reg_read(struct phy_device *phydev, u16 reg)
{
	int ret;

	ret = phy_write(phydev, QCA8084_DEBUG_ADDR, reg);
	if (ret < 0)
		return ret;

	return phy_read(phydev, QCA8084_DEBUG_DATA);
}

static int qca8084_debug_reg_mask(struct phy_device *phydev, u16 reg,
			  u16 clear, u16 set)
{
	u16 val;
	int ret;

	ret = qca8084_debug_reg_read(phydev, reg);
	if (ret < 0)
		return ret;

	val = ret & 0xffff;
	val &= ~clear;
	val |= set;

	return phy_write(phydev, QCA8084_DEBUG_DATA, val);
}

static int __qca8084_set_page(struct mii_bus *bus, u16 sw_addr, u16 page)
{
	return __mdiobus_write(bus, QCA8084_HIGH_ADDR_PREFIX | (sw_addr >> 5),
			       sw_addr & 0x1f, page);
}

static void __qca8084_mii_add_split(u32 regaddr, u16 *reg, u16 *addr,
	u16 *page, u16 *sw_addr)
{
	*reg = FIELD_GET(QCA8084_MII_REG_MASK, regaddr);
	*addr = FIELD_GET(QCA8084_MII_PHY_ADDR_MASK, regaddr);
	*page = FIELD_GET(QCA8084_MII_PAGE_MASK, regaddr);
	*sw_addr = FIELD_GET(QCA8084_MII_SW_ADDR_MASK, regaddr);
}

static int __qca8084_mii_read(struct mii_bus *bus, u16 addr, u16 reg,
	u32 *val)
{
	int ret, data;

	ret = __mdiobus_read(bus, addr, reg);
	if (ret < 0)
		return ret;

	data = ret;
	ret = __mdiobus_read(bus, addr,
			     reg | QCA8084_MII_REG_DATA_UPPER_16_BITS);
	if (ret < 0)
		return ret;

	*val =  data | ret << 16;

	return 0;
}

static int qca8084_mii_read(struct phy_device *phydev, u32 regaddr,
	u32 *val)
{
	int ret;
	u16 reg, addr, page, sw_addr;
	struct mii_bus *bus;

	bus = phydev->mdio.bus;
	mutex_lock(&bus->mdio_lock);
	__qca8084_mii_add_split(regaddr, &reg, &addr, &page, &sw_addr);
	ret = __qca8084_set_page(bus, sw_addr, page);
	if (ret < 0)
		goto qca8084_mii_read_exit;
	ret = __qca8084_mii_read(bus, QCA8084_LOW_ADDR_PREFIX | addr,
		reg, val);

qca8084_mii_read_exit:
	mutex_unlock(&bus->mdio_lock);

	return ret;
}

int qca8084_phy_index_get(struct phy_device *phydev)
{
	return (phydev->mdio.addr - phydev->shared->addr + 1);
}

int qca8084_phy_fifo_reset(struct phy_device *phydev, bool enable)
{
	u16 phy_data = 0;

	if (!enable)
		phy_data |= QCA8084_PHY_FIFO_RESET;

	return phy_modify(phydev, QCA8084_PHY_FIFO_CONTROL,
		QCA8084_PHY_FIFO_RESET, phy_data);
}

static int qca8084_qusgmii_speed_fix_up(struct phy_device *phydev,
	struct qca8084_shared_priv *shared_priv)
{
	int phy_index;
	bool phy_clock_en = false;

	phy_index = qca8084_phy_index_get(phydev);

	if (!shared_priv->phy_xpcs_autoneg_restart)
		return -EINVAL;
	shared_priv->phy_xpcs_autoneg_restart(0, phy_index);
	if (!shared_priv->phy_speed_clk_set)
		return -EINVAL;
	shared_priv->phy_speed_clk_set(0, phy_index, phydev->speed);
	if (phydev->link)
		phy_clock_en = true;
	if (!shared_priv->phy_clk_en_set)
		return -EINVAL;
	shared_priv->phy_clk_en_set(0, phy_index, GENMASK(1, 0),
		phy_clock_en);
	mdelay(100);
	if (!shared_priv->phy_clk_reset)
		return -EINVAL;
	shared_priv->phy_clk_reset(0, phy_index, GENMASK(1, 0));
	if (!shared_priv->phy_xpcs_function_reset)
		return -EINVAL;
	shared_priv->phy_xpcs_function_reset(0, phy_index);
	qca8084_phy_fifo_reset(phydev, true);
	mdelay(50);
	if (phydev->link)
		qca8084_phy_fifo_reset(phydev, false);
	/*change IPG from 10 to 11 for 1G speed*/
	phy_modify_mmd(phydev, MDIO_MMD_AN, QCA8084_MMD7_IPG_OP,
		QCA8084_IPG_10_TO_11_EN,
		phydev->speed == SPEED_1000 ?
		QCA8084_IPG_10_TO_11_EN : 0);

	return 0;
}

static int qca8084_sgmii_speed_fix_up(struct phy_device *phydev,
	struct qca8084_shared_priv *shared_priv)
{
	int phy_index;

	phy_index = qca8084_phy_index_get(phydev);
	if (phy_index != 4)
		return -EOPNOTSUPP;
	phydev_dbg(phydev, "disable ethphy3 and uniphy0 clock\n");
	if (!shared_priv->phy_clk_en_set)
		return -EINVAL;
	shared_priv->phy_clk_en_set(0, phy_index, BIT(0), false);
	shared_priv->phy_clk_en_set(0, phy_index + 1, BIT(1), false);
	if (!shared_priv->phy_speed_clk_set)
		return -EINVAL;
	shared_priv->phy_speed_clk_set(0, phy_index, phydev->speed);
	if (phydev->link) {
		shared_priv->phy_clk_en_set(0, phy_index,
			BIT(0), true);
		shared_priv->phy_clk_en_set(0, phy_index + 1,
			BIT(1), true);
	}
	if (!shared_priv->phy_clk_reset)
		return -EINVAL;
	shared_priv->phy_clk_reset(0, phy_index, BIT(0));
	shared_priv->phy_clk_reset(0, phy_index+1, BIT(1));
	if (!shared_priv->phy_sgmii_function_reset)
		return -EINVAL;
	shared_priv->phy_sgmii_function_reset(0, 0);
	qca8084_phy_fifo_reset(phydev, true);
	mdelay(50);
	qca8084_phy_fifo_reset(phydev, false);

	return 0;
}

static int qca8084_interface_fix_up(struct phy_device *phydev,
	struct qca8084_shared_priv *shared_priv)
{
	u32 interface_old;

	interface_old = phydev->interface;
	if (phydev->link && phydev->speed == SPEED_2500)
		phydev->interface = PHY_INTERFACE_MODE_2500BASEX;
	else
		phydev->interface = PHY_INTERFACE_MODE_SGMII;

	if (phydev->interface == interface_old)
		return 0;

	if (!shared_priv->phy_sgmii_mode_set)
		return -EINVAL;
	shared_priv->phy_sgmii_mode_set(0, phydev->interface);

	return 0;
}

static int qca8084_phy_icc_fix_up(struct phy_device *phydev)
{
	int ret = 0;
	u32 icc_value = 0;
	struct qca8084_priv *priv = phydev->priv;

	/* retrim icc value for link up 100M, and set orginal icc value */
	/* for link down and other speeds to fix 100M template issue and */
	if (phydev->speed == SPEED_100) {
		if (priv->icc_value < (0x1f - 3))
			icc_value = priv->icc_value + 3;
		else
			icc_value = 0x1f;
	} else {
		icc_value = priv->icc_value;
	}
	ret = qca8084_debug_reg_mask(phydev, QCA8084_PHY_DEBUG_ANA_ICC,
		QCA8084_PHY_DEBUG_ANA_ICC_MASK, icc_value);
	mdelay(10);

	return ret;
}

static int qca8084_link_change(struct phy_device *phydev)
{
	int ret, phy_index;
	struct qca8084_shared_priv *shared_priv;

	shared_priv = phydev->shared->priv;
	if (!shared_priv)
		return -EINVAL;

	phydev_dbg(phydev, "qca8084 would be fix up when link changed\n");

	ret = qca8084_phy_icc_fix_up(phydev);
	if (ret < 0)
		return ret;

	switch (shared_priv->work_mode) {
	case QCA8084_WORK_MODE_QXGMII:
		qca8084_qusgmii_speed_fix_up(phydev, shared_priv);
		break;
	case QCA8084_WORK_MODE_SWITCH_PORT4_SGMII:
		phy_index = qca8084_phy_index_get(phydev);
		if (phy_index != 4)
			return 0;
		qca8084_interface_fix_up(phydev, shared_priv);
		qca8084_sgmii_speed_fix_up(phydev, shared_priv);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int qca8084_read_specific_status(struct phy_device *phydev)
{
	int spec_status, speed;

	spec_status = phy_read(phydev, QCA8084_SPECIFIC_STATUS);

	if (spec_status & QCA8084_SS_SPEED_DUPLEX_RESOLVED) {

		speed = FIELD_GET(QCA8084_SS_SPEED_MASK, spec_status);

		switch (speed) {
		case QCA8084_SS_SPEED_10:
			phydev->speed = SPEED_10;
			break;
		case QCA8084_SS_SPEED_100:
			phydev->speed = SPEED_100;
			break;
		case QCA8084_SS_SPEED_1000:
			phydev->speed = SPEED_1000;
			break;
		case QCA8084_SS_SPEED_2500:
			phydev->speed = SPEED_2500;
			break;
		}
		if (spec_status & QCA8084_SS_DUPLEX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;

		if (spec_status & QCA8084_SS_MDIX)
			phydev->mdix = ETH_TP_MDI_X;
		else
			phydev->mdix = ETH_TP_MDI;

	}

	return 0;
}

static int qca8084_read_status(struct phy_device *phydev)
{
	int ret, old_link;

	old_link = phydev->link;

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_STAT);
	if (ret < 0)
		return ret;

	linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
		phydev->lp_advertising, ret & MDIO_AN_10GBT_STAT_LP2_5G);

	ret = genphy_read_status(phydev);
	if (ret)
		return ret;

	ret = qca8084_read_specific_status(phydev);
	if (ret < 0)
		return ret;

	if (phydev->link != old_link)
		qca8084_link_change(phydev);

	return 0;
}

static int qca8084_get_features(struct phy_device *phydev)
{
	int features[] = {
		ETHTOOL_LINK_MODE_10baseT_Half_BIT,
		ETHTOOL_LINK_MODE_10baseT_Full_BIT,
		ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		ETHTOOL_LINK_MODE_100baseT_Full_BIT,
		ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
		ETHTOOL_LINK_MODE_Pause_BIT,
		ETHTOOL_LINK_MODE_Asym_Pause_BIT,
		ETHTOOL_LINK_MODE_Autoneg_BIT,
	};

	linkmode_set_bit_array(features, ARRAY_SIZE(features),
		phydev->supported);

	return 0;
}

static int qca8084_config_aneg(struct phy_device *phydev)
{
	int ret, phy_ctrl = 0, duplex = 0;

	if (phydev->autoneg == AUTONEG_DISABLE) {
		duplex = phydev->duplex;
		if (phydev->duplex == DUPLEX_HALF)
			phydev->duplex = DUPLEX_FULL;
		genphy_c45_pma_setup_forced(phydev);
		phydev->duplex = duplex;
	}

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
		phydev->advertising))
		phy_ctrl = MDIO_AN_10GBT_CTRL_ADV2_5G;

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN,
		MDIO_AN_10GBT_CTRL,
		MDIO_AN_10GBT_CTRL_ADV2_5G, phy_ctrl);
	if (ret < 0)
		return ret;

	return __genphy_config_aneg(phydev, ret);
}

static int qca8084_phy_package_config_init_once(struct phy_device *phydev)
{
	int ret;
	u32 val;
	struct qca8084_shared_priv *shared_priv;

	shared_priv = phydev->shared->priv;
	if (!shared_priv)
		return -EINVAL;
	/*get work mode and configure the interface mode*/
	ret = qca8084_mii_read(phydev, QCA8084_WORK_MODE_CFG, &val);
	if (ret < 0)
		return ret;
	if ((val & QCA8084_WORK_MODE_QXGMII) == QCA8084_WORK_MODE_QXGMII) {
		if (!shared_priv->phy_qusgmii_mode_set)
			return -EINVAL;
		shared_priv->phy_qusgmii_mode_set(0);
		if (!shared_priv->phy_clk_init)
			return -EINVAL;
		shared_priv->phy_clk_init(0,
			QCA8084_WORK_MODE_QXGMII, 0);
		shared_priv->work_mode = QCA8084_WORK_MODE_QXGMII;
	} else if ((val & QCA8084_WORK_MODE_SWITCH)
		== QCA8084_WORK_MODE_SWITCH) {
		shared_priv->work_mode = QCA8084_WORK_MODE_SWITCH;
	} else if ((val & QCA8084_WORK_MODE_SWITCH_PORT4_SGMII)
		== QCA8084_WORK_MODE_SWITCH_PORT4_SGMII) {
		shared_priv->work_mode
			= QCA8084_WORK_MODE_SWITCH_PORT4_SGMII;
	} else {
		return -EOPNOTSUPP;
	}

	return ret;
}

static int qca8084_ability_fix_up(struct phy_device *phydev)
{
	int phy_index;

	struct qca8084_shared_priv *shared_priv;

	shared_priv = phydev->shared->priv;
	if (!shared_priv)
		return -EINVAL;

	phy_index = qca8084_phy_index_get(phydev);

	if ((shared_priv->work_mode == QCA8084_WORK_MODE_QXGMII) ||
	((shared_priv->work_mode ==
	QCA8084_WORK_MODE_SWITCH_PORT4_SGMII) &&
	(phy_index == 4))) {
		linkmode_clear_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
			phydev->supported);
		linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
			phydev->supported);
		linkmode_clear_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
			phydev->advertising);
		linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
			phydev->advertising);
	}

	return 0;
}

static int qca8084_icc_efuse_init(struct phy_device *phydev)
{
	int ret = 0;
	u32 data = 0, otp_ver = 0, phy_index = 0, icc_value = 0;
	struct qca8084_priv *priv = phydev->priv;

	phy_index = qca8084_phy_index_get(phydev);

	switch (phy_index) {
	case 1:
		ret = qca8084_mii_read(phydev,
			QCA8084_CALIBRATION_PHY1_EFUSE, &data);
		if (ret < 0)
			return ret;
		icc_value = FIELD_GET(GENMASK(26, 22), data);
		break;
	case 2:
		ret = qca8084_mii_read(phydev,
			QCA8084_CALIBRATION_PHY2_EFUSE, &data);
		if (ret < 0)
			return ret;
		icc_value = FIELD_GET(GENMASK(31, 27), data);
		break;
	case 3:
		ret = qca8084_mii_read(phydev,
			QCA8084_CALIBRATION_PHY3_EFUSE, &data);
		if (ret < 0)
			return ret;
		icc_value = FIELD_GET(GENMASK(31, 27), data);
		break;
	case 4:
		ret = qca8084_mii_read(phydev,
			QCA8084_CALIBRATION_PHY4_EFUSE, &data);
		if (ret < 0)
			return ret;
		icc_value = FIELD_GET(GENMASK(22, 18), data);
		break;
	default:
		return -EOPNOTSUPP;
	}
	ret = qca8084_mii_read(phydev, QCA8084_PTE_EFUSE,
		&data);
	if (ret < 0)
		return ret;
	otp_ver = FIELD_GET(GENMASK(23, 16), data);
	/* the bit 4 of OTP version 1 and OTP version 2 is correct, */
	/* so no need below fixup, other OTP version is incorrect, */
	/* may be 0 or 1, so need to get the opposite value */
	if (otp_ver != 1 && otp_ver != 2) {
		if (icc_value & BIT(4))
			icc_value &= ~BIT(4);
		else
			icc_value |= BIT(4);
	}

	priv->icc_value = icc_value;

	return ret;
}

static int qca8084_config_init(struct phy_device *phydev)
{
	int ret, index;

	if (phy_package_init_once(phydev)) {
		ret = qca8084_phy_package_config_init_once(phydev);
		if (ret < 0)
			return ret;
	}

	/* Disable the LDO2 and LDO3 which are not used */
	index = qca8084_phy_index_get(phydev);
	if (index == 2 || index == 3) {
		qca8084_debug_reg_mask(phydev,
			QCA8084_DEBUG_AFE25_CMN_2_MII,
			QCA8084_DEBUG_AFE25_LDO_EN, 0);
	}
	/* Configure the ADC to convert the signal using falling edge
	 * instead of the default rising edge.
	 */
	ret = qca8084_debug_reg_mask(phydev, QCA8084_ADC_CLK_SEL,
		QCA8084_ADC_CLK_SEL_ACLK,
		FIELD_PREP(QCA8084_ADC_CLK_SEL_ACLK,
		QCA8084_ADC_CLK_SEL_ACLK_FALL));
	if (ret < 0)
		return ret;

	/* Adjust MSE threshold value to avoid link issue with
	 * some link partner.
	 */
	ret = phy_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA8084_MSE_THRESHOLD,
		QCA8084_MSE_THRESHOLD_2P5G_VAL);
	if (ret < 0)
		return ret;

	/* initialize the icc value */
	ret = qca8084_icc_efuse_init(phydev);
	if (ret < 0)
		return ret;

	return qca8084_ability_fix_up(phydev);
}

static int qca8084_probe(struct phy_device *phydev)
{
	u32 val;
	int ret;
	struct qca8084_priv *priv;
	struct device *dev = &phydev->mdio.dev;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	phydev->priv = priv;

	ret = qca8084_mii_read(phydev, QCA8084_EPHY_CFG, &val);
	if (ret < 0)
		return ret;

	devm_phy_package_join(&phydev->mdio.dev, phydev,
		FIELD_GET(QCA8084_EPHY_ADDR0_MASK, val),
		sizeof(struct qca8084_shared_priv));

	return 0;
}

static struct phy_driver qca8084_driver[] = {
{
	/* Qualcomm QCA8084 */
	PHY_ID_MATCH_MODEL(QCA8084_PHY_ID),
	.name			= "Qualcomm QCA8084",
	.flags			= PHY_POLL_CABLE_TEST,
	.get_features		= qca8084_get_features,
	.config_aneg		= qca8084_config_aneg,
	.suspend		= genphy_suspend,
	.resume			= genphy_resume,
	.read_status		= qca8084_read_status,
	.soft_reset		= genphy_soft_reset,
	.config_init		= qca8084_config_init,
	.probe			= qca8084_probe,
},
};

module_phy_driver(qca8084_driver);
MODULE_LICENSE("Dual BSD/GPL");
