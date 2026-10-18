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
#include "nss_phy_c45_common.h"

int nss_phy_c45_common_eee_adv_set(struct nss_phy_device *nss_phydev,
	u32 adv)
{
	u16 phy_data = 0;
	int ret;

	if (adv & EEE_100BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_100M;
	if (adv & EEE_1000BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_1000M;
	if (adv & EEE_10000BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_10000M;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_CTRL, NSS_PHY_MMD7_EEE_MASK,
		phy_data);
	if (ret < 0)
		return ret;

	phy_data = 0;
	if (adv & EEE_2500BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_2500M;
	if (adv & EEE_5000BASE_T)
		phy_data |= NSS_PHY_MMD7_EEE_ADV_5000M;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_CTRL1, NSS_PHY_MMD7_EEE_MASK1,
		phy_data);

	nss_phydev_eee_update(nss_phydev, adv);

	return nss_phy_c45_common_autoneg_restart(nss_phydev);
}

int nss_phy_c45_common_eee_adv_get(struct nss_phy_device *nss_phydev,
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
	if (phy_data & NSS_PHY_MMD7_EEE_ADV_10000M)
		*adv |= EEE_10000BASE_T;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_CTRL1);
	if (phy_data & NSS_PHY_MMD7_EEE_ADV_2500M)
		*adv |= EEE_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_ADV_5000M)
		*adv |= EEE_5000BASE_T;

	return 0;
}

int nss_phy_c45_common_eee_partner_adv_get(struct nss_phy_device *nss_phydev,
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
	if (phy_data & NSS_PHY_MMD7_EEE_PARTNER_ADV_10000M)
		*adv |= EEE_10000BASE_T;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_8023AZ_EEE_PARTNER1);
	if (phy_data & NSS_PHY_MMD7_EEE_PARTNER_ADV_2500M)
		*adv |= EEE_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_PARTNER_ADV_5000M)
		*adv |= EEE_5000BASE_T;

	return 0;
}

int nss_phy_c45_common_eee_cap_get(struct nss_phy_device *nss_phydev,
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
	if (phy_data & NSS_PHY_MMD3_EEE_CAPABILITY_10000M)
		*cap |= EEE_10000BASE_T;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_8023AZ_EEE_CAPABILITY1);
	if (phy_data & NSS_PHY_MMD3_EEE_CAPABILITY_2500M)
		*cap |= EEE_2500BASE_T;
	if (phy_data & NSS_PHY_MMD3_EEE_CAPABILITY_5000M)
		*cap |= EEE_5000BASE_T;

	return 0;
}

int nss_phy_c45_common_eee_status_get(struct nss_phy_device *nss_phydev,
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
	if (phy_data & NSS_PHY_MMD7_EEE_STATUS_2500M)
		*status |= EEE_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_STATUS_5000M)
		*status |= EEE_5000BASE_T;
	if (phy_data & NSS_PHY_MMD7_EEE_STATUS_10000M)
		*status |= EEE_10000BASE_T;

	return 0;
}

int nss_phy_c45_common_8023az_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u32 eee_adv = 0;

	if (enable)
		eee_adv = ALL_SPEED_EEE;

	return nss_phy_c45_common_eee_adv_set(nss_phydev, eee_adv);
}

int nss_phy_c45_common_8023az_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u32 eee_adv = 0, eee_cap = 0;
	int ret = 0;

	ret = nss_phy_c45_common_eee_adv_get(nss_phydev, &eee_adv);
	if (ret < 0)
		return ret;
	ret = nss_phy_c45_common_eee_cap_get(nss_phydev, &eee_cap);
	if (ret < 0)
		return ret;

	if (eee_adv == eee_cap)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_c45_common_autoneg_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;
	int ret = 0;

	if (enable)
		phy_data |= NSS_PHY_AUTONEG_EN;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_AN_CONTROL, NSS_PHY_AUTONEG_EN, phy_data);
	if (ret < 0)
		return ret;

	return nss_phydev_autoneg_update(nss_phydev, enable);
}

int nss_phy_c45_common_force_speed_set(struct nss_phy_device *nss_phydev)
{
	u16 phy_speed_ctrl = 0, phy_speed_type = 0;
	int ret = 0;

	switch (nss_phydev_speed_get(nss_phydev)) {
	case NSS_PHY_SPEED_10:
		phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_10M;
		phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_10M;
		break;
	case NSS_PHY_SPEED_100:
		phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_100M;
		phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_100M;
		break;
	case NSS_PHY_SPEED_1000:
		phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_1000M;
		phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_1000M;
		break;
	case NSS_PHY_SPEED_2500:
		phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_2500M;
		phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_2500M;
		break;
	case NSS_PHY_SPEED_5000:
		phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_5000M;
		phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_5000M;
		break;
	case NSS_PHY_SPEED_10000:
		phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_10000M;
		phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_10000M;
		break;
	default:
		if (nss_phy_support_10m(nss_phydev)) {
			phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_10M;
			phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_10M;
		} else {
			phy_speed_ctrl = NSS_PHY_MMD1_PMA_CONTROL_100M;
			phy_speed_type = NSS_PHY_MMD1_PMA_TYPE_100M;
		}
	}
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_PMA_CONTROL, NSS_PHY_MMD1_PMA_SPEED_MASK,
		phy_speed_ctrl);
	if (ret < 0)
		return ret;

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_PMA_TTYPE, NSS_PHY_MMD1_PMA_TYPE_MASK,
		phy_speed_type);
}

int nss_phy_c45_common_pma_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0;
	u32 autoneg = 0;
	int ret = 0;

	if (enable) {
		ret = nss_phy_c45_common_force_speed_set(nss_phydev);
		if (ret < 0)
			return ret;
		phy_data |= NSS_PHY_LOCAL_LOOPBACK_EN;
	} else {
		autoneg = !NSS_PHY_FALSE;
	}

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_CONTROL, NSS_PHY_LOCAL_LOOPBACK_EN, phy_data);
	if (ret < 0)
		return ret;

	return nss_phy_c45_common_autoneg_set(nss_phydev, autoneg);
}

int nss_phy_c45_common_pma_local_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_CONTROL);

	if (phy_data & NSS_PHY_LOCAL_LOOPBACK_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_c45_common_pcs_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	int ret;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM, NSS_PHY_CONTROL,
		NSS_PHY_LOCAL_LOOPBACK_EN, enable ? NSS_PHY_LOCAL_LOOPBACK_EN : 0);
	if (ret < 0)
		return ret;
	ret = nss_phy_c45_common_autoneg_set(nss_phydev, !enable);
	if (ret < 0)
		return ret;
	if (enable) {
		ret = nss_phy_c45_common_force_speed_set(nss_phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int nss_phy_c45_common_pcs_local_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_CONTROL);

	if (phy_data & NSS_PHY_LOCAL_LOOPBACK_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_c45_common_fifo_reset(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	int phy_data = 0;

	if (!enable)
		phy_data |= NSS_PHY_FIFO_RESET_MASK;

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_FIFO_CONTROL,
		NSS_PHY_FIFO_RESET_MASK,
		phy_data);
}

int nss_phy_c45_common_autoneg_restart(struct nss_phy_device *nss_phydev)
{
	int ret = 0;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_AN_CONTROL,
		NSS_PHY_AUTONEG_RESTART | NSS_PHY_AUTONEG_EN,
		NSS_PHY_AUTONEG_RESTART | NSS_PHY_AUTONEG_EN);
	if (ret < 0)
		return ret;

	return nss_phydev_autoneg_update(nss_phydev, !NSS_PHY_FALSE);
}

int nss_phy_c45_common_cdt_start(struct nss_phy_device *nss_phydev)
{
	u16 status = 0;
	u16 ii = 100;
	int ret = 0;

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_CDT_CONTROL, NSS_PHY_RUN_CDT |
		NSS_PHY_CABLE_LENGTH_UNIT);
	if (ret < 0)
		return ret;

	do {
		nss_phy_mdelay(30);
		status = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
			NSS_PHY_CDT_CONTROL);
	} while ((status & NSS_PHY_RUN_CDT) && (--ii));

	if (ii == 0)
		return -NSS_PHY_ETIMEOUT;

	return 0;
}

int nss_phy_c45_common_mdix_mode_set(struct nss_phy_device *nss_phydev,
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
		return -NSS_PHY_EINVAL;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_SPEC_CONTROL, NSS_PHY_MDIX_AUTO, phy_data);

	return ret;
}

int nss_phy_c45_common_mdix_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_mode *mode)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_SPEC_CONTROL);

	if ((phy_data & NSS_PHY_MDIX_AUTO) == NSS_PHY_MDIX_AUTO)
		*mode = MODE_AUTO;
	else if ((phy_data & NSS_PHY_MDIX_AUTO) == NSS_PHY_MDIX)
		*mode = MODE_MDIX;
	else
		*mode = MODE_MDI;

	return 0;
}

int nss_phy_c45_common_mdix_status_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_status *mode)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_SPEC_STATUS);

	*mode = (phy_data & NSS_PHY_MDIX_STATUS) ? STATUS_MDIX :
		STATUS_MDI;

	return 0;
}

int nss_phy_c45_common_stats_status_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	int ret = 0;
	u16 phy_data = 0;

	if (enable)
		phy_data |= NSS_PHY_MMD3_10G_FRAME_CHECK_EN;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_FRAME_CHECK_CTRL,
		NSS_PHY_MMD3_10G_FRAME_CHECK_EN,
		phy_data);

	return ret;
}

int nss_phy_c45_common_stats_status_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u16 phy_data  = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_FRAME_CHECK_CTRL);
	if (phy_data & NSS_PHY_MMD3_10G_FRAME_CHECK_EN)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

int nss_phy_c45_common_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_stats_info *stats_info)
{
	u64 cnt_h = 0, cnt_m = 0, cnt_l  = 0;

	cnt_h = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_INGRESS_COUNTER_HIGH);
	cnt_m = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_INGRESS_COUNTER_MIDDLE);
	cnt_l = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_INGRESS_COUNTER_LOW);
	stats_info->RxGoodFrame = (cnt_h << 32) | (cnt_m << 16) |
		cnt_l;
	stats_info->RxFcsErr = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_INGRESS_ERROR_COUNTER);

	cnt_h = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_EGRESS_COUNTER_HIGH);
	cnt_m = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_EGRESS_COUNTER_MIDDLE);
	cnt_l = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_EGRESS_COUNTER_LOW);
	stats_info->TxGoodFrame = (cnt_h << 32) | (cnt_m << 16) |
		cnt_l;
	stats_info->TxFcsErr = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_10G_EGRESS_ERROR_COUNTER);

	return 0;
}

u16 nss_phy_c45_common_intr_to_reg(struct nss_phy_device *nss_phydev,
	u32 mask)
{
	u16 phy_data = 0;

	if (INTR_WOL & mask)
		phy_data |= NSS_PHY_INTR_WOL;
	if (INTR_POE & mask)
		phy_data |= NSS_PHY_INTR_POE;
	if (INTR_TX_PTP & mask)
		phy_data |= NSS_PHY_MMD31_INTR_TX_PTP;
	if (INTR_RX_PTP & mask)
		phy_data |= NSS_PHY_MMD31_INTR_RX_PTP;
	if (INTR_10MS_PTP & mask)
		phy_data |= NSS_PHY_MMD31_INTR_10MS_PTP;
	if (INTR_DOWNSHIF & mask)
		phy_data |= NSS_PHY_MMD31_INTR_DOWNSHIF;
	if (INTR_FAST_LINK_DOWN_100M & mask)
		phy_data |= NSS_PHY_MMD31_INTR_FAST_LINK_DOWN_100M;
	if (INTR_FAST_LINK_DOWN_1000M & mask)
		phy_data |= NSS_PHY_MMD31_INTR_FAST_LINK_DOWN_1000M;
	if (INTR_LINK_UP & mask)
		phy_data |= NSS_PHY_INTR_LINK_UP;
	if (INTR_LINK_DOWN & mask)
		phy_data |= NSS_PHY_INTR_LINK_DOWN;
	if (INTR_SEC_ENA & mask)
		phy_data |= NSS_PHY_MMD31_INTR_SEC_ENA;
	if (INTR_SPEED & mask)
		phy_data |= NSS_PHY_INTR_SPEED;
	if (INTR_FAST_LINK_DOWN & mask)
		phy_data |= NSS_PHY_MMD31_INTR_FAST_LINK_DOWN;

	return phy_data;
}

u32 nss_phy_c45_common_intr_from_reg(struct nss_phy_device *nss_phydev,
	u16 phy_data)
{
	u32 mask = 0;

	if (NSS_PHY_INTR_WOL & phy_data)
		mask |= INTR_WOL;
	if (NSS_PHY_INTR_POE & phy_data)
		mask |= INTR_POE;
	if (NSS_PHY_MMD31_INTR_TX_PTP & phy_data)
		mask |= INTR_TX_PTP;
	if (NSS_PHY_MMD31_INTR_RX_PTP & phy_data)
		mask |= INTR_RX_PTP;
	if (NSS_PHY_MMD31_INTR_10MS_PTP & phy_data)
		mask |= INTR_10MS_PTP;
	if (NSS_PHY_MMD31_INTR_DOWNSHIF & phy_data)
		mask |= INTR_DOWNSHIF;
	if (NSS_PHY_MMD31_INTR_FAST_LINK_DOWN_100M & phy_data)
		mask |= INTR_FAST_LINK_DOWN_100M;
	if (NSS_PHY_MMD31_INTR_FAST_LINK_DOWN_1000M & phy_data)
		mask |= INTR_FAST_LINK_DOWN_1000M;
	if (NSS_PHY_INTR_LINK_UP & phy_data)
		mask |= INTR_LINK_UP;
	if (NSS_PHY_INTR_LINK_DOWN & phy_data)
		mask |= INTR_LINK_DOWN;
	if (NSS_PHY_INTR_MEDIA_TYPE & phy_data)
		mask |= INTR_MEDIA_TYPE;
	if (NSS_PHY_INTR_SPEED & phy_data)
		mask |= INTR_SPEED;
	if (NSS_PHY_MMD31_INTR_FAST_LINK_DOWN & phy_data)
		mask |= INTR_FAST_LINK_DOWN;

	return mask;
}

int nss_phy_c45_common_intr_mask_set(struct nss_phy_device *nss_phydev,
	u32 intr_mask)
{
	u16 phy_data = 0;

	phy_data = nss_phy_c45_common_intr_to_reg(nss_phydev, intr_mask);

	return nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_INTR_MASK, phy_data);
}

int nss_phy_c45_common_intr_mask_get(struct nss_phy_device *nss_phydev,
	u32 *intr_mask)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_INTR_MASK);

	*intr_mask = nss_phy_common_intr_from_reg(nss_phydev, phy_data);

	return 0;
}

int nss_phy_c45_common_intr_status_get(struct nss_phy_device *nss_phydev,
	u32 *intr_status)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_INTR_STATUS);

	*intr_status = nss_phy_common_intr_from_reg(nss_phydev, phy_data);

	return 0;
}

static u32 nss_phy_c45_led_force_reg_get
	(struct nss_phy_device *nss_phydev, u32 source_id)
{
	u16 led_ctrl[3] = {NSS_PHY_MMD7_LED0_FORCE_CTRL,
		NSS_PHY_MMD7_LED1_FORCE_CTRL,
		NSS_PHY_MMD7_LED2_FORCE_CTRL
	};

	if (source_id > NSS_PHY_LED_SOURCE2) {
		nss_phy_err(nss_phydev, "source %d is not support\n",
			source_id);
		return NSS_PHY_INVALID_REG;
	}

	return led_ctrl[source_id];
}

int nss_phy_c45_common_led_force_set(struct nss_phy_device *nss_phydev,
	u32 source_id, u32 enable, u32 force_mode)
{
	int ret = 0;
	u32 mmd_reg = 0;
	u16 phy_data = 0;

	mmd_reg = nss_phy_c45_led_force_reg_get(nss_phydev,
		source_id);
	if (mmd_reg == NSS_PHY_INVALID_REG)
		return -NSS_PHY_EOPNOTSUPP;

	if (enable) {
		ret = nss_phy_common_led_force_to_phy(nss_phydev,
			force_mode, &phy_data);
		if (ret < 0)
			return ret;
	}
	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		mmd_reg, NSS_PHY_MMD7_LED_FORCE_EN |
		NSS_PHY_MMD7_LED_FORCE_MASK,
		phy_data);
}

int nss_phy_c45_common_led_force_get(struct nss_phy_device *nss_phydev,
	u32 source_id, u32 *enable, u32 *force_mode)
{
	int ret = 0;
	u32 mmd_reg = 0;
	u16 phy_data = 0;

	mmd_reg = nss_phy_c45_led_force_reg_get(nss_phydev,
		source_id);
	if (mmd_reg == NSS_PHY_INVALID_REG)
		return -NSS_PHY_EOPNOTSUPP;

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

	return ret;
}

static u32 nss_phy_2500m_led_reg_get(struct nss_phy_device *nss_phydev,
	u32 source_id)
{
	u16 led_ctrl[3] = {NSS_PHY_MMD7_LED0_CTRL,
		NSS_PHY_MMD7_LED1_CTRL,
		NSS_PHY_MMD7_LED2_CTRL
	};

	if (source_id > NSS_PHY_LED_SOURCE2) {
		nss_phy_err(nss_phydev, "source %d is not support\n",
			source_id);
		return NSS_PHY_INVALID_REG;
	}

	return led_ctrl[source_id];
}

static int nss_phy_2500m_led_from_phy(struct nss_phy_device *nss_phydev,
	u32 *status_bmap, u16 phy_data)
{
	if (nss_phy_support_2500(nss_phydev)) {
		if (phy_data & NSS_PHY_MMD7_LINK_2500M_LIGHT_EN)
			*status_bmap |= NSS_BIT(LED_LINK_2500M_LIGHT_EN);
	}

	return nss_phy_common_led_from_phy(nss_phydev, status_bmap, phy_data);
}

static int nss_phy_2500m_led_to_phy(struct nss_phy_device *nss_phydev,
	u32 status_bmap, u16 *phy_data)
{
	if (nss_phy_support_2500(nss_phydev)) {
		if (status_bmap & NSS_BIT(LED_LINK_2500M_LIGHT_EN))
			*phy_data |=  NSS_PHY_MMD7_LINK_2500M_LIGHT_EN;
	}

	return nss_phy_common_led_to_phy(nss_phydev, status_bmap, phy_data);
}

int nss_phy_2500m_led_ctrl_source_set(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	int ret = 0;
	u32 mmd_reg = 0;
	u16 phy_data = 0;

	if (source_id > NSS_PHY_LED_SOURCE2)
		return -NSS_PHY_EOPNOTSUPP;

	ret = nss_phy_common_led_active_set(nss_phydev,
		pattern->active_level);
	if (ret < 0)
		return ret;
	ret = nss_phy_common_led_blink_freq_set(nss_phydev, pattern->mode,
		pattern->freq);
	if (ret < 0)
		return ret;
	if (pattern->mode == ACT_PHY_STATUS) {
		ret = nss_phy_c45_common_led_force_set(nss_phydev,
			source_id, NSS_PHY_FALSE, pattern->mode);
		if (ret < 0)
			return ret;
		ret = nss_phy_2500m_led_to_phy(nss_phydev,
			pattern->phy_status_bmap, &phy_data);
		if (ret < 0)
			return ret;
		mmd_reg = nss_phy_2500m_led_reg_get(nss_phydev,
			source_id);
		if (mmd_reg == NSS_PHY_INVALID_REG)
			return -NSS_PHY_EOPNOTSUPP;
		ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD7_NUM, mmd_reg,
			phy_data);
		if (ret < 0)
			return ret;
	} else {
		ret = nss_phy_c45_common_led_force_set(nss_phydev, source_id,
			!NSS_PHY_FALSE, pattern->mode);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int nss_phy_2500m_led_ctrl_source_get(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	int ret = 0;
	u32 mmd_reg = 0, force_enable = NSS_PHY_FALSE;
	u16 phy_data = 0;

	if (source_id > NSS_PHY_LED_SOURCE2)
		return -NSS_PHY_EOPNOTSUPP;

	ret = nss_phy_common_led_active_get(nss_phydev,
		&(pattern->active_level));
	if (ret < 0)
		return ret;
	pattern->phy_status_bmap = 0;
	ret = nss_phy_c45_common_led_force_get(nss_phydev, source_id,
		&force_enable, &(pattern->mode));
	if (ret < 0)
		return ret;
	if (!force_enable) {
		pattern->mode = ACT_PHY_STATUS;
		mmd_reg = nss_phy_2500m_led_reg_get(nss_phydev,
			source_id);
		if (mmd_reg == NSS_PHY_INVALID_REG)
			return -NSS_PHY_EOPNOTSUPP;
		phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			mmd_reg);
		ret = nss_phy_2500m_led_from_phy(nss_phydev,
			&(pattern->phy_status_bmap), phy_data);
		if (ret < 0)
			return ret;
	}
	ret = nss_phy_common_led_blink_freq_get(nss_phydev, pattern->mode,
		&(pattern->freq));

	return ret;
}

static int nss_phy_10g_led_ctrl_set(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	u16 phy_data = 0, mask = 0;

	mask = NSS_BIT(NSS_PHY_MMD7_10G_SRC0_OFFSET + source_id * 2) |
		NSS_BIT(NSS_PHY_MMD7_5G_SRC0_OFFSET + source_id * 2);

	if (nss_phy_support_10g(nss_phydev)) {
		if (pattern->phy_status_bmap &
				NSS_BIT(LED_LINK_10000M_LIGHT_EN))
			phy_data |= NSS_BIT(NSS_PHY_MMD7_10G_SRC0_OFFSET +
				source_id * 2);
	}
	if (pattern->phy_status_bmap &
		NSS_BIT(LED_LINK_5000M_LIGHT_EN))
		phy_data |= NSS_BIT(NSS_PHY_MMD7_5G_SRC0_OFFSET +
			source_id * 2);

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LED_10G_CTRL, mask, phy_data);
}

static int nss_phy_10g_led_ctrl_get(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LED_10G_CTRL);
	if (nss_phy_support_10g(nss_phydev)) {
		if (phy_data & NSS_BIT(NSS_PHY_MMD7_10G_SRC0_OFFSET +
			source_id * 2))
			pattern->phy_status_bmap
				|= NSS_BIT(LED_LINK_10000M_LIGHT_EN);
	}
	if (phy_data & NSS_BIT(NSS_PHY_MMD7_5G_SRC0_OFFSET +
		source_id * 2))
		pattern->phy_status_bmap
			|= NSS_BIT(LED_LINK_5000M_LIGHT_EN);

	return 0;
}

int nss_phy_c45_common_led_ctrl_source_set(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	int ret;

	ret = nss_phy_2500m_led_ctrl_source_set(nss_phydev, source_id,
		pattern);
	if (ret < 0)
		return ret;
	if (pattern->mode == ACT_PHY_STATUS) {
		ret = nss_phy_10g_led_ctrl_set(nss_phydev, source_id, pattern);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int nss_phy_c45_common_led_ctrl_source_get(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	int ret = 0;

	ret = nss_phy_2500m_led_ctrl_source_get(nss_phydev, source_id,
		pattern);
	if (ret < 0)
		return ret;

	if (pattern->mode == ACT_PHY_STATUS) {
		ret = nss_phy_10g_led_ctrl_get(nss_phydev, source_id, pattern);
		if (ret < 0)
			return ret;
	}

	return 0;
}
