/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "nss_phy.h"
#include "nss_phy_common.h"

int nss_phy_common_hibernation_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	return nss_phy_modify_debug(nss_phydev,
		NSS_PHY_DEBUG_PHY_HIBERNATION_CTRL,
		NSS_PHY_DEBUG_HIBERNATION_EN,
		enable ? NSS_PHY_DEBUG_HIBERNATION_EN : 0);
}

int nss_phy_common_hibernation_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	int ret;

	ret = nss_phy_read_debug(nss_phydev,
		NSS_PHY_DEBUG_PHY_HIBERNATION_CTRL);
	if (ret < 0)
		return ret;

	if (ret & NSS_PHY_DEBUG_HIBERNATION_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_powersave_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	return nss_phy_modify_debug(nss_phydev, NSS_PHY_DEBUG_POWER_SAVE,
		NSS_PHY_DEBUG_POWER_SAVE_EN,
		enable ? NSS_PHY_DEBUG_POWER_SAVE_EN : 0);
}

int nss_phy_common_powersave_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	int ret;

	ret = nss_phy_read_debug(nss_phydev, NSS_PHY_DEBUG_POWER_SAVE);
	if (ret < 0)
		return ret;

	if (ret & NSS_PHY_DEBUG_POWER_SAVE_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_combo_prefer_medium_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_medium *phy_medium)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_CHIP_CONFIGURATION);

	*phy_medium = (phy_data & NSS_PHY_PREFER_FIBER) ? MEDIUM_FIBER :
		MEDIUM_COPPER;

	return 0;
}

int nss_phy_common_combo_prefer_medium_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_medium phy_medium)
{
	u16 phy_data = 0;

	if (phy_medium == MEDIUM_FIBER)
		phy_data |= NSS_PHY_PREFER_FIBER;
	else if (phy_medium == MEDIUM_COPPER)
		phy_data &= ~NSS_PHY_PREFER_FIBER;
	else
		return -NSS_PHY_EOPNOTSUPP;

	return nss_phy_modify(nss_phydev, NSS_PHY_CHIP_CONFIGURATION,
		NSS_PHY_PREFER_FIBER, phy_data);
}

int nss_phy_combo_medium_status_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_medium *phy_medium)
{
	if (nss_phy_is_fiber(nss_phydev))
		*phy_medium = MEDIUM_FIBER;
	else
		*phy_medium = MEDIUM_COPPER;

	return 0;
}

int nss_phy_common_eee_adv_set(struct nss_phy_device *nss_phydev,
	u32 adv)
{
	int ret;
	u16 phy_data = 0;

	if (!nss_phydev_eee_support(nss_phydev))
		return -NSS_PHY_EOPNOTSUPP;

	if (adv & EEE_100BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_100M;
	if (adv & EEE_1000BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_1000M;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_CTRL, NSS_PHY_MMD7_EEE_MASK,
		phy_data);
	if (ret < 0)
		return ret;

	nss_phydev_eee_update(nss_phydev, (adv & GE_EEE));

	return nss_phy_common_autoneg_restart(nss_phydev);
}

int nss_phy_common_eee_adv_get(struct nss_phy_device *nss_phydev,
	u32 *adv)
{
	u16 phy_data = 0;

	*adv = 0;
	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_CTRL);
	if (phy_data & NSS_PHY_MMD7_EEE_ADV_100M)
		*adv |= EEE_100BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_ADV_1000M)
		*adv |= EEE_1000BASE_T;

	return 0;
}

int nss_phy_common_eee_partner_adv_get(struct nss_phy_device *nss_phydev,
	u32 *adv)
{
	u16 phy_data = 0;

	*adv = 0;
	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_PARTNER);
	if (phy_data & NSS_PHY_MMD7_EEE_PARTNER_ADV_100M)
		*adv |= EEE_100BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_PARTNER_ADV_1000M)
		*adv |= EEE_1000BASE_T;

	return 0;
}

int nss_phy_common_eee_cap_get(struct nss_phy_device *nss_phydev,
	u32 *cap)
{
	u16 phy_data = 0;

	*cap = 0;
	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_8023AZ_EEE_CAPABILITY);
	if (phy_data & NSS_PHY_MMD3_EEE_CAPABILITY_100M)
		*cap |= EEE_100BASE_T;
	if (phy_data & NSS_PHY_MMD3_EEE_CAPABILITY_1000M)
		*cap |= EEE_1000BASE_T;

	return 0;
}

int nss_phy_common_eee_status_get(struct nss_phy_device *nss_phydev,
	u32 *status)
{
	u16 phy_data = 0;

	*status = 0;
	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_STATUS);

	if (phy_data & NSS_PHY_MMD7_EEE_STATUS_100M)
		*status |= EEE_100BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_STATUS_1000M)
		*status |= EEE_1000BASE_T;

	return 0;
}

int nss_phy_common_8023az_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u32 eee_adv = 0;

	if (enable)
		eee_adv = GE_EEE;

	return nss_phy_common_eee_adv_set(nss_phydev, eee_adv);
}

int nss_phy_common_8023az_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u32 eee_adv = 0, eee_cap = 0;
	int ret = 0;

	ret = nss_phy_common_eee_adv_get(nss_phydev, &eee_adv);
	if (ret < 0)
		return ret;
	ret = nss_phy_common_eee_cap_get(nss_phydev, &eee_cap);
	if (ret < 0)
		return ret;

	if (eee_adv == eee_cap)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_reg_pages_sel(struct nss_phy_device *nss_phydev,
	enum nss_phy_reg_pages phy_reg_pages)
{
	u16 reg_pages = 0;

	reg_pages = nss_phy_read(nss_phydev, NSS_PHY_CHIP_CONFIGURATION);

	if (phy_reg_pages == PAGE_COPPER)
		reg_pages |= NSS_PHY_BT_BX_REG_SEL;

	return nss_phy_modify(nss_phydev, NSS_PHY_CHIP_CONFIGURATION,
		NSS_PHY_BT_BX_REG_SEL, reg_pages);
}

int nss_phy_common_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;
	u32 cur_speed = 0, autoneg = 0;
	int ret = 0;

	if (enable) {
		cur_speed = nss_phydev_speed_get(nss_phydev);
		if (cur_speed == NSS_PHY_SPEED_1000)
			phy_data = NSS_PHY_LOOPBACK_1000M;
		else if (cur_speed == NSS_PHY_SPEED_100)
			phy_data = NSS_PHY_LOOPBACK_100M;
		else
			phy_data = NSS_PHY_LOOPBACK_10M;
	} else {
		phy_data = NSS_PHY_COMMON_CTRL;
		autoneg = !NSS_PHY_FALSE;
	}

	ret = nss_phy_write(nss_phydev, NSS_PHY_CONTROL, phy_data);
	if (ret < 0)
		return ret;

	return nss_phydev_autoneg_update(nss_phydev, autoneg);
}

int nss_phy_common_local_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_CONTROL);

	if (phy_data & NSS_PHY_LOCAL_LOOPBACK_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_remote_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;

	if (enable)
		phy_data |= NSS_PHY_MMD3_REMOTE_LOOPBACK_EN;

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_REMOTE_LOOPBACK_CTRL,
		NSS_PHY_MMD3_REMOTE_LOOPBACK_EN, phy_data);
}

int nss_phy_common_remote_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_REMOTE_LOOPBACK_CTRL);

	if (phy_data & NSS_PHY_MMD3_REMOTE_LOOPBACK_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_combo_fiber_mode_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_fiber_mode fiber_mode)
{
	u16 phy_data = 0;

	if (fiber_mode == FIBER_1000BX)
		phy_data |= NSS_PHY_FIBER_MODE_1000BX;
	else if (fiber_mode == FIBER_100FX)
		phy_data &= ~NSS_PHY_FIBER_MODE_1000BX;
	else
		return -NSS_PHY_EOPNOTSUPP;

	return nss_phy_modify(nss_phydev, NSS_PHY_CHIP_CONFIGURATION,
		NSS_PHY_FIBER_MODE_1000BX, phy_data);
}

int nss_phy_common_combo_fiber_mode_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_fiber_mode *fiber_mode)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_CHIP_CONFIGURATION);

	*fiber_mode = (phy_data & NSS_PHY_FIBER_MODE_1000BX) ?
		FIBER_1000BX : FIBER_100FX;

	return 0;
}

int nss_phy_common_led_from_phy(struct nss_phy_device *nss_phydev,
	u32 *status_bmap, u16 phy_data)
{
	if (phy_data & NSS_PHY_MMD7_LINK_1000M_LIGHT_EN)
		*status_bmap |= NSS_BIT(LED_LINK_1000M_LIGHT_EN);
	if (phy_data & NSS_PHY_MMD7_LINK_100M_LIGHT_EN)
		*status_bmap |= NSS_BIT(LED_LINK_100M_LIGHT_EN);
	if (phy_data & NSS_PHY_MMD7_LINK_10M_LIGHT_EN)
		*status_bmap |= NSS_BIT(LED_LINK_10M_LIGHT_EN);
	if (phy_data & NSS_PHY_MMD7_RX_TRAFFIC_BLINK_EN)
		*status_bmap |= NSS_BIT(LED_RX_TRAFFIC_BLINK_EN);
	if (phy_data & NSS_PHY_MMD7_TX_TRAFFIC_BLINK_EN)
		*status_bmap |= NSS_BIT(LED_TX_TRAFFIC_BLINK_EN);

	return 0;
}

int nss_phy_common_led_to_phy(struct nss_phy_device *nss_phydev,
	u32 status_bmap, u16 *phy_data)
{
	if (status_bmap & NSS_BIT(LED_LINK_1000M_LIGHT_EN))
		*phy_data |= NSS_PHY_MMD7_LINK_1000M_LIGHT_EN;
	if (status_bmap & NSS_BIT(LED_LINK_100M_LIGHT_EN))
		*phy_data |= NSS_PHY_MMD7_LINK_100M_LIGHT_EN;
	if (status_bmap & NSS_BIT(LED_LINK_10M_LIGHT_EN))
		*phy_data |= NSS_PHY_MMD7_LINK_10M_LIGHT_EN;
	if (status_bmap & NSS_BIT(LED_RX_TRAFFIC_BLINK_EN))
		*phy_data |= NSS_PHY_MMD7_RX_TRAFFIC_BLINK_EN;
	if (status_bmap & NSS_BIT(LED_TX_TRAFFIC_BLINK_EN))
		*phy_data |= NSS_PHY_MMD7_TX_TRAFFIC_BLINK_EN;

	return 0;
}

int nss_phy_common_led_force_from_phy(struct nss_phy_device *nss_phydev,
	u32 *force_mode, u16 phy_data)
{
	if (!(phy_data & NSS_PHY_MMD7_LED_FORCE_EN))
		return -NSS_PHY_EINVAL;

	switch (phy_data & NSS_PHY_MMD7_LED_FORCE_MASK) {
	case NSS_PHY_MMD7_LED_FORCE_ALWAYS_OFF:
		*force_mode = ALWAYS_OFF;
		break;
	case NSS_PHY_MMD7_LED_FORCE_ALWAYS_ON:
		*force_mode = ALWAYS_ON;
		break;
	case NSS_PHY_MMD7_LED_FORCE_ALWAYS_BLINK:
		*force_mode = ALWAYS_BLINK;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}

	return 0;
}

int nss_phy_common_led_force_to_phy(struct nss_phy_device *nss_phydev,
	u32 force_mode, u16 *phy_data)
{
	*phy_data |= NSS_PHY_MMD7_LED_FORCE_EN;

	if (force_mode == ALWAYS_OFF)
		*phy_data |= NSS_PHY_MMD7_LED_FORCE_ALWAYS_OFF;
	else if (force_mode == ALWAYS_ON)
		*phy_data |= NSS_PHY_MMD7_LED_FORCE_ALWAYS_ON;
	else if (force_mode == ALWAYS_BLINK)
		*phy_data |= NSS_PHY_MMD7_LED_FORCE_ALWAYS_BLINK;
	else
		return -NSS_PHY_EOPNOTSUPP;

	return 0;
}

int nss_phy_common_led_active_set(struct nss_phy_device *nss_phydev,
	u32 active_level)
{
	u16 phy_data = 0;

	if (active_level == LED_ACTIVE_HIGH)
		phy_data |= NSS_PHY_MMD7_LED_POLARITY_MASK;

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LED_POLARITY_CTRL, NSS_PHY_MMD7_LED_POLARITY_MASK,
		phy_data);
}

int nss_phy_common_led_active_get(struct nss_phy_device *nss_phydev,
	u32 *active_level)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LED_POLARITY_CTRL);
	if (phy_data & NSS_PHY_MMD7_LED_POLARITY_MASK)
		*active_level = LED_ACTIVE_HIGH;

	return 0;
}

int nss_phy_common_led_blink_freq_set(struct nss_phy_device *nss_phydev,
	u32 mode, u32 freq)
{
	u16 blink_freq = 0, blink_freq_mask = 0;

	if (mode == ALWAYS_BLINK) {
		blink_freq_mask = NSS_PHY_MMD7_ALWAYS_BLINK_FREQ_MASK;
		blink_freq = freq << 3;
	} else if (mode == ACT_PHY_STATUS) {
		blink_freq_mask = NSS_PHY_MMD7_MAP_BLINK_FREQ_MASK;
		blink_freq = freq << 9;
	}

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LED_BLINK_FREQ_CTRL, blink_freq_mask,
		blink_freq);
}

int nss_phy_common_led_blink_freq_get(struct nss_phy_device *nss_phydev,
	u32 mode, u32 *freq)
{
	u16 phy_data = 0;

	phy_data =  nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LED_BLINK_FREQ_CTRL);
	if (mode == ALWAYS_BLINK)
		*freq = ((phy_data & NSS_PHY_MMD7_ALWAYS_BLINK_FREQ_MASK) >> 3);
	else if (mode == ACT_PHY_STATUS)
		*freq = ((phy_data & NSS_PHY_MMD7_MAP_BLINK_FREQ_MASK) >> 9);

	return 0;
}

int nss_phy_common_fifo_reset(struct nss_phy_device *nss_phydev, u32 enable)
{
	int phy_data = 0;

	if (!enable)
		phy_data |= NSS_PHY_FIFO_RESET_MASK;

	return nss_phy_modify(nss_phydev, NSS_PHY_FIFO_CONTROL,
		NSS_PHY_FIFO_RESET_MASK, phy_data);
}

int nss_phy_common_soft_reset(struct nss_phy_device *nss_phydev)
{
	return nss_phy_modify(nss_phydev, NSS_PHY_CONTROL,
		NSS_PHY_SOFT_RESET, NSS_PHY_SOFT_RESET);
}

int nss_phy_common_function_reset(struct nss_phy_device *nss_phydev,
	enum nss_phy_reset reset_type)
{
	int ret = 0;

	switch (reset_type) {
	case FIFO_RESET:
		ret = nss_phy_common_fifo_reset(nss_phydev, true);
		if (ret < 0)
			return ret;
		nss_phy_mdelay(50);
		ret = nss_phy_common_fifo_reset(nss_phydev, false);
		if (ret < 0)
			return ret;
		break;
	case SOFT_RESET:
		ret = nss_phy_common_soft_reset(nss_phydev);
		if (ret < 0)
			return ret;
		break;
	default:
		return NSS_PHY_EOPNOTSUPP;
	}

	return 0;
}

int nss_phy_common_autoneg_restart(struct nss_phy_device *nss_phydev)
{
	int ret = 0;

	ret = nss_phy_modify(nss_phydev, NSS_PHY_CONTROL,
			NSS_PHY_AUTONEG_RESTART | NSS_PHY_AUTONEG_EN,
			NSS_PHY_AUTONEG_RESTART | NSS_PHY_AUTONEG_EN);
	if (ret)
		return ret;

	return nss_phydev_autoneg_update(nss_phydev, true);
}

int nss_phy_common_autoneg_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;
	int ret = 0;

	if (enable)
		phy_data |= NSS_PHY_AUTONEG_EN;

	ret = nss_phy_modify(nss_phydev, NSS_PHY_CONTROL,
		NSS_PHY_AUTONEG_EN, phy_data);
	if (ret < 0)
		return ret;

	return nss_phydev_autoneg_update(nss_phydev, enable);
}

int nss_phy_common_mdix_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_mode mode)
{
	u16 phy_data = 0;
	int ret = 0;

	if (mode == MODE_AUTO)
		phy_data = NSS_PHY_MDIX_AUTO;
	else if (mode == MODE_MDIX)
		phy_data = NSS_PHY_MDIX;
	else if (mode == MODE_MDI)
		phy_data = NSS_PHY_MDI;
	else
		return NSS_PHY_EOPNOTSUPP;

	ret = nss_phy_modify(nss_phydev, NSS_PHY_SPEC_CONTROL,
		NSS_PHY_MDIX_AUTO, phy_data);
	if (ret < 0)
		return ret;

	return nss_phy_common_soft_reset(nss_phydev);
}

int nss_phy_common_mdix_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_mode *mode)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_SPEC_CONTROL);

	if ((phy_data & NSS_PHY_MDIX_AUTO) == NSS_PHY_MDIX_AUTO)
		*mode = MODE_AUTO;
	else if ((phy_data & NSS_PHY_MDIX_AUTO) == NSS_PHY_MDIX)
		*mode = MODE_MDIX;
	else
		*mode = MODE_MDI;

	return 0;
}

int nss_phy_common_mdix_status_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_status *mode)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_SPEC_STATUS);

	*mode = (phy_data & NSS_PHY_MDIX_STATUS) ? STATUS_MDIX :
		STATUS_MDI;

	return 0;
}

int nss_phy_common_magic_frame_set(struct nss_phy_device *nss_phydev,
	struct nss_phy_mac *mac)
{
	u16 phy_data1 = 0;
	u16 phy_data2 = 0;
	u16 phy_data3 = 0;
	u16 ret = 0;

	phy_data1 = (mac->uc[0] << 8) | mac->uc[1];
	phy_data2 = (mac->uc[2] << 8) | mac->uc[3];
	phy_data3 = (mac->uc[4] << 8) | mac->uc[5];

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_MAGIC_MAC_CTRL1, phy_data1);
	if (ret < 0)
		return ret;

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_MAGIC_MAC_CTRL2, phy_data2);
	if (ret < 0)
		return ret;

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_MAGIC_MAC_CTRL3, phy_data3);

	return ret;
}

int nss_phy_common_magic_frame_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_mac *mac)
{
	u16 phy_data1 = 0;
	u16 phy_data2 = 0;
	u16 phy_data3 = 0;
	int ret = 0;

	phy_data1 = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_MAGIC_MAC_CTRL1);
	if (ret < 0)
		return ret;

	phy_data2 = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_MAGIC_MAC_CTRL2);
	if (ret < 0)
		return ret;

	phy_data3 = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_MAGIC_MAC_CTRL3);
	if (ret < 0)
		return ret;

	mac->uc[0] = (phy_data1 >> 8);
	mac->uc[1] = (phy_data1 & 0x00ff);
	mac->uc[2] = (phy_data2 >> 8);
	mac->uc[3] = (phy_data2 & 0x00ff);
	mac->uc[4] = (phy_data3 >> 8);
	mac->uc[5] = (phy_data3 & 0x00ff);

	return 0;
}

int nss_phy_common_wol_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;

	if (enable)
		phy_data |= NSS_PHY_MMD3_WOL_EN;

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_CTRL, NSS_PHY_MMD3_WOL_EN, phy_data);
}

int nss_phy_common_wol_get(struct nss_phy_device *nss_phydev,
	u32 *enable)

{
	u16 phy_data = 0;

	*enable = NSS_PHY_FALSE;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_WOL_CTRL);
	if (phy_data & NSS_PHY_MMD3_WOL_EN)
		*enable = !NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_stats_status_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;

	if (enable) {
		phy_data |= NSS_PHY_MMD7_FRAME_CHECK_EN;
		phy_data |= NSS_PHY_MMD7_CNT_SELFCLR;
	}

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_COUNTER_CTRL,
		NSS_PHY_MMD7_FRAME_CHECK_EN | NSS_PHY_MMD7_CNT_SELFCLR,
		phy_data);
}

int nss_phy_common_stats_status_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_COUNTER_CTRL);
	if (phy_data & NSS_PHY_MMD7_FRAME_CHECK_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_common_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_stats_info *stats_info)
{
	u16 ingress_high_counter = 0;
	u16 ingress_low_counter = 0;
	u16 egress_high_counter = 0;
	u16 egress_low_counter = 0;

	ingress_high_counter = nss_phy_read_mmd(nss_phydev,
		NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_INGRESS_COUNTER_HIGH);
	ingress_low_counter = nss_phy_read_mmd(nss_phydev,
		NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_INGRESS_COUNTER_LOW);
	stats_info->RxGoodFrame = (ingress_high_counter << 16) |
		ingress_low_counter;
	stats_info->RxFcsErr = nss_phy_read_mmd(nss_phydev,
		NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_INGRESS_ERROR_COUNTER);

	egress_high_counter = nss_phy_read_mmd(nss_phydev,
		NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_EGRESS_COUNTER_HIGH);
	egress_low_counter = nss_phy_read_mmd(nss_phydev,
		NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_EGRESS_COUNTER_LOW);
	stats_info->TxGoodFrame = (egress_high_counter << 16) |
		egress_low_counter;
	stats_info->TxFcsErr = nss_phy_read_mmd(nss_phydev,
		NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_EGRESS_ERROR_COUNTER);

	return 0;
}

static enum nss_phy_cable_status
nss_phy_common_cdt_cable_status_get(u32 mdi_pair, u16 phy_status)
{
	enum nss_phy_cable_status status = CABLE_INVALID;

	switch (NSS_PHY_CDT_PAIR_STATUS(mdi_pair, phy_status)) {
	case 1:
		status = CABLE_NORMAL;
		break;
	case 2:
		status = CABLE_OPENED;
		break;
	case 3:
		status = CABLE_SHORT;
		break;
	}

	return status;
}

int nss_phy_common_cdt_start(struct nss_phy_device *nss_phydev)
{
	u16 status = 0, ii = 100;

	/* RUN CDT */
	nss_phy_write(nss_phydev, NSS_PHY_CDT_CONTROL,
		NSS_PHY_RUN_CDT | NSS_PHY_CABLE_LENGTH_UNIT);
	do {
		nss_phy_mdelay(30);
		status =
			nss_phy_read(nss_phydev, NSS_PHY_CDT_CONTROL);
	} while ((status & NSS_PHY_RUN_CDT) && (--ii));

	if (ii == 0)
		return -NSS_PHY_ETIMEOUT;

	return 0;
}

int nss_phy_common_cdt_status_get(struct nss_phy_device *nss_phydev,
	u32 mdi_pair, enum nss_phy_cable_status *cable_status, u32 *cable_len)
{
	u16 cable_delta_time = 0;
	u16 status = 0;

	if (mdi_pair >= NSS_PHY_MDI_PAIR_NUM)
		return -NSS_PHY_EINVAL;

	/* Get cable status */
	status = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CDT_STATUS);
	*cable_status = nss_phy_common_cdt_cable_status_get(mdi_pair,
		status);
	switch (mdi_pair) {
	case 0:
		cable_delta_time =
			nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_CDT_PAIR0);
		break;
	case 1:
		cable_delta_time =
			nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_CDT_PAIR1);
		break;
	case 2:
		cable_delta_time =
			nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_CDT_PAIR2);
		break;
	case 3:
		cable_delta_time =
			nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_CDT_PAIR3);
		break;
	}

	/* the actual cable length equals to CableDeltaTime * 0.824 */
	*cable_len = ((cable_delta_time & 0xff) * 824) / 1000;

	return 0;
}

int nss_phy_common_cdt(struct nss_phy_device *nss_phydev, u32 mdi_pair,
	enum nss_phy_cable_status *cable_status, u32 *cable_len)
{
	int ret;

	ret = nss_phy_common_cdt_start(nss_phydev);
	if (ret < 0)
		return ret;
	ret = nss_phy_common_cdt_status_get(nss_phydev, mdi_pair, cable_status,
		cable_len);

	return ret;
}

u32 nss_phy_common_reset_done(struct nss_phy_device *nss_phydev)
{
	u16 phy_data;
	u16 ii = 200;

	do {
		phy_data = nss_phy_read(nss_phydev, NSS_PHY_CONTROL);
			nss_phy_mdelay(10);
	} while ((phy_data & NSS_PHY_SOFT_RESET) && --ii);

	if (ii == 0)
		return NSS_PHY_FALSE;

	return !NSS_PHY_FALSE;
}

u16 nss_phy_common_intr_to_reg(struct nss_phy_device *nss_phydev,
	u32 mask)
{
	u16 phy_data = 0;

	if (INTR_WOL & mask)
		phy_data |= NSS_PHY_INTR_WOL;
	if (INTR_POE & mask)
		phy_data |= NSS_PHY_INTR_POE;
	if (INTR_BX_FX_LINK_UP & mask)
		phy_data |= NSS_PHY_INTR_BX_FX_LINK_UP;
	if (INTR_BX_FX_LINK_DOWN & mask)
		phy_data |= NSS_PHY_INTR_BX_FX_LINK_DOWN;
	if (INTR_LINK_UP & mask)
		phy_data |= NSS_PHY_INTR_LINK_UP;
	if (INTR_LINK_DOWN & mask)
		phy_data |= NSS_PHY_INTR_LINK_DOWN;
	if (INTR_MEDIA_TYPE & mask)
		phy_data |= NSS_PHY_INTR_MEDIA_TYPE;
	if (INTR_DUPLEX & mask)
		phy_data |= NSS_PHY_INTR_DUPLEX;
	if (INTR_SPEED & mask)
		phy_data |= NSS_PHY_INTR_SPEED;

	return phy_data;
}

u32 nss_phy_common_intr_from_reg(struct nss_phy_device *nss_phydev,
	u16 phy_data)
{
	u32 mask = 0;

	if (NSS_PHY_INTR_WOL & phy_data)
		mask |= INTR_WOL;
	if (NSS_PHY_INTR_POE & phy_data)
		mask |= INTR_POE;
	if (NSS_PHY_INTR_BX_FX_LINK_UP & phy_data)
		mask |= INTR_BX_FX_LINK_UP;
	if (NSS_PHY_INTR_BX_FX_LINK_DOWN & phy_data)
		mask |= INTR_BX_FX_LINK_DOWN;
	if (NSS_PHY_INTR_LINK_UP & phy_data)
		mask |= INTR_LINK_UP;
	if (NSS_PHY_INTR_LINK_DOWN & phy_data)
		mask |= INTR_LINK_DOWN;
	if (NSS_PHY_INTR_MEDIA_TYPE & phy_data)
		mask |= INTR_MEDIA_TYPE;
	if (NSS_PHY_INTR_DUPLEX & phy_data)
		mask |= INTR_DUPLEX;
	if (NSS_PHY_INTR_SPEED & phy_data)
		mask |= INTR_SPEED;

	return mask;
}

int nss_phy_common_intr_mask_set(struct nss_phy_device *nss_phydev,
	u32 intr_mask)
{
	u16 phy_data = 0;

	phy_data = nss_phy_common_intr_to_reg(nss_phydev, intr_mask);

	return nss_phy_write(nss_phydev, NSS_PHY_INTR_MASK,
		phy_data);
}

int nss_phy_common_intr_mask_get(struct nss_phy_device *nss_phydev,
	u32 *intr_mask)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_INTR_MASK);

	*intr_mask = nss_phy_common_intr_from_reg(nss_phydev, phy_data);

	return 0;
}

int nss_phy_common_intr_status_get(struct nss_phy_device *nss_phydev,
	u32 *intr_status)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_INTR_STATUS);

	*intr_status = nss_phy_common_intr_from_reg(nss_phydev, phy_data);

	return 0;
}

int nss_phy_common_hibernation_status_get(struct nss_phy_device *nss_phydev,
	u32 *status)
{
	u16 phy_data;

	*status = !NSS_PHY_FALSE;

	phy_data = nss_phy_read_debug(nss_phydev,
		NSS_PHY_DEBUG_PHY_HIBERNATION_STAT);

	if (phy_data & NSS_PHY_DEBUG_HIBERNATION_STAT_EN)
		*status = NSS_PHY_FALSE;

	return 0;
}

/* Reads AN failure count from NSS_PHY_DEBUG_CONTROL_REGISTER0 bits[7:4] on
 * each link state transition. The hardware resets this field to zero after
 * a successful link-up, so the latest snapshot overwrites the previous value.
 */
void nss_phy_common_an_fail_cnt_poll(struct nss_phy_device *nss_phydev,
	bool link_transition)
{
	int hw_cnt;

	if (!link_transition)
		return;

	hw_cnt = nss_phy_read_debug(nss_phydev, NSS_PHY_DEBUG_CONTROL_REGISTER0);
	if (hw_cnt < 0)
		return;

	atomic64_set(&nss_phydev->an_fail_count, (hw_cnt & NSS_PHY_DEBUG_AN_FAIL_CNT_MASK) >> 4);
}

int nss_phy_common_an_fail_counter_get(struct nss_phy_device *nss_phydev,
	u64 *count)
{
	if (!count)
		return -NSS_PHY_EINVAL;

	*count = atomic64_read(&nss_phydev->an_fail_count);

	return 0;
}

void nss_phy_common_an_fail_counter_reset(struct nss_phy_device *nss_phydev)
{
	atomic64_set(&nss_phydev->an_fail_count, 0);
}

/*
 * log2(1 + k/16) * 1000 for k = 0..15, used by nss_phy_mse_to_snr_centi_db().
 */
static const u16 nss_phy_log2_frac[16] = {
	0, 87, 170, 248, 322, 392, 459, 524,
	585, 644, 700, 755, 807, 858, 907, 954
};

/*
 * Returns 10 * log10(num / den) in centidB (0.01 dB units).
 * Uses integer arithmetic with approximately 0.2 dB accuracy; the
 * approximation is systematically conservative (underestimates SNR).
 * Returns 0 when den or num is 0.
 */
s32 nss_phy_mse_to_snr_centi_db(u32 num, u32 den)
{
	u64 r;
	int n;
	u32 idx;
	s32 log2_milli;

	if (!den || !num)
		return 0;

	/* Scale by 2^20 to preserve precision across the range of mse values.
	 * Use do_div() to avoid __udivdi3 on 32-bit platforms.
	 */
	r = (u64)num << 20;
	do_div(r, den);
	if (!r)
		return 0;

	n = (int)(fls64(r) - 1);
	idx = (n >= 4) ? (u32)((r >> (n - 4)) & 0xf) : (u32)((r << (4 - n)) & 0xf);

	/* log2(num/den) * 1000 millibits: integer + fractional parts */
	log2_milli = (n - 20) * 1000 + nss_phy_log2_frac[idx];

	/* centidB = log2_milli * 1000/log2(10)/1000 * 100
	 *         = log2_milli * 30103 / 100000
	 *         ≈ log2_milli * 301 / 1000
	 */
	return log2_milli * 301 / 1000;
}

int nss_phy_common_mse_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_mse *mse)
{
	static const struct {
		u32 great;
		u32 good;
		u32 normal;
		u32 crc;
	} thresh_100m = { 15, 30, 150, 349 },
	  thresh_1g   = { 19, 38, 94,  187 };
	const typeof(thresh_100m) *thresh;
	static const u16 ch_regs[4] = {
		NSS_PHY_DEBUG_MSE_100M_1G_CH0, NSS_PHY_DEBUG_MSE_100M_1G_CH1,
		NSS_PHY_DEBUG_MSE_100M_1G_CH2, NSS_PHY_DEBUG_MSE_100M_1G_CH3,
	};
	static const u16 ch_masks[4] = {
		NSS_PHY_DEBUG_MSE_100M_1G_CH0_MASK, NSS_PHY_DEBUG_MSE_100M_1G_CH1_MASK,
		NSS_PHY_DEBUG_MSE_100M_1G_CH2_MASK, NSS_PHY_DEBUG_MSE_100M_1G_CH3_MASK,
	};
	u32 speed;
	int ret, i;

	if (!mse)
		return -NSS_PHY_EINVAL;

	memset(mse, 0, sizeof(*mse));

	speed = nss_phydev_speed_get(nss_phydev);
	if (speed != NSS_PHY_SPEED_100 && speed != NSS_PHY_SPEED_1000)
		return -NSS_PHY_EOPNOTSUPP;

	mutex_lock(&nss_phydev->mse_lock);

	ret = nss_phy_modify_debug(nss_phydev, NSS_PHY_DEBUG_CONTROL_REGISTER0,
		NSS_PHY_DEBUG_MSE_100M_1G_EN, NSS_PHY_DEBUG_MSE_100M_1G_EN);
	if (ret < 0) {
		mutex_unlock(&nss_phydev->mse_lock);
		return ret;
	}

	for (i = 0; i < 4; i++) {
		ret = nss_phy_read_debug(nss_phydev, ch_regs[i]);
		if (ret < 0) {
			mse->quality[i] = NSS_PHY_MSE_QUALITY_NA;
			continue;
		}
		mse->mse[i] = ret & ch_masks[i];
		mse->quality[i] = NSS_PHY_MSE_QUALITY_LINK_DOWN; /* threshold loop will classify */
	}

	nss_phy_modify_debug(nss_phydev, NSS_PHY_DEBUG_CONTROL_REGISTER0,
		NSS_PHY_DEBUG_MSE_100M_1G_EN, 0);

	mutex_unlock(&nss_phydev->mse_lock);

	thresh = (speed == NSS_PHY_SPEED_100) ? &thresh_100m : &thresh_1g;

	for (i = 0; i < 4; i++) {
		u32 v = mse->mse[i];
		s32 thresh_centi_db = (speed == NSS_PHY_SPEED_100) ? -2300 : -2500;

		if (mse->quality[i] == NSS_PHY_MSE_QUALITY_NA)
			continue;

		if (v <= thresh->great)
			mse->quality[i] = NSS_PHY_MSE_QUALITY_GREAT;
		else if (v <= thresh->good)
			mse->quality[i] = NSS_PHY_MSE_QUALITY_GOOD;
		else if (v <= thresh->normal)
			mse->quality[i] = NSS_PHY_MSE_QUALITY_NORMAL;
		else if (v <= thresh->crc)
			mse->quality[i] = NSS_PHY_MSE_QUALITY_CRC;
		else
			mse->quality[i] = NSS_PHY_MSE_QUALITY_LINK_DOWN;

		mse->snr_margin[i] = v ? nss_phy_mse_to_snr_centi_db(29687, v) + thresh_centi_db : 0;
	}

	return 0;
}

int nss_phy_common_ms_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_ms_mode mode)
{
	u16 val;
	int ret;

	switch (mode) {
	case NSS_PHY_MS_AUTO:
	case NSS_PHY_MS_PREFER_MASTER:
		/* MII reg 9 has no "prefer master" bit; PREFER_MASTER maps to AUTO. */
		val = 0;
		break;
	case NSS_PHY_MS_FORCE_MASTER:
		val = NSS_PHY_MII_MS_MANUAL_EN | NSS_PHY_MII_MS_MASTER_VAL;
		break;
	case NSS_PHY_MS_FORCE_SLAVE:
		val = NSS_PHY_MII_MS_MANUAL_EN;
		break;
	default:
		return -NSS_PHY_EINVAL;
	}

	ret = nss_phy_modify(nss_phydev, NSS_PHY_MII_CTRL1000,
		NSS_PHY_MII_MS_CTRL_MASK, val);
	if (ret < 0)
		return ret;

	return nss_phy_common_autoneg_restart(nss_phydev);
}

int nss_phy_common_ms_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_ms_mode *mode)
{
	int ret = nss_phy_read(nss_phydev, NSS_PHY_MII_CTRL1000);
	u16 val;

	if (ret < 0)
		return ret;

	val = (u16)ret & NSS_PHY_MII_MS_CTRL_MASK;

	if (!(val & NSS_PHY_MII_MS_MANUAL_EN))
		*mode = NSS_PHY_MS_AUTO;
	else if (val & NSS_PHY_MII_MS_MASTER_VAL)
		*mode = NSS_PHY_MS_FORCE_MASTER;
	else
		*mode = NSS_PHY_MS_FORCE_SLAVE;

	return 0;
}

/* Returns 1 if PHY resolved as master, 0 if slave, <0 on error. */
int nss_phy_common_ms_status_get(struct nss_phy_device *nss_phydev)
{
	int ret = nss_phy_read(nss_phydev, NSS_PHY_MII_STAT1000);

	if (ret < 0)
		return ret;

	return ((u16)ret & NSS_PHY_MII_MS_RESOLVED_MASTER) ? 1 : 0;
}

void nss_phy_common_flap_stats_update(struct nss_phy_device *nss_phydev,
	bool link_up_transition, bool link_down_transition)
{
	struct nss_phy_link_flap_stats *stats = &nss_phydev->flap_stats;
	time64_t now = ktime_get_seconds();

	if (link_up_transition) {
		atomic64_inc(&stats->up_count);
		atomic64_set(&stats->last_up_time, now);
	} else if (link_down_transition) {
		atomic64_inc(&stats->down_count);
		atomic64_set(&stats->last_down_time, now);
	}

	atomic64_set(&stats->last_change_time, now);
}

int nss_phy_common_flap_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_link_flap_stats *out)
{
	if (!out)
		return -NSS_PHY_EINVAL;

	atomic64_set(&out->up_count,          atomic64_read(&nss_phydev->flap_stats.up_count));
	atomic64_set(&out->down_count,        atomic64_read(&nss_phydev->flap_stats.down_count));
	atomic64_set(&out->last_up_time,      atomic64_read(&nss_phydev->flap_stats.last_up_time));
	atomic64_set(&out->last_down_time,    atomic64_read(&nss_phydev->flap_stats.last_down_time));
	atomic64_set(&out->last_change_time,  atomic64_read(&nss_phydev->flap_stats.last_change_time));

	return 0;
}

void nss_phy_common_flap_stats_reset(struct nss_phy_device *nss_phydev)
{
	struct nss_phy_link_flap_stats *stats = &nss_phydev->flap_stats;

	atomic64_set(&stats->up_count, 0);
	atomic64_set(&stats->down_count, 0);
	atomic64_set(&stats->last_up_time, 0);
	atomic64_set(&stats->last_down_time, 0);
	atomic64_set(&stats->last_change_time, 0);
}

int nss_phy_common_cld_enable(struct nss_phy_device *nss_phydev)
{
	int ret;

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CLD_CTRL16, NSS_PHY_MMD3_CLD_CTRL16_VAL);
	if (ret < 0)
		return ret;

	return nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_CLD_CTRL, NSS_PHY_MMD7_CLD_CTRL_VAL);
}

int nss_phy_common_cld_enable_get(struct nss_phy_device *nss_phydev,
	bool *enabled)
{
	int ret;

	ret = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_CLD_CTRL);
	if (ret < 0)
		return ret;

	*enabled = !!((u16)ret & NSS_PHY_MMD7_CLD_CTRL_VAL);
	return 0;
}

int nss_phy_common_cld_cable_len_read(struct nss_phy_device *nss_phydev,
	u16 cld_len_reg, u16 cld_len_mask, u32 *cable_len)
{
	bool cld_enabled;
	int ret, restore_ret;

	ret = nss_phy_common_cld_enable_get(nss_phydev, &cld_enabled);
	if (ret < 0)
		return ret;

	if (!cld_enabled) {
		ret = nss_phy_common_cld_enable(nss_phydev);
		if (ret < 0)
			return ret;
	}

	ret = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM, cld_len_reg);
	if (ret >= 0)
		*cable_len = (u32)ret & cld_len_mask;

	if (!cld_enabled) {
		restore_ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_CLD_CTRL, NSS_PHY_MMD7_CLD_CTRL_VAL, 0);
		if (restore_ret < 0 && ret >= 0)
			return restore_ret;
	}

	return (ret < 0) ? ret : 0;
}

