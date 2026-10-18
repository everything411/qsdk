/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "nss_phy.h"
#include "qca807x_phy.h"
#include "nss_phy_common.h"

static bool qca807x_phyaddr_is_combo(struct nss_phy_device *nss_phydev)
{
	int addr = 0, addr_shared = 0;

	addr = nss_phy_addr_get(nss_phydev);
	addr_shared = nss_phy_share_addr_get(nss_phydev);

	return (addr == addr_shared + QCA807X_PHY_COMBO_ADDR);
}

static int qca807x_phy_powersave_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	int ret = 0;
	u16 dac_qt_bias_en = 0, vd_half_bias = 0, afe_tx_am = 0;

	if (!enable) {
		dac_qt_bias_en = QCA807X_PHY_MMD3_DAC_AT_BIAS_EN;
		vd_half_bias = QCA807X_PHY_MMD3_VD_HALF_BIAS_EN;
		afe_tx_am = QCA807X_PHY_MMD3_AFE_TX_FULL_AMPLITUDE_EN;
	}
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA807X_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL,
		QCA807X_PHY_MMD3_DAC_AT_BIAS_EN, dac_qt_bias_en);
	if (ret < 0)
		return ret;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA807X_PHY_MMD3_ADDR_CLD_CTRL5,
		QCA807X_PHY_MMD3_VD_HALF_BIAS_EN, vd_half_bias);
	if (ret < 0)
		return ret;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA807X_PHY_MMD3_ADDR_CLD_CTRL3,
		QCA807X_PHY_MMD3_AFE_TX_FULL_AMPLITUDE_EN,
		afe_tx_am);
	if (ret < 0)
		return ret;

	return nss_phy_common_soft_reset(nss_phydev);
}

static int qca807x_phy_powersave_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data = 0, phy_data1 = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA807X_PHY_MMD3_ADDR_CLD_CTRL5);
	phy_data1 = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA807X_PHY_MMD3_ADDR_CLD_CTRL3);
	if (!(phy_data & QCA807X_PHY_MMD3_VD_HALF_BIAS_EN) &&
		!(phy_data1 & QCA807X_PHY_MMD3_AFE_TX_FULL_AMPLITUDE_EN))
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;

}

static int qca807x_phy_function_reset(struct nss_phy_device *nss_phydev,
	enum nss_phy_reset reset_type)
{
	int ret = 0;

	switch (reset_type) {
	case SERDES_RESET:
		ret = nss_phy_write_pcs(nss_phydev, QCA807X_PHY_SERDES_ADDR,
			QCA807X_PHY_SERDES_RESET_REG, QCA807X_PHY_SERDES_RESET);
		if (ret < 0)
			return ret;
		nss_phy_mdelay(100);
		ret = nss_phy_write_pcs(nss_phydev, QCA807X_PHY_SERDES_ADDR,
			QCA807X_PHY_SERDES_RESET_REG,
			QCA807X_PHY_SERDES_RELEASE);
		if (ret < 0)
			return ret;
		break;
	case SOFT_RESET:
		ret = nss_phy_common_function_reset(nss_phydev, SOFT_RESET);
		if (ret < 0)
			return ret;
		break;
	default:
		return NSS_PHY_EOPNOTSUPP;
	}

	return 0;
}

static int qca807x_phy_interface_set(struct nss_phy_device *nss_phydev,
	nss_phy_interface_t interface)
{
	int ret = 0;
	u16 phy_mode = 0, fiber_auto_detect = 0;

	switch (interface) {
	case NSS_PHY_INTERFACE_MODE_PSGMII:
		phy_mode = QCA807X_PHY_INTERFACE_PSGMII;
		break;
	case NSS_PHY_INTERFACE_MODE_1000BASEX:
	case NSS_PHY_INTERFACE_MODE_100BASEX:
		phy_mode = QCA807X_PHY_INTERFACE_PSGMII_COMBO;
		fiber_auto_detect =
			QCA807X_PHY_MMD7_AUTO_DETECTION_EN;
		break;
	case NSS_PHY_INTERFACE_MODE_SGMII:
	case NSS_PHY_INTERFACE_MODE_QSGMII:
		phy_mode = QCA807X_PHY_INTERFACE_QSGMII_SGMII;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}
	ret = nss_phy_modify_pcs(nss_phydev,
		QCA807X_PHY_COMBO_ADDR,
		NSS_PHY_CHIP_CONFIGURATION,
		QCA807X_PHY_INTERFACE_MASK,
		phy_mode);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		QCA807X_PHY_MMD7_FIBER_MODE_AUTO_DETECTION,
		QCA807X_PHY_MMD7_AUTO_DETECTION_EN,
		fiber_auto_detect);
	if (ret < 0)
		return ret;

	ret = qca807x_phy_function_reset(nss_phydev, SERDES_RESET);

	return ret;
}

static int qca807x_phy_interface_get(struct nss_phy_device *nss_phydev,
	nss_phy_interface_t *interface)
{
	int ret = 0;
	u16 phy_data = 0;

	ret = nss_phy_read_pcs(nss_phydev, QCA807X_PHY_COMBO_ADDR,
		NSS_PHY_CHIP_CONFIGURATION);
	if (ret < 0)
		return ret;

	switch (ret & QCA807X_PHY_INTERFACE_MASK) {
	case QCA807X_PHY_INTERFACE_PSGMII:
		*interface = NSS_PHY_INTERFACE_MODE_PSGMII;
		break;
	case QCA807X_PHY_INTERFACE_PSGMII_COMBO:
		if (nss_phy_is_fiber(nss_phydev)) {
			phy_data = nss_phy_read_mmd(nss_phydev,
				NSS_PHY_MMD7_NUM,
				QCA807X_PHY_MMD7_FIBER_MODE_AUTO_DETECTION);
			if (phy_data &
				QCA807X_PHY_MMD7_AUTO_DETECTION_1000BX)
				*interface = NSS_PHY_INTERFACE_MODE_1000BASEX;
			else
				*interface = NSS_PHY_INTERFACE_MODE_100BASEX;
		} else {
			*interface = NSS_PHY_INTERFACE_MODE_PSGMII;
		}
		break;
	case QCA807X_PHY_INTERFACE_QSGMII_SGMII:
		if (qca807x_phyaddr_is_combo(nss_phydev))
			*interface = NSS_PHY_INTERFACE_MODE_SGMII;
		else
			*interface = NSS_PHY_INTERFACE_MODE_QSGMII;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}

	return 0;
}

static u32
qca807x_phy_led_source_map_mmd_reg_get
	(struct nss_phy_device *nss_phydev, u32 source_id)
{
	u16 mmd_reg = 0;

	switch (source_id) {
	case NSS_PHY_LED_SOURCE0:
		mmd_reg = QCA807X_PHY_MMD7_LED_100_N_MAP_CTRL;
		break;
	case NSS_PHY_LED_SOURCE1:
		mmd_reg = QCA807X_PHY_MMD7_LED_1000_N_MAP_CTRL;
		break;
	default:
		nss_phy_err(nss_phydev, "source %d is not support\n",
			source_id);
		break;
	}

	return mmd_reg;
}

static u32
qca807x_phy_led_source_force_mmd_reg_get
	(struct nss_phy_device *nss_phydev, u32 source_id)
{
	u16 mmd_reg = 0;

	switch (source_id) {
	case NSS_PHY_LED_SOURCE0:
		mmd_reg = QCA807X_PHY_MMD7_LED_100_N_FORCE_CTRL;
		break;
	case NSS_PHY_LED_SOURCE1:
		mmd_reg = QCA807X_PHY_MMD7_LED_1000_N_FORCE_CTRL;
		break;
	default:
		nss_phy_err(nss_phydev, "source %d is not support\n",
			source_id);
		break;
	}

	return mmd_reg;
}

static int
qca807x_phy_led_force_set(struct nss_phy_device *nss_phydev,
	u32 source_id, u32 enable, u32 force_mode)
{
	u32 mmd_reg = 0;
	u16 phy_data = 0;

	if (enable)
		nss_phy_common_led_force_to_phy(nss_phydev,
			force_mode, &phy_data);

	mmd_reg = qca807x_phy_led_source_force_mmd_reg_get(nss_phydev,
		source_id);

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		mmd_reg, NSS_PHY_MMD7_LED_FORCE_EN |
		NSS_PHY_MMD7_LED_FORCE_MASK, phy_data);
}

static int
qca807x_phy_led_force_get(struct nss_phy_device *nss_phydev,
	u32 source_id, u32 *enable, u32 *force_mode)
{
	u32 mmd_reg = 0;
	u16 phy_data = 0;
	int ret = 0;

	mmd_reg = qca807x_phy_led_source_force_mmd_reg_get(nss_phydev,
		source_id);
	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		mmd_reg);
	if (phy_data & NSS_PHY_MMD7_LED_FORCE_EN) {
		*enable = !NSS_PHY_FALSE;
		ret = nss_phy_common_led_force_from_phy(nss_phydev,
			force_mode, phy_data);
		if (ret < 0)
			return ret;
	} else {
		*enable = NSS_PHY_FALSE;
	}

	return 0;
}

static int qca807x_phy_led_ctrl_source_set(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	u32 mmd_reg = 0;
	u16 phy_data = 0;
	int ret = 0;

	if (source_id > NSS_PHY_LED_SOURCE1)
		return -NSS_PHY_EOPNOTSUPP;

	ret = nss_phy_common_led_blink_freq_set(nss_phydev, pattern->mode,
		pattern->freq);
	if (ret < 0)
		return ret;

	if (pattern->mode == ACT_PHY_STATUS) {
		ret = qca807x_phy_led_force_set(nss_phydev, source_id,
			NSS_PHY_FALSE, pattern->mode);
		if (ret < 0)
			return ret;
		ret = nss_phy_common_led_to_phy(nss_phydev,
			pattern->phy_status_bmap, &phy_data);
		if (ret < 0)
			return ret;
		mmd_reg = qca807x_phy_led_source_map_mmd_reg_get(nss_phydev,
			source_id);
		ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD7_NUM, mmd_reg,
			phy_data);
		if (ret < 0)
			return ret;
	} else {
		ret = qca807x_phy_led_force_set(nss_phydev, source_id,
			!NSS_PHY_FALSE, pattern->mode);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int
qca807x_phy_led_ctrl_source_get(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	u32 mmd_reg = 0, force_enable = NSS_PHY_FALSE;
	u16 phy_data = 0;
	int ret = 0;

	if (source_id > NSS_PHY_LED_SOURCE1)
		return -NSS_PHY_EOPNOTSUPP;

	pattern->phy_status_bmap = 0;
	ret = qca807x_phy_led_force_get(nss_phydev, source_id,
		&force_enable, &(pattern->mode));
	if (ret < 0)
		return ret;
	if (!force_enable) {
		pattern->mode = ACT_PHY_STATUS;
		mmd_reg = qca807x_phy_led_source_map_mmd_reg_get(nss_phydev,
			source_id);
		phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			mmd_reg);
		ret = nss_phy_common_led_from_phy(nss_phydev,
			&(pattern->phy_status_bmap), phy_data);
		if (ret < 0)
			return ret;
	}
	ret = nss_phy_common_led_blink_freq_get(nss_phydev, pattern->mode,
		&(pattern->freq));

	return ret;
}

int qca807x_phy_fixup(struct nss_phy_device *nss_phydev)
{
	int ret = 0;

	/* change malibu control_dac[2:0] of MMD7 0x801A bit[9:7] */
	/* from 111 to 101 */
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		QCA807X_PHY_MMD7_DAC_CTRL, QCA807X_PHY_MMD7_DAC_CTRL_MASK,
		QCA807X_PHY_MMD7_DAC_CTRL_VALUE);
	if (ret < 0)
		return ret;

	/*disable Extended next page*/
	ret = nss_phy_modify(nss_phydev, NSS_PHY_AUTONEG_ADV,
		NSS_PHY_EXTENDED_NEXT_PAGE_EN, 0);
	if (ret < 0)
		return ret;

	/* workaround to enable AZ transmitting ability */
	ret = nss_phy_pcs_modify_mmd(nss_phydev, QCA807X_PHY_SERDES_ADDR,
		NSS_PHY_MMD1_NUM, QCA807X_PHY_MMD1_PSGMII_MODE_CTRL,
		QCA807X_PHY_MMD1_PSGMII_MODE_CTRL_VALUE,
		QCA807X_PHY_MMD1_PSGMII_MODE_CTRL_VALUE);
	if (ret < 0)
		return ret;

	/* adjust psgmii serdes tx amp */
	ret = nss_phy_write_pcs(nss_phydev, QCA807X_PHY_SERDES_ADDR,
		QCA807X_PHY_PSGMII_TX_DRIVER_1_CTRL,
		QCA807X_PHY_PSGMII_REDUCE_SERDES_TX_AMP);
	if (ret < 0)
		return ret;

	/* to avoid psgmii module goes into hibernation, */
	/* work with psgmii self test */
	ret = nss_phy_pcs_modify_mmd(nss_phydev, QCA807X_PHY_COMBO_ADDR,
		NSS_PHY_MMD3_NUM, NSS_PHY_MMD3_REMOTE_LOOPBACK_CTRL,
		NSS_BIT(1), 0);
	if (ret < 0)
		return ret;

	return 0;
}

int qca807x_phy_ops_init(struct nss_phy_ops *ops)
{
	ops->hibernation_set = nss_phy_common_hibernation_set;
	ops->hibernation_get = nss_phy_common_hibernation_get;
	ops->powersave_set = qca807x_phy_powersave_set;
	ops->powersave_get = qca807x_phy_powersave_get;
	ops->function_reset = qca807x_phy_function_reset;
	ops->interface_set = qca807x_phy_interface_set;
	ops->interface_get = qca807x_phy_interface_get;
	ops->eee_adv_set = nss_phy_common_eee_adv_set;
	ops->eee_adv_get = nss_phy_common_eee_adv_get;
	ops->eee_partner_adv_get = nss_phy_common_eee_partner_adv_get;
	ops->eee_cap_get = nss_phy_common_eee_cap_get;
	ops->eee_status_get = nss_phy_common_eee_status_get;
	ops->ieee_8023az_set = nss_phy_common_8023az_set;
	ops->ieee_8023az_get = nss_phy_common_8023az_get;
	ops->local_loopback_set = nss_phy_common_local_loopback_set;
	ops->local_loopback_get = nss_phy_common_local_loopback_get;
	ops->remote_loopback_set = nss_phy_common_remote_loopback_set;
	ops->remote_loopback_get = nss_phy_common_remote_loopback_get;
	ops->combo_prefer_medium_set = nss_phy_common_combo_prefer_medium_set;
	ops->combo_prefer_medium_get = nss_phy_common_combo_prefer_medium_get;
	ops->combo_medium_status_get = nss_phy_combo_medium_status_get;
	ops->combo_fiber_mode_set = nss_phy_common_combo_fiber_mode_set;
	ops->combo_fiber_mode_get = nss_phy_common_combo_fiber_mode_get;
	ops->led_ctrl_source_set = qca807x_phy_led_ctrl_source_set;
	ops->led_ctrl_source_get = qca807x_phy_led_ctrl_source_get;
	ops->cdt = nss_phy_common_cdt;
	ops->wol_set = nss_phy_common_wol_set;
	ops->wol_get = nss_phy_common_wol_get;
	ops->magic_frame_set = nss_phy_common_magic_frame_set;
	ops->magic_frame_get = nss_phy_common_magic_frame_get;
	ops->mdix_set = nss_phy_common_mdix_set;
	ops->mdix_get = nss_phy_common_mdix_get;
	ops->mdix_status_get = nss_phy_common_mdix_status_get;
	ops->stats_status_set = nss_phy_common_stats_status_set;
	ops->stats_status_get = nss_phy_common_stats_status_get;
	ops->stats_get = nss_phy_common_stats_get;
	ops->intr_mask_set = nss_phy_common_intr_mask_set;
	ops->intr_mask_get = nss_phy_common_intr_mask_get;
	ops->intr_status_get = nss_phy_common_intr_status_get;

	return 0;
}
