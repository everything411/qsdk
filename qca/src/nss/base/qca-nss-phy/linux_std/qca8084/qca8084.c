/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/sysfs.h>
#include <linux/timekeeping.h>
#include "../qcom_phy_lib.h"
#include "../clock/qca8k_clk.h"

#define QCA8084_PHY_ID				0x004dd180

#define QCA8084_SPECIFIC_FUNCTION_CONTROL	0x10
#define QCA8084_AUTO_SOFT_RESET_EN		0x8
#define QCA8084_MDI_MASK			GENMASK(6, 5)
#define QCA8084_MDI_AUTO			0x3
#define QCA8084_MDI_X				0x1
#define QCA8084_MDI				0
#define QCA8084_SPECIFIC_STATUS			0x11
#define QCA8084_SS_SPEED_MASK			GENMASK(9, 7)
#define QCA8084_SS_SPEED_2500			4
#define QCA8084_SS_SPEED_1000			2
#define QCA8084_SS_SPEED_100			1
#define QCA8084_SS_SPEED_10			0
#define QCA8084_SS_DUPLEX			BIT(13)
#define QCA8084_SS_SPEED_DUPLEX_RESOLVED		BIT(11)
#define QCA8084_SS_MDIX				BIT(6)
#define QCA8084_SFC_MDI_CROSSOVER_MODE_M	GENMASK(6, 5)

#define QCA8084_PHY_FIFO_CONTROL		0x19
#define QCA8084_PHY_FIFO_RESET			0x3

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

#define QCA8084_CALIBRATION_PHY1_EFUSE		0xC900048
#define QCA8084_CALIBRATION_PHY2_EFUSE		0xC90005C
#define QCA8084_CALIBRATION_PHY3_EFUSE		0xC900060
#define QCA8084_CALIBRATION_PHY4_EFUSE		0xC900068
#define QCA8084_PTE_EFUSE			0xC900014

#define QCA8084_DEBUG_ANEG_STAT			0x1f
#define QCA8084_DEBUG_AUTONEG_FAIL_CNT_MASK	GENMASK(7, 4)

#define QCA8084_PCS_STATUS1			0x1
#define QCA8084_EEE_TX_LPI			BIT(11)

#define QCA8084_ANEG_FAIL_CNT_THRESHOLD		2

enum qca8084_init_state {
	QCA8084_INIT_STATE_START = 0,
	QCA8084_INIT_STATE_ICC_EFUSE_FAILURE,
	QCA8084_INIT_STATE_ABILITY_FIXUP_FAILURE,
	QCA8084_INIT_STATE_SUCCESS,
	QCA8084_INIT_STATE_INVALID = 0xff,
};

struct qca8084_debug_stats {
	atomic64_t read_status_count;
	atomic64_t config_aneg_count;
	atomic64_t fifo_reset_count;
};

struct qca8084_priv {
	u32 icc_value;
	enum qca8084_init_state init_state;
	struct qca8084_debug_stats debug_stats;
	__ETHTOOL_DECLARE_LINK_MODE_MASK(eee_disabled_by_wa);
};

static void
qca8084_priv_atomic64_inc(struct phy_device *phydev, atomic64_t *v)
{
	struct qca8084_priv *priv = phydev->priv;

	if (priv && v)
		atomic64_inc(v);
}

static void
qca8084_set_init_state(struct phy_device *phydev, enum qca8084_init_state state)
{
	struct qca8084_priv *priv = phydev->priv;

	if (priv)
		priv->init_state = state;
}

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

static int qca8084_phy_index_get(struct phy_device *phydev)
{
	return (phydev->mdio.addr - phydev->shared->addr + 1);
}

static int qca8084_phy_fifo_reset(struct phy_device *phydev, bool enable)
{
	u16 phy_data = 0;

	qca8084_priv_atomic64_inc(phydev,
		&((struct qca8084_priv *)phydev->priv)->debug_stats.fifo_reset_count);

	if (!enable)
		phy_data |= QCA8084_PHY_FIFO_RESET;

	return phy_modify(phydev, QCA8084_PHY_FIFO_CONTROL,
		QCA8084_PHY_FIFO_RESET, phy_data);
}

static int qca8084_phy_qusgmii_speed_fix_up(struct phy_device *phydev)
{
	u32 phy_index;
	bool phy_clock_en = false;

	phy_index = qca8084_phy_index_get(phydev);

	qcom_phy_xpcs_autoneg_restart(phydev, XPCS_ADDR_OFFSET, phy_index);
	qcom_phy_pcs_speed_clock_set(phydev, phy_index, phydev->speed);
	if (phydev->link)
		phy_clock_en = true;
	qca8k_port_clk_en_set(&phydev->mdio, phy_index, GENMASK(1, 0),
		phy_clock_en);
	mdelay(100);
	qca8k_port_clk_reset(&phydev->mdio, phy_index, GENMASK(1, 0));
	qcom_phy_pcs_usxgmii_reset(phydev, PCS1_ADDR_OFFSET, phy_index);
	qcom_phy_xpcs_qusgmii_function_reset(phydev, XPCS_ADDR_OFFSET, phy_index);
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

static int qca8084_phy_sgmii_speed_fix_up(struct phy_device *phydev)
{
	int phy_index;

	phy_index = qca8084_phy_index_get(phydev);
	if (phy_index != 4)
		return -EOPNOTSUPP;

	qca8k_port_clk_en_set(&phydev->mdio, phy_index, BIT(0), false);
	qca8k_port_clk_en_set(&phydev->mdio, phy_index + 1, BIT(1), false);
	qcom_phy_pcs_speed_clock_set(phydev, phy_index, phydev->speed);
	if (phydev->link) {
		qca8k_port_clk_en_set(&phydev->mdio, phy_index, BIT(0), true);
		qca8k_port_clk_en_set(&phydev->mdio, phy_index + 1, BIT(1), true);
	}
	qca8k_port_clk_reset(&phydev->mdio, phy_index, BIT(0));
	qca8k_port_clk_reset(&phydev->mdio, phy_index+1, BIT(1));
	qcom_phy_pcs_sgmii_function_reset(phydev, PCS0_ADDR_OFFSET);
	qcom_phy_pcs_ipg_tune_reset(phydev, PCS0_ADDR_OFFSET);
	qca8084_phy_fifo_reset(phydev, true);
	mdelay(50);
	qca8084_phy_fifo_reset(phydev, false);

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

/*
 * qca8084_phy_eee_wa - Workaround for 2.5G EEE compatibility issue with Intel X550 NIC.
 *
 * When TX LPI is active and auto-negotiation failures exceed the threshold,
 * disable 2.5G EEE advertisement to allow stable 2.5G link negotiation.
 * Re-enable 2.5G EEE when conditions normalize (no TX LPI or low failure count).
 */
static int qca8084_phy_eee_wa(struct phy_device *phydev)
{
	int ret;
	u32 cnt = 0;
	u16 eee_2p5_adv = 0;
	bool txlpi = false;
	struct qca8084_priv *priv = phydev->priv;

	if (!priv)
		return 0;

	if (!phydev->eee_enabled)
		return 0;

	ret = qca8084_debug_reg_read(phydev, QCA8084_DEBUG_ANEG_STAT);
	if (ret < 0)
		return ret;
	cnt = FIELD_GET(QCA8084_DEBUG_AUTONEG_FAIL_CNT_MASK, ret);

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA8084_PCS_STATUS1);
	if (ret < 0)
		return ret;
	txlpi = !!(ret & QCA8084_EEE_TX_LPI);

	if (txlpi && (cnt >= QCA8084_ANEG_FAIL_CNT_THRESHOLD)) {
		if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, priv->eee_disabled_by_wa))
			return 0;
		eee_2p5_adv = 0;
	} else if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, priv->eee_disabled_by_wa)) {
		eee_2p5_adv = MDIO_EEE_2_5GT;
	} else {
		return 0;
	}

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN,
		MDIO_AN_EEE_ADV2, MDIO_EEE_2_5GT, eee_2p5_adv);
	if (ret < 0)
		return ret;

	if (eee_2p5_adv == 0) {
		linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, priv->eee_disabled_by_wa);
		linkmode_clear_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, phydev->advertising_eee);
	} else {
		linkmode_clear_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, priv->eee_disabled_by_wa);
		linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, phydev->advertising_eee);
	}

	if (ret > 0)
		return genphy_restart_aneg(phydev);

	return ret;
}

static int qca8084_link_change(struct phy_device *phydev)
{
	int ret;

	phydev_dbg(phydev, "qca8084 would be fix up when link changed\n");

	ret = qca8084_phy_icc_fix_up(phydev);
	if (ret < 0)
		return ret;

	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_QUSGMII:
		qca8084_phy_qusgmii_speed_fix_up(phydev);
		break;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_2500BASEX:
		qcom_phy_sgmii_interface_fix_up(phydev);
		qca8084_phy_sgmii_speed_fix_up(phydev);
		break;
	default:
		break;
	}

	if (!phydev->link) {
		ret = qca8084_phy_eee_wa(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int qca8084_phy_mdix_ctrl_get(struct phy_device *phydev)
{
	int ret;

	ret = phy_read(phydev, QCA8084_SPECIFIC_FUNCTION_CONTROL);
	if (ret < 0)
		return ret;
	switch (FIELD_GET(QCA8084_MDI_MASK, ret)) {
	case QCA8084_MDI:
		phydev->mdix_ctrl = ETH_TP_MDI;
		break;
	case QCA8084_MDI_X:
		phydev->mdix_ctrl = ETH_TP_MDI_X;
		break;
	case QCA8084_MDI_AUTO:
		phydev->mdix_ctrl = ETH_TP_MDI_AUTO;
		break;
	default:
		phydev->mdix_ctrl = ETH_TP_MDI_INVALID;
		return -EINVAL;
	}

	return 0;
}

static int qca8084_phy_mdix_ctrl_set(struct phy_device *phydev)
{
	int ret;
	u16 val;

	switch (phydev->mdix_ctrl) {
	case ETH_TP_MDI:
		val = QCA8084_MDI;
		break;
	case ETH_TP_MDI_X:
		val = QCA8084_MDI_X;
		break;
	case ETH_TP_MDI_AUTO:
		val = QCA8084_MDI_AUTO;
		break;
	default:
		return 0;
	}
	ret = phy_modify_changed(phydev, QCA8084_SPECIFIC_FUNCTION_CONTROL,
		QCA8084_MDI_MASK, FIELD_PREP(QCA8084_MDI_MASK, val));
	if (ret > 0) {
		ret = genphy_soft_reset(phydev);
		if (ret < 0)
			return ret;
	}

	return ret;
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

	return qca8084_phy_mdix_ctrl_get(phydev);
}

static int qca8084_read_status(struct phy_device *phydev)
{
	int ret, old_link;

	qca8084_priv_atomic64_inc(phydev,
		&((struct qca8084_priv *)phydev->priv)->debug_stats.read_status_count);
	old_link = phydev->link;

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_STAT);
	if (ret < 0)
		return ret;

	linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
		phydev->lp_advertising, ret & MDIO_AN_10GBT_STAT_LP2_5G);

	ret = genphy_read_status(phydev);
	if (ret < 0)
		return ret;

	ret = qca8084_read_specific_status(phydev);
	if (ret < 0)
		return ret;

	if (phydev->link != old_link) {
		qca8084_link_change(phydev);
	}

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

	genphy_c45_read_eee_abilities(phydev);

	return 0;
}

static int qca8084_config_aneg(struct phy_device *phydev)
{
	int ret, phy_ctrl = 0, duplex = 0;

	qca8084_priv_atomic64_inc(phydev,
		&((struct qca8084_priv *)phydev->priv)->debug_stats.config_aneg_count);

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

	ret = __genphy_config_aneg(phydev, ret);
	if (ret < 0)
		return ret;

	return qca8084_phy_mdix_ctrl_set(phydev);
}

static int qca8084_ability_fix_up(struct phy_device *phydev)
{
	int phy_index;

	phy_index = qca8084_phy_index_get(phydev);

	if (phydev->interface != PHY_INTERFACE_MODE_INTERNAL &&
		phydev->interface != PHY_INTERFACE_MODE_GMII) {
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

static void qca8084_fill_possible_interfaces(struct phy_device *phydev)
{
	unsigned long *possible = phydev->possible_interfaces;

	if (phydev->interface == PHY_INTERFACE_MODE_QUSGMII) {
		__set_bit(PHY_INTERFACE_MODE_QUSGMII, possible);
	} else {
		__set_bit(PHY_INTERFACE_MODE_SGMII, possible);
		__set_bit(PHY_INTERFACE_MODE_2500BASEX, possible);
	}
}

static int qca8084_config_init(struct phy_device *phydev)
{
	int ret = 0, index;
	enum qca8084_init_state state = QCA8084_INIT_STATE_START;

	qca8084_set_init_state(phydev, state);

	if (phydev->interface != PHY_INTERFACE_MODE_INTERNAL &&
		phydev->interface != PHY_INTERFACE_MODE_GMII) {
		if (phy_package_init_once(phydev)) {
			struct qcom_phy_pcs_cfg config = {0};

			config.clock_mode = CLOCK_PHY_MODE;
			config.auto_neg = true;
			config.type = phydev->interface;
			if (phydev->interface == PHY_INTERFACE_MODE_QUSGMII) {
				config.addr_offset = PCS1_ADDR_OFFSET;
				qcom_phy_pcs_interface_set(phydev, config);
				qca8k_gcc_clock_init(&phydev->mdio, QCA8084_WORK_MODE_QXGMII, 0);
			} else if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
				config.addr_offset = PCS0_ADDR_OFFSET;
				qcom_phy_pcs_interface_set(phydev, config);
			}
		}
		qca8084_fill_possible_interfaces(phydev);
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
	qca8084_debug_reg_mask(phydev, QCA8084_ADC_CLK_SEL,
		QCA8084_ADC_CLK_SEL_ACLK,
		FIELD_PREP(QCA8084_ADC_CLK_SEL_ACLK,
		QCA8084_ADC_CLK_SEL_ACLK_FALL));
	/* Adjust MSE threshold value to avoid link issue with
	 * some link partner.
	 */
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA8084_MSE_THRESHOLD,
		QCA8084_MSE_THRESHOLD_2P5G_VAL);
	/* initialize the icc value */
	ret = qca8084_icc_efuse_init(phydev);
	if (ret < 0) {
		state = QCA8084_INIT_STATE_ICC_EFUSE_FAILURE;
		goto err_out;
	}

	ret = qca8084_ability_fix_up(phydev);
	if (ret < 0) {
		state = QCA8084_INIT_STATE_ABILITY_FIXUP_FAILURE;
		goto err_out;
	}

	state = QCA8084_INIT_STATE_SUCCESS;

	/* Reset the EEE workaround state on (re-)initialization to avoid
	 * stale state if config_init is called while the workaround has
	 * disabled 2.5G EEE.
	 */
	if (phydev->priv)
		linkmode_zero(((struct qca8084_priv *)phydev->priv)->eee_disabled_by_wa);

err_out:
	qca8084_set_init_state(phydev, state);
	return ret;
}

static ssize_t qca8084_phy_show_debug_module_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct qca8084_priv *priv = phydev->priv;
	ssize_t len = 0;

	if (!priv)
		return -EINVAL;

	switch (priv->init_state) {
	case QCA8084_INIT_STATE_START:
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%-24s : Success\n", "PHY INIT START");
		break;
	case QCA8084_INIT_STATE_ICC_EFUSE_FAILURE:
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%-24s : Failure\n", "PHY ICC EFUSE");
		break;
	case QCA8084_INIT_STATE_ABILITY_FIXUP_FAILURE:
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%-24s : Failure\n", "PHY ABILITY FIXUP");
		break;
	case QCA8084_INIT_STATE_SUCCESS:
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%-24s : Success\n", "PHY INIT");
		break;
	case QCA8084_INIT_STATE_INVALID:
	default:
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%-24s : INVALID\n", "PHY INIT");
		break;
	}

	/* PHY Device Structure Information (not available via ethtool) */
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"\n%s\n", "PHY Device Internal State");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s\n", "is_c45", phydev->is_c45 ? "true" : "false");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s\n", "suspended", phydev->suspended ? "true" : "false");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s\n", "loopback_enabled", phydev->loopback_enabled ? "true" : "false");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s\n", "interrupts", phydev->interrupts ? "enabled" : "disabled");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s\n", "irq_suspended", phydev->irq_suspended ? "true" : "false");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %d\n", "state", phydev->state);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s (%d)\n", "interface", phy_modes(phydev->interface), phydev->interface);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : %s\n", "eee_enabled", phydev->eee_enabled ? "true" : "false");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : 0x%08x\n", "eee_broken_modes", phydev->eee_broken_modes);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : ", "eee_disabled_by_wa");
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, priv->eee_disabled_by_wa))
		len += scnprintf(buf + len, PAGE_SIZE - len, "2.5G EEE");
	else
		len += scnprintf(buf + len, PAGE_SIZE - len, "none");
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	len += scnprintf(buf + len, PAGE_SIZE - len,
		"\n%s\n", "AFE Configuration");
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"    %-20s : 0x%04x\n", "ICC Value", priv->icc_value);

	return len;
}

static ssize_t qca8084_phy_show_debug_module_statistics(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct qca8084_priv *priv = phydev->priv;
	ssize_t len = 0;

	if (!priv)
		return -EINVAL;

	len += scnprintf(buf + len, PAGE_SIZE - len, "QCA8084 PHY Debug Statistics\n");
	len += scnprintf(buf + len, PAGE_SIZE - len, "    API Call Counters\n");
	len += scnprintf(buf + len, PAGE_SIZE - len, "        Read Status Calls         : %llu\n",
		atomic64_read(&priv->debug_stats.read_status_count));
	len += scnprintf(buf + len, PAGE_SIZE - len, "        Config Aneg Calls         : %llu\n",
		atomic64_read(&priv->debug_stats.config_aneg_count));
	len += scnprintf(buf + len, PAGE_SIZE - len, "        Fifo Reset Calls          : %llu\n",
		atomic64_read(&priv->debug_stats.fifo_reset_count));

	return len;
}

static void _qca8084_phy_debug_module_reset_statistics(struct qca8084_priv *priv)
{
	if (!priv)
		return;

	atomic64_set(&priv->debug_stats.read_status_count, 0);
	atomic64_set(&priv->debug_stats.config_aneg_count, 0);
	atomic64_set(&priv->debug_stats.fifo_reset_count, 0);
}

static ssize_t qca8084_phy_debug_module_reset_statistics(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct qca8084_priv *priv = phydev->priv;

	if (!priv)
		return -EINVAL;

	if (count > 0 && (buf[0] == '0' || buf[0] == '\n'))
		_qca8084_phy_debug_module_reset_statistics(priv);

	return count;
}

static DEVICE_ATTR(module_state, 0444, qca8084_phy_show_debug_module_state, NULL);
static DEVICE_ATTR(module_statistics, 0644, qca8084_phy_show_debug_module_statistics, qca8084_phy_debug_module_reset_statistics);

static void qca8084_sysfs_init(struct phy_device *phydev)
{
	struct qca8084_priv *priv = phydev->priv;

	_qca8084_phy_debug_module_reset_statistics(priv);
	device_create_file(&phydev->mdio.dev, &dev_attr_module_state);
	device_create_file(&phydev->mdio.dev, &dev_attr_module_statistics);
}

static void qca8084_sysfs_exit(struct phy_device *phydev)
{
	device_remove_file(&phydev->mdio.dev, &dev_attr_module_state);
	device_remove_file(&phydev->mdio.dev, &dev_attr_module_statistics);
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
		FIELD_GET(QCA8084_EPHY_ADDR0_MASK, val), 0);

	qca8084_sysfs_init(phydev);

	return 0;
}

static void qca8084_remove(struct phy_device *phydev)
{
	qca8084_sysfs_exit(phydev);
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
	.remove			= qca8084_remove,
},
};

module_phy_driver(qca8084_driver);
MODULE_LICENSE("Dual BSD/GPL");
