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
