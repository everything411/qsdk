/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/math64.h>
#include "nss_phy.h"
#include "nss_phy_c45_common.h"

int nss_phy_c45_common_eee_adv_set(struct nss_phy_device *nss_phydev,
	u32 adv)
{
	u16 phy_data = 0;
	int ret;

	if (!nss_phydev_eee_support(nss_phydev))
		return -NSS_PHY_EOPNOTSUPP;

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
	if (nss_phydev_speed_get(nss_phydev) == NSS_PHY_SPEED_100) {
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_TX_CTRL,
			NSS_PHY_MMD7_TX_ZERO, enable ? NSS_PHY_MMD7_TX_ZERO : 0);
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

int nss_phy_c45_common_soft_reset(struct nss_phy_device *nss_phydev)
{
	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_CONTROL, NSS_PHY_SOFT_RESET, NSS_PHY_SOFT_RESET);
}

int nss_phy_c45_common_mdix_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_mode mode)
{
	int ret = 0;

	ret = nss_phy_c45_common_mdix_mode_set(nss_phydev, mode);
	if (ret < 0)
		return ret;

	return nss_phy_c45_common_soft_reset(nss_phydev);
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

	*intr_mask = nss_phy_c45_common_intr_from_reg(nss_phydev, phy_data);

	return 0;
}

int nss_phy_c45_common_intr_status_get(struct nss_phy_device *nss_phydev,
	u32 *intr_status)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_INTR_STATUS);

	*intr_status = nss_phy_c45_common_intr_from_reg(nss_phydev, phy_data);

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

int nss_phy_c45_function_reset(struct nss_phy_device *nss_phydev,
	enum nss_phy_reset reset_type)
{
	int ret = 0;

	switch (reset_type) {
	case FIFO_RESET:
		ret = nss_phy_c45_common_fifo_reset(nss_phydev, true);
		if (ret < 0)
			return ret;
		nss_phy_mdelay(50);
		ret = nss_phy_c45_common_fifo_reset(nss_phydev, false);
		if (ret < 0)
			return ret;
		break;
	case SOFT_RESET:
		ret = nss_phy_c45_common_soft_reset(nss_phydev);
		if (ret < 0)
			return ret;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}

	return 0;
}

static int nss_phy_c45_common_fr_bypass_set(struct nss_phy_device *nss_phydev,
	u32 bypass_enable)
{
	int ret = 0;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_BYPASS_25, NSS_PHY_MMD7_FR_BYPASS_2500M,
		bypass_enable ? NSS_PHY_MMD7_FR_BYPASS_2500M : 0);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_BYPASS_10_5,
		NSS_PHY_MMD7_FR_BYPASS_10G | NSS_PHY_MMD7_FR_BYPASS_5000M,
		bypass_enable ? (NSS_PHY_MMD7_FR_BYPASS_10G |
		NSS_PHY_MMD7_FR_BYPASS_5000M) : 0);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_FR_ENABLE_BYPASS_REG,
		NSS_PHY_MMD3_FR_ENABLE_BYPASS,
		bypass_enable ? NSS_PHY_MMD3_FR_ENABLE_BYPASS : 0);
	if (ret < 0)
		return ret;

	return 0;
}

static int nss_phy_c45_common_fr_ieee_adv_get(struct nss_phy_device *nss_phydev,
	u32 *adv_bitmap)
{
	int phy_data = 0;

	*adv_bitmap = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_ADV);
	if (phy_data < 0)
		return phy_data;
	if (phy_data & NSS_PHY_MMD7_FR_ADV_2500M)
		*adv_bitmap |= NSS_PHY_FR_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_FR_ADV_5000M)
		*adv_bitmap |= NSS_PHY_FR_5000BASE_T;
	if (phy_data & NSS_PHY_MMD7_FR_ADV_10G)
		*adv_bitmap |= NSS_PHY_FR_10000BASE_T;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_THP_BYPASS_ADV);
	if (phy_data < 0)
		return phy_data;
	if (phy_data & NSS_PHY_MMD7_FR_THP_BYPASS_ADV_2500M)
		*adv_bitmap |= NSS_PHY_FR_THP_BYPASS_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_FR_THP_BYPASS_ADV_5000M)
		*adv_bitmap |= NSS_PHY_FR_THP_BYPASS_5000BASE_T;

	return 0;
}

static int nss_phy_c45_common_fr_ieee_adv_set(struct nss_phy_device *nss_phydev,
	u32 mask, u32 bitmap)
{
	u16 reg_mask = 0, reg_value = 0;
	int ret = 0;

	if (mask & (NSS_PHY_FR_2500BASE_T | NSS_PHY_FR_5000BASE_T |
			NSS_PHY_FR_10000BASE_T)) {
		reg_mask = 0;
		reg_value = 0;
		if (mask & NSS_PHY_FR_2500BASE_T) {
			reg_mask |= NSS_PHY_MMD7_FR_ADV_2500M;
			if (bitmap & NSS_PHY_FR_2500BASE_T)
				reg_value |= NSS_PHY_MMD7_FR_ADV_2500M;
		}
		if (mask & NSS_PHY_FR_5000BASE_T) {
			reg_mask |= NSS_PHY_MMD7_FR_ADV_5000M;
			if (bitmap & NSS_PHY_FR_5000BASE_T)
				reg_value |= NSS_PHY_MMD7_FR_ADV_5000M;
		}
		if (mask & NSS_PHY_FR_10000BASE_T) {
			reg_mask |= NSS_PHY_MMD7_FR_ADV_10G;
			if (bitmap & NSS_PHY_FR_10000BASE_T)
				reg_value |= NSS_PHY_MMD7_FR_ADV_10G;
		}
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_FR_ADV, reg_mask, reg_value);
		if (ret < 0)
			return ret;
	}

	if (mask & (NSS_PHY_FR_THP_BYPASS_2500BASE_T |
			NSS_PHY_FR_THP_BYPASS_5000BASE_T)) {
		reg_mask = 0;
		reg_value = 0;
		if (mask & NSS_PHY_FR_THP_BYPASS_2500BASE_T) {
			reg_mask |= NSS_PHY_MMD7_FR_THP_BYPASS_ADV_2500M;
			if (bitmap & NSS_PHY_FR_THP_BYPASS_2500BASE_T)
				reg_value |= NSS_PHY_MMD7_FR_THP_BYPASS_ADV_2500M;
		}
		if (mask & NSS_PHY_FR_THP_BYPASS_5000BASE_T) {
			reg_mask |= NSS_PHY_MMD7_FR_THP_BYPASS_ADV_5000M;
			if (bitmap & NSS_PHY_FR_THP_BYPASS_5000BASE_T)
				reg_value |= NSS_PHY_MMD7_FR_THP_BYPASS_ADV_5000M;
		}
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_FR_THP_BYPASS_ADV, reg_mask, reg_value);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int nss_phy_c45_common_fr_ieee_lp_ability_get(struct nss_phy_device *nss_phydev,
	u32 *ability_bitmap)
{
	int phy_data = 0;

	*ability_bitmap = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LP_FR_ABILITY);
	if (phy_data < 0)
		return phy_data;
	if (phy_data & NSS_PHY_MMD7_LP_FR_ABILITY_2500M)
		*ability_bitmap |= NSS_PHY_FR_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_LP_FR_ABILITY_5000M)
		*ability_bitmap |= NSS_PHY_FR_5000BASE_T;
	if (phy_data & NSS_PHY_MMD7_LP_FR_ABILITY_10G)
		*ability_bitmap |= NSS_PHY_FR_10000BASE_T;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_41);
	if (phy_data < 0)
		return phy_data;
	if (phy_data & NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_2500M)
		*ability_bitmap |= NSS_PHY_FR_THP_BYPASS_2500BASE_T;
	if (phy_data & NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_5000M)
		*ability_bitmap |= NSS_PHY_FR_THP_BYPASS_5000BASE_T;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_42);
	if (phy_data < 0)
		return phy_data;
	if (phy_data & NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_10G)
		*ability_bitmap |= NSS_PHY_FR_THP_BYPASS_10000BASE_T;

	return 0;
}

/* Reads MMD3.0xa038 once and derives both the local advertisement and the
 * link-partner ability bitmaps from that single snapshot, so the two
 * bitmaps are guaranteed consistent with each other.
 */
static int nss_phy_c45_common_fr_cisco_adv_and_lp_ability_get(
	struct nss_phy_device *nss_phydev, u32 *adv_bitmap, u32 *ability_bitmap)
{
	int phy_data = 0;

	*adv_bitmap = 0;
	*ability_bitmap = 0;

	phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CFR_CTRL);
	if (phy_data < 0)
		return phy_data;

	if (phy_data & NSS_PHY_MMD3_CFR_ADV)
		*adv_bitmap |= NSS_PHY_FR_CISCO_ABILITY;
	if (phy_data & NSS_PHY_MMD3_CFR_THP_BYPASS_ADV)
		*adv_bitmap |= NSS_PHY_FR_CISCO_THP_BYPASS_ABILITY;
	if (phy_data & NSS_PHY_MMD3_CFR_EXTEND_WAIT_ADV)
		*adv_bitmap |= NSS_PHY_FR_CISCO_EXTEND_WAIT_ABILITY;
	if (phy_data & NSS_PHY_MMD3_CFR_DISABLE_TIMER_ADV)
		*adv_bitmap |= NSS_PHY_FR_CISCO_DISABLE_TIMER_ABILITY;

	if (phy_data & NSS_PHY_MMD3_LP_CFR_ABILITY)
		*ability_bitmap |= NSS_PHY_FR_CISCO_ABILITY;
	if (phy_data & NSS_PHY_MMD3_LP_CFR_THP_BYPASS_ABILITY)
		*ability_bitmap |= NSS_PHY_FR_CISCO_THP_BYPASS_ABILITY;
	if (phy_data & NSS_PHY_MMD3_LP_CFR_EXTEND_WAIT_ABILITY)
		*ability_bitmap |= NSS_PHY_FR_CISCO_EXTEND_WAIT_ABILITY;
	if (phy_data & NSS_PHY_MMD3_LP_CFR_DISABLE_TIMER_ABILITY)
		*ability_bitmap |= NSS_PHY_FR_CISCO_DISABLE_TIMER_ABILITY;

	return 0;
}

static int nss_phy_c45_common_fr_cisco_adv_set(struct nss_phy_device *nss_phydev,
	u32 mask, u32 bitmap)
{
	u16 reg_mask = 0, reg_value = 0;

	if (mask & NSS_PHY_FR_CISCO_ABILITY) {
		reg_mask |= NSS_PHY_MMD3_CFR_ADV;
		if (bitmap & NSS_PHY_FR_CISCO_ABILITY)
			reg_value |= NSS_PHY_MMD3_CFR_ADV;
	}
	if (mask & NSS_PHY_FR_CISCO_THP_BYPASS_ABILITY) {
		reg_mask |= NSS_PHY_MMD3_CFR_THP_BYPASS_ADV;
		if (bitmap & NSS_PHY_FR_CISCO_THP_BYPASS_ABILITY)
			reg_value |= NSS_PHY_MMD3_CFR_THP_BYPASS_ADV;
	}
	if (mask & NSS_PHY_FR_CISCO_EXTEND_WAIT_ABILITY) {
		reg_mask |= NSS_PHY_MMD3_CFR_EXTEND_WAIT_ADV;
		if (bitmap & NSS_PHY_FR_CISCO_EXTEND_WAIT_ABILITY)
			reg_value |= NSS_PHY_MMD3_CFR_EXTEND_WAIT_ADV;
	}
	if (mask & NSS_PHY_FR_CISCO_DISABLE_TIMER_ABILITY) {
		reg_mask |= NSS_PHY_MMD3_CFR_DISABLE_TIMER_ADV;
		if (bitmap & NSS_PHY_FR_CISCO_DISABLE_TIMER_ABILITY)
			reg_value |= NSS_PHY_MMD3_CFR_DISABLE_TIMER_ADV;
	}

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CFR_CTRL, reg_mask, reg_value);
}

/* Cisco FR is enabled iff both its local advertisement bit
 * (MMD3.0xa038[3]) and the global FR enable bit (MMD1.93) are set. Reads
 * the cache instead of MMD1.93 directly since that register is
 * read-clear (see nss_phy_c45_common_fr_cnt_get()).
 */
static bool nss_phy_c45_common_fr_cisco_mode_get(struct nss_phy_device *nss_phydev)
{
	return nss_phydev->fr_cisco_enabled;
}

/* IEEE FR is enabled iff the global FR enable bit is set and the
 * IEEE FR bypass bit is clear (the bypass bit is set only when Cisco FR
 * is active without IEEE FR). Reads the cache for the same reason as
 * nss_phy_c45_common_fr_cisco_mode_get() above.
 */
static bool nss_phy_c45_common_fr_ieee_mode_get(struct nss_phy_device *nss_phydev)
{
	return nss_phydev->fr_ieee_enabled;
}

/* Derives nss_phydev->fr_ieee_enabled/fr_cisco_enabled from the current
 * hardware advertisement state instead of assuming the power-on default,
 * so a bootloader that rewrote these (non read-clear) registers before
 * the driver probed, or a prior fr_cfg_apply() that failed partway
 * through, doesn't leave the cache out of sync with the hardware.
 */
void nss_phy_c45_common_fr_state_init(struct nss_phy_device *nss_phydev)
{
	int ieee_adv, cisco_adv, bypass;

	ieee_adv = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_ADV);
	if (ieee_adv < 0)
		nss_phy_warn(nss_phydev,
			"fr_state_init: MMD7 FR_ADV read failed (%d), cache defaults to disabled\n",
			ieee_adv);

	cisco_adv = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CFR_CTRL);
	if (cisco_adv < 0)
		nss_phy_warn(nss_phydev,
			"fr_state_init: MMD3 CFR_CTRL read failed (%d), cache defaults to disabled\n",
			cisco_adv);

	bypass = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_BYPASS_25);

	nss_phydev->fr_cisco_enabled = cisco_adv >= 0 &&
		!!(cisco_adv & NSS_PHY_MMD3_CFR_ADV);

	/* When Cisco-only FR is active, fr_cfg_apply() writes IEEE ADV bits
	 * (2.5G/5G) as the Cisco bring-up channel and sets the bypass bit.
	 * Reading those IEEE ADV bits alone would falsely infer fr_ieee_enabled.
	 * Disambiguate: IEEE FR is truly enabled only when the IEEE ADV bits
	 * are set AND the bypass bit is NOT set (bypass is always clear for
	 * pure IEEE FR, always set for Cisco-only).
	 */
	nss_phydev->fr_ieee_enabled = ieee_adv >= 0 &&
		!!(ieee_adv & (NSS_PHY_MMD7_FR_ADV_2500M |
		NSS_PHY_MMD7_FR_ADV_5000M | NSS_PHY_MMD7_FR_ADV_10G)) &&
		!(bypass >= 0 && (bypass & NSS_PHY_MMD7_FR_BYPASS_2500M));
}

/* MMD1.93 (NSS_PHY_MMD1_FR_CONTROL) is read-clear (see
 * nss_phy_c45_common_fr_cnt_get()) - the only bit this function writes
 * is NSS_PHY_MMD1_FR_ENABLE.  No additional lock is needed for the
 * concurrent polling worker: write operations do not affect read-clear
 * bits, so writing EN does not discard any accumulated count delta.
 */
static int nss_phy_c45_common_fr_cfg_apply(struct nss_phy_device *nss_phydev,
	u32 ieee_enable, u32 cisco_enable)
{
	u32 ieee_bitmap = 0;
	u32 ieee_mask = NSS_PHY_FR_2500BASE_T | NSS_PHY_FR_5000BASE_T |
		NSS_PHY_FR_10000BASE_T;
	int ret = 0;

	/* FR has no defined behavior below 2.5G. */
	if (!nss_phy_support_2500(nss_phydev))
		return -NSS_PHY_EOPNOTSUPP;

	/* Avoid an unnecessary autoneg restart (and the resulting link
	 * flap) when the requested config already matches the cache.
	 */
	if (!!ieee_enable == nss_phydev->fr_ieee_enabled &&
		!!cisco_enable == nss_phydev->fr_cisco_enabled)
		return 0;

	if (ieee_enable) {
		ieee_bitmap = NSS_PHY_FR_2500BASE_T;
		if (nss_phy_support_5g(nss_phydev))
			ieee_bitmap |= NSS_PHY_FR_5000BASE_T;
		if (nss_phy_support_10g(nss_phydev))
			ieee_bitmap |= NSS_PHY_FR_10000BASE_T;
	} else if (cisco_enable) {
		/* IEEE FR's advertisement register also serves as Cisco
		 * FR's bring-up channel when Cisco FR is active without
		 * IEEE FR.
		 */
		ieee_bitmap = NSS_PHY_FR_2500BASE_T;
		if (nss_phy_support_5g(nss_phydev))
			ieee_bitmap |= NSS_PHY_FR_5000BASE_T;
	}

	ret = nss_phy_c45_common_fr_ieee_adv_set(nss_phydev, ieee_mask,
		ieee_bitmap);
	if (ret < 0)
		goto resync;

	ret = nss_phy_c45_common_fr_cisco_adv_set(nss_phydev,
		NSS_PHY_FR_CISCO_ABILITY,
		cisco_enable ? NSS_PHY_FR_CISCO_ABILITY : 0);
	if (ret < 0)
		goto resync;

	ret = nss_phy_c45_common_fr_bypass_set(nss_phydev,
		(cisco_enable && !ieee_enable) ? 1 : 0);
	if (ret < 0)
		goto resync;

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_FR_CONTROL,
		(ieee_enable || cisco_enable) ? NSS_PHY_MMD1_FR_ENABLE : 0);
	if (ret < 0)
		goto resync;

	nss_phydev->fr_ieee_enabled = !!ieee_enable;
	nss_phydev->fr_cisco_enabled = !!cisco_enable;

	return nss_phy_c45_common_autoneg_restart(nss_phydev);

resync:
	/* Partial write: re-read hardware to keep cache consistent. */
	nss_phy_c45_common_fr_state_init(nss_phydev);
	return ret;
}

int nss_phy_c45_common_fr_cfg_set(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_cfg *cfg)
{
	if (!cfg)
		return -NSS_PHY_EINVAL;

	if (nss_phydev_link_get(nss_phydev) != NSS_PHY_FALSE &&
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	return nss_phy_c45_common_fr_cfg_apply(nss_phydev, cfg->ieee_fr_en,
		cfg->cisco_fr_en);
}

int nss_phy_c45_common_fr_cfg_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_cfg *cfg)
{
	if (!cfg)
		return -NSS_PHY_EINVAL;

	memset(cfg, 0, sizeof(*cfg));

	if (nss_phydev_link_get(nss_phydev) != NSS_PHY_FALSE &&
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	cfg->ieee_fr_en = nss_phy_c45_common_fr_ieee_mode_get(nss_phydev);
	cfg->cisco_fr_en = nss_phy_c45_common_fr_cisco_mode_get(nss_phydev);

	return 0;
}

int nss_phy_c45_common_fr_status_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_status *status)
{
	u32 ieee_adv_bitmap = 0, ieee_lp_bitmap = 0;
	u32 cisco_adv_bitmap = 0, cisco_lp_bitmap = 0;
	int raw_8201 = 0;
	bool fail = false, success = false, start = false;
	int ret = 0;

	/* fal_port_ctrl.c passes this struct as (void*) cast to
	 * fal_port_fr_status_t whose bool-ish fields are enum a_bool_t.
	 * Assert field widths, offsets, and total size so layout mismatches
	 * between this struct and ssdk's fal_port_fr_status_t are caught at
	 * compile time.
	 */
	BUILD_BUG_ON(sizeof(((struct nss_phy_fr_status *)0)->ieee_enabled) !=
		sizeof(u32));
	BUILD_BUG_ON(offsetof(struct nss_phy_fr_status, fail) !=
		5 * sizeof(u32));
	BUILD_BUG_ON(offsetof(struct nss_phy_fr_status, rx_count) !=
		6 * sizeof(u32));
	BUILD_BUG_ON(sizeof(struct nss_phy_fr_status) !=
		6 * sizeof(u32) + 4 * sizeof(u64));

	if (!status)
		return -NSS_PHY_EINVAL;

	memset(status, 0, sizeof(*status));

	if (nss_phydev_link_get(nss_phydev) != NSS_PHY_FALSE &&
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	status->ieee_enabled = nss_phy_c45_common_fr_ieee_mode_get(nss_phydev);
	status->cisco_enabled = nss_phy_c45_common_fr_cisco_mode_get(nss_phydev);

	ret = nss_phy_c45_common_fr_ieee_adv_get(nss_phydev, &ieee_adv_bitmap);
	if (ret < 0)
		return ret;

	ret = nss_phy_c45_common_fr_ieee_lp_ability_get(nss_phydev,
		&ieee_lp_bitmap);
	if (ret < 0)
		return ret;

	ret = nss_phy_c45_common_fr_cisco_adv_and_lp_ability_get(nss_phydev,
		&cisco_adv_bitmap, &cisco_lp_bitmap);
	if (ret < 0)
		return ret;

	raw_8201 = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_8201_FR_STATUS);
	if (raw_8201 < 0)
		return raw_8201;

	fail = !!(raw_8201 & NSS_PHY_MMD1_8201_FR_FAIL);
	success = !!(raw_8201 & NSS_PHY_MMD1_8201_FR_SUCCESS);
	start = !!(raw_8201 & NSS_PHY_MMD1_8201_FR_START);

	status->negotiated = (ieee_adv_bitmap & ieee_lp_bitmap) |
		(cisco_adv_bitmap & cisco_lp_bitmap);

	status->active = (status->ieee_enabled || status->cisco_enabled) &&
		start;
	status->success = success;
	status->fail = fail;

	spin_lock_bh(&nss_phydev->fr_sw_cnt_lock);
	status->rx_count = nss_phydev->fr_sw_cnt.rx_count;
	status->tx_count = nss_phydev->fr_sw_cnt.tx_count;
	status->rx_total = nss_phydev->fr_sw_cnt.rx_total;
	status->tx_total = nss_phydev->fr_sw_cnt.tx_total;
	spin_unlock_bh(&nss_phydev->fr_sw_cnt_lock);

	return 0;
}

/* MMD1.93 is read-clear: each read returns the rx/tx retrain count
 * accumulated since the previous read, then resets to 0 in hardware.
 */
static int nss_phy_c45_common_fr_cnt_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_cnt *cnt)
{
	int raw_93 = 0;

	if (!cnt)
		return -NSS_PHY_EINVAL;

	memset(cnt, 0, sizeof(*cnt));

	if (nss_phydev_link_get(nss_phydev) != NSS_PHY_FALSE &&
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	raw_93 = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_FR_CONTROL);
	if (raw_93 < 0)
		return raw_93;

	cnt->rx_count = (raw_93 & NSS_PHY_MMD1_FR_RX_CNT_MASK) >>
		NSS_PHY_MMD1_FR_RX_CNT_SHIFT;
	cnt->tx_count = (raw_93 & NSS_PHY_MMD1_FR_TX_CNT_MASK) >>
		NSS_PHY_MMD1_FR_TX_CNT_SHIFT;

	if (cnt->rx_count == NSS_PHY_MMD1_FR_CNT_MAX ||
	    cnt->tx_count == NSS_PHY_MMD1_FR_CNT_MAX)
		nss_phy_warn(nss_phydev,
			"FR counter saturated (rx=%u tx=%u): retrain rate >31/s, counts may be lost\n",
			cnt->rx_count, cnt->tx_count);

	return 0;
}

/* Accumulates the read-clear rx/tx retrain delta into nss_phydev->fr_sw_cnt:
 * rx/tx_total are cumulative across all link sessions.
 * rx/tx_count count events since the most recent link-up; on a link-up
 * transition the pre-link-down residual delta is added to _total only.
 *
 * Hardware counter is 5 bits (max 31 per read); with a 1-second poll period
 * up to 31 retrains/s can be captured without loss.
 */
int nss_phy_c45_common_fr_cnt_poll(struct nss_phy_device *nss_phydev,
	bool link_up_transition)
{
	struct nss_phy_fr_cnt cnt = {0};
	int ret;

	ret = nss_phy_c45_common_fr_cnt_get(nss_phydev, &cnt);

	spin_lock_bh(&nss_phydev->fr_sw_cnt_lock);
	if (!ret) {
		nss_phydev->fr_sw_cnt.rx_total += cnt.rx_count;
		nss_phydev->fr_sw_cnt.tx_total += cnt.tx_count;
		if (link_up_transition) {
			/* Discard the pre-link-down residual from the
			 * since-link-up counters; _total already got it above.
			 */
			nss_phydev->fr_sw_cnt.rx_count = 0;
			nss_phydev->fr_sw_cnt.tx_count = 0;
		} else {
			nss_phydev->fr_sw_cnt.rx_count += cnt.rx_count;
			nss_phydev->fr_sw_cnt.tx_count += cnt.tx_count;
		}
	}
	spin_unlock_bh(&nss_phydev->fr_sw_cnt_lock);

	return ret;
}

int nss_phy_c45_common_fr_trigger(struct nss_phy_device *nss_phydev)
{
	bool ieee_enabled, cisco_enabled;
	int ret = 0;

	if (nss_phydev_link_get(nss_phydev) == NSS_PHY_FALSE ||
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	ieee_enabled = nss_phy_c45_common_fr_ieee_mode_get(nss_phydev);
	cisco_enabled = nss_phy_c45_common_fr_cisco_mode_get(nss_phydev);

	/* Both IEEE and Cisco FR disabled; nothing to trigger. */
	if (!(ieee_enabled || cisco_enabled))
		return 0;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_FR_TRIGGER_REG, NSS_PHY_MMD3_FR_TRIGGER, 0);
	if (ret < 0)
		return ret;

	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_FR_TRIGGER_REG, NSS_PHY_MMD3_FR_TRIGGER,
		NSS_PHY_MMD3_FR_TRIGGER);
}

/*
 * nss_phy_c45_common_pcs_status_get()
 *
 * Reads PCS Status (MMD3.0x20) for pcs_locked and block_lock.
 * Valid only at 2.5G and above; returns 0 at lower speeds.
 */
int nss_phy_c45_common_pcs_status_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_pcs_status *status)
{
	int pcs_status;

	if (!status)
		return -NSS_PHY_EINVAL;

	if (!nss_phydev_link_get(nss_phydev) ||
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	memset(status, 0, sizeof(*status));

	/* PCS Status (MMD3.0x20): pcs_locked (bit 12) and block_lock (bit 0) */
	pcs_status = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_PCS_STATUS);
	if (pcs_status < 0)
		return pcs_status;

	status->pcs_locked = !!(pcs_status & NSS_PHY_MMD3_PCS_STATUS_LOCKED);
	status->block_lock = !!(pcs_status & NSS_PHY_MMD3_PCS_STATUS_BLK_LOCK);

	return 0;
}

/*
 * nss_phy_c45_common_link_training_completion_get()
 *
 * Reads link training completion bitmap from MMD7.0x8001 bits[10:8].
 * Valid only at 2.5G and above; returns 0 at lower speeds.
 */
int nss_phy_c45_common_link_training_completion_get(struct nss_phy_device *nss_phydev,
	u32 *training_complete)
{
	int autoneg_status;

	if (!training_complete)
		return -NSS_PHY_EINVAL;

	if (!nss_phydev_link_get(nss_phydev) ||
		nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	*training_complete = 0;

	autoneg_status = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_AUTONEG_STATUS);
	if (autoneg_status < 0)
		return autoneg_status;

	if (autoneg_status & NSS_PHY_MMD7_LINK_OK_10G)
		*training_complete |= NSS_PHY_TRAINING_COMPLETE_10G;
	if (autoneg_status & NSS_PHY_MMD7_LINK_OK_5G)
		*training_complete |= NSS_PHY_TRAINING_COMPLETE_5G;
	if (autoneg_status & NSS_PHY_MMD7_LINK_OK_2P5G)
		*training_complete |= NSS_PHY_TRAINING_COMPLETE_2P5G;

	return 0;
}

static int nss_phy_c45_pma_monitor_set(struct nss_phy_device *nss_phydev, bool enable)
{
	u16 val;
	int ret;

	val = enable ? NSS_PHY_MMD3_PHY_PMA_MONITOR_EN0 : 0;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_PHY_MISC_CTRL0,
		NSS_PHY_MMD3_PHY_PMA_MONITOR_EN0, val);
	if (enable && ret < 0)
		return ret;

	val = enable ? NSS_PHY_MMD3_PHY_PMA_MONITOR_EN1 : 0;
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_PHY_MISC_CTRL1,
		NSS_PHY_MMD3_PHY_PMA_MONITOR_EN1, val);
	if (enable && ret < 0)
		return ret;

	val = enable ? NSS_PHY_MMD3_PHY_PMA_MONITOR_EN : 0;
	nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_PHY_PMA_MONITOR_STATUS,
		NSS_PHY_MMD3_PHY_PMA_MONITOR_MASK, val);

	return 0;
}

int nss_phy_c45_common_mse_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_mse *mse)
{
	static const struct {
		u32 great;
		u32 good;
		u32 normal;
		u32 crc;
	} thresh_10g     = { 166, 332, 526,  1321 },
	  thresh_2_5g_5g = { 166, 418, 662,  1321 };
	const typeof(thresh_10g) *thresh;
	static const u16 ch_regs[4] = {
		NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH0, NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH1,
		NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH2, NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH3,
	};
	u32 speed;
	int val, ret, i;

	if (!mse)
		return -NSS_PHY_EINVAL;

	speed = nss_phydev_speed_get(nss_phydev);

	if (speed == NSS_PHY_SPEED_100 || speed == NSS_PHY_SPEED_1000)
		return nss_phy_common_mse_get(nss_phydev, mse);

	memset(mse, 0, sizeof(*mse));

	if (speed != NSS_PHY_SPEED_2500 && speed != NSS_PHY_SPEED_5000 &&
		speed != NSS_PHY_SPEED_10000)
		return -NSS_PHY_EOPNOTSUPP;

	mutex_lock(&nss_phydev->mse_lock);

	ret = nss_phy_c45_pma_monitor_set(nss_phydev, true);
	if (ret < 0) {
		mutex_unlock(&nss_phydev->mse_lock);
		return ret;
	}

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_PHY_MISC_CTRL0,
		NSS_PHY_MMD3_MSE_2_5G_5G_10G_EN,
		NSS_PHY_MMD3_MSE_2_5G_5G_10G_EN);
	if (ret < 0) {
		nss_phy_c45_pma_monitor_set(nss_phydev, false);
		mutex_unlock(&nss_phydev->mse_lock);
		return ret;
	}

	/* MSE accumulator needs ~4s to collect samples after PMA monitor enable. */
	nss_phy_msleep(4000);

	for (i = 0; i < 4; i++) {
		val = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM, ch_regs[i]);
		if (val < 0) {
			mse->quality[i] = NSS_PHY_MSE_QUALITY_NA;
			continue;
		}
		mse->mse[i] = (u16)val;
		mse->quality[i] = NSS_PHY_MSE_QUALITY_LINK_DOWN; /* threshold loop will classify */
	}

	nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_PHY_MISC_CTRL0,
		NSS_PHY_MMD3_MSE_2_5G_5G_10G_EN, 0);

	nss_phy_c45_pma_monitor_set(nss_phydev, false);

	mutex_unlock(&nss_phydev->mse_lock);

	thresh = (speed == NSS_PHY_SPEED_10000) ? &thresh_10g : &thresh_2_5g_5g;

	for (i = 0; i < 4; i++) {
		u32 v = mse->mse[i];
		s32 thresh_centi_db = (speed == NSS_PHY_SPEED_10000) ? -2500 : -2700;

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

		/* num = 85 * 2^12 = 348160 for 2.5G/5G/10G */
		mse->snr_margin[i] = v ? nss_phy_mse_to_snr_centi_db(348160, v) + thresh_centi_db : 0;
	}

	return 0;
}

static int nss_phy_c45_ms_read(struct nss_phy_device *nss_phydev, u16 *val)
{
	int ret = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM, NSS_PHY_MMD7_FR_ADV);

	if (ret < 0)
		return ret;
	*val = (u16)ret & NSS_PHY_AN_MS_CTRL_MASK;
	return 0;
}

static int nss_phy_c45_ms_write(struct nss_phy_device *nss_phydev, u16 mask, u16 val)
{
	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_FR_ADV, mask, val);
}

int nss_phy_c45_common_ms_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_ms_mode mode)
{
	u16 val;
	int ret;

	switch (mode) {
	case NSS_PHY_MS_AUTO:
		val = 0;
		break;
	case NSS_PHY_MS_PREFER_MASTER:
		val = NSS_PHY_AN_MS_PREFER_MASTER;
		break;
	case NSS_PHY_MS_FORCE_MASTER:
		val = NSS_PHY_AN_MS_FORCE_EN | NSS_PHY_AN_MS_MASTER_VAL;
		break;
	case NSS_PHY_MS_FORCE_SLAVE:
		val = NSS_PHY_AN_MS_FORCE_EN;
		break;
	default:
		return -NSS_PHY_EINVAL;
	}

	ret = nss_phy_c45_ms_write(nss_phydev, NSS_PHY_AN_MS_CTRL_MASK, val);
	if (ret < 0)
		return ret;

	return nss_phy_c45_common_autoneg_restart(nss_phydev);
}

int nss_phy_c45_common_ms_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_ms_mode *mode)
{
	u16 val;
	int ret = nss_phy_c45_ms_read(nss_phydev, &val);

	if (ret < 0)
		return ret;

	if (!(val & NSS_PHY_AN_MS_FORCE_EN))
		*mode = (val & NSS_PHY_AN_MS_PREFER_MASTER) ?
			NSS_PHY_MS_PREFER_MASTER : NSS_PHY_MS_AUTO;
	else
		*mode = (val & NSS_PHY_AN_MS_MASTER_VAL) ?
			NSS_PHY_MS_FORCE_MASTER : NSS_PHY_MS_FORCE_SLAVE;

	return 0;
}

/* Returns 1 if local PHY resolved as master, 0 if slave, <0 on error.
 * MMD7.0x21 bit 14: set means local PHY resolved as master (IEEE 802.3-2018 §45.2.55).
 */
int nss_phy_c45_common_ms_status_get(struct nss_phy_device *nss_phydev)
{
	int ret = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
		NSS_PHY_MMD7_LP_FR_ABILITY);

	if (ret < 0)
		return ret;

	return ((u16)ret & NSS_PHY_AN_MS_RESOLUTION_MASTER) ? 1 : 0;
}

int nss_phy_c45_common_link_status_get(struct nss_phy_device *nss_phydev, bool is_c45)
{
	int val = is_c45
		? nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD31_NUM, NSS_PHY_SPEC_STATUS)
		: nss_phy_read(nss_phydev, NSS_PHY_SPEC_STATUS);

	if (val < 0)
		return val;

	return !!(val & NSS_PHY_SPEC_STATUS_LINK_UP);
}

int nss_phy_c45_common_clk_ppm_offset_get(struct nss_phy_device *nss_phydev,
	int *ppm, u64 div_1g, u64 div_2_5g, u64 div_5g_10g)
{
	bool is_c45 = nss_phydev_is_c45(nss_phydev);
	enum nss_phy_ms_mode prev_ms_mode;
	bool ms_forced = false;
	u64 divisor;
	bool link_up = false;
	int ret, raw;
	u32 speed;
	int i;

	if (!ppm)
		return -NSS_PHY_EINVAL;

	speed = nss_phydev_speed_get(nss_phydev);

	switch (speed) {
	case NSS_PHY_SPEED_1000:
		if (!div_1g)
			return -NSS_PHY_EOPNOTSUPP;
		divisor = div_1g;
		break;
	case NSS_PHY_SPEED_2500:
		if (!div_2_5g)
			return -NSS_PHY_EOPNOTSUPP;
		divisor = div_2_5g;
		break;
	case NSS_PHY_SPEED_5000:
	case NSS_PHY_SPEED_10000:
		if (!div_5g_10g)
			return -NSS_PHY_EOPNOTSUPP;
		divisor = div_5g_10g;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}

	/*
	 * Serialise the entire ms-swap → measure → restore sequence.
	 * Using a dedicated mutex (not phydev->lock) avoids deadlock with
	 * the PHY state machine.
	 */
	mutex_lock(&nss_phydev->ppm_lock);

	/* PPM measurement requires the local PHY to be in slave role.
	 * If already slave, read PPM directly.  If master, force slave,
	 * wait for link-up via hardware register (MMD31.0x11 bit 10 for C45,
	 * MII 0x11 bit 10 for C22), then measure.
	 */
	ret = is_c45 ? nss_phy_c45_common_ms_status_get(nss_phydev)
		     : nss_phy_common_ms_status_get(nss_phydev);
	if (ret < 0)
		goto unlock;

	if (ret != 0) {
		/* Currently master — save config, force slave, wait for link */
		ret = is_c45 ? nss_phy_c45_common_ms_get(nss_phydev, &prev_ms_mode)
			     : nss_phy_common_ms_get(nss_phydev, &prev_ms_mode);
		if (ret < 0)
			goto unlock;

		ret = is_c45 ? nss_phy_c45_common_ms_set(nss_phydev, NSS_PHY_MS_FORCE_SLAVE)
			     : nss_phy_common_ms_set(nss_phydev, NSS_PHY_MS_FORCE_SLAVE);
		if (ret < 0)
			goto unlock;
		ms_forced = true;

		for (i = 0; i < 150; i++) {
			nss_phy_msleep(100);
			ret = nss_phy_c45_common_link_status_get(nss_phydev, is_c45);
			if (ret < 0)
				goto restore;
			if (ret) {
				link_up = true;
				break;
			}
		}

		if (!link_up) {
			ret = -NSS_PHY_ETIMEOUT;
			goto restore;
		}
	} else {
		/* Already slave — verify link is actually up before reading PPM.
		 * ms_status_get may return 0 (no resolution) when link is down,
		 * which would cause a stale PPM read.
		 */
		ret = nss_phy_c45_common_link_status_get(nss_phydev, is_c45);
		if (ret < 0)
			goto unlock;
		if (!ret) {
			ret = -NSS_PHY_ENOLINK;
			goto unlock;
		}
		link_up = true;
	}

	if (speed == NSS_PHY_SPEED_1000) {
		ret = nss_phy_modify_debug(nss_phydev, NSS_PHY_DEBUG_CONTROL_REGISTER0,
				     NSS_PHY_DEBUG_MSE_100M_1G_EN, NSS_PHY_DEBUG_MSE_100M_1G_EN);
		if (ret < 0)
			goto restore;
		nss_phy_msleep(100);
		raw = nss_phy_read_debug(nss_phydev, NSS_PHY_DEBUG_PPM_OFFSET);
	} else {
		/* NSS_PHY_MMD3_PHY_MISC_CTRL0 BIT(2) enables the PPM database. */
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_PHY_MISC_CTRL0,
			NSS_PHY_MMD3_PHY_PMA_MONITOR_EN0,
			NSS_PHY_MMD3_PHY_PMA_MONITOR_EN0);
		if (ret < 0)
			goto restore;
		/* NSS_PHY_MMD3_PHY_MISC_CTRL1 BIT(13) is the PPM capture trigger;
		 * the hardware self-clears it after latching the offset value,
		 * so no explicit restore of this bit is required.
		 */
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_PHY_MISC_CTRL1,
			NSS_PHY_MMD3_PHY_PMA_MONITOR_EN1,
			NSS_PHY_MMD3_PHY_PMA_MONITOR_EN1);
		if (ret < 0)
			goto restore;
		/* Allow the hardware one measurement window to latch the
		 * PPM offset after the capture trigger (BIT(13)) is set.
		 */
		nss_phy_msleep(100);
		raw = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_PPM_OFFSET);
	}

	if (raw < 0) {
		ret = raw;
		goto restore;
	}

	*ppm = (int)div_s64((s64)((s16)(u16)raw) * 1000000, (s64)divisor);
	ret = 0;

restore:
	if (link_up) {
		if (speed == NSS_PHY_SPEED_1000)
			nss_phy_modify_debug(nss_phydev, NSS_PHY_DEBUG_CONTROL_REGISTER0,
					     NSS_PHY_DEBUG_MSE_100M_1G_EN, 0);
		else
			nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
				NSS_PHY_MMD3_PHY_MISC_CTRL0,
				NSS_PHY_MMD3_PHY_PMA_MONITOR_EN0, 0);
	}
	if (ms_forced) {
		if (is_c45)
			nss_phy_c45_common_ms_set(nss_phydev, prev_ms_mode);
		else
			nss_phy_common_ms_set(nss_phydev, prev_ms_mode);
	}

unlock:
	mutex_unlock(&nss_phydev->ppm_lock);
	return ret;
}

/*
 * nss_phy_c45_common_ldpc_stats_get()
 *	Read LDPC decoder statistics from the per-iteration bucket counters.
 *
 * Each of the five iteration buckets is stored as four consecutive 16-bit
 * sub-registers (a=LSW .. d=MSW) in MMD3.  0xFFFFFFFFFFFFFFFF (all sub-registers
 * 0xffff) is the hardware sentinel for "counter not available / clamped";
 * treat the whole bucket as zero.
 *
 * Only meaningful when the link is operating at 2.5G or above
 * (IEEE 802.3bz for NBase-T, IEEE 802.3an for 10GBASE-T).
 * Returns -NSS_PHY_EOPNOTSUPP at lower speeds, -NSS_PHY_EINVAL if @stats is NULL,
 * or a negative MDIO error code on register read failure.
 *
 * avg_iter_x100 is the weighted average of iterations needed to decode a
 * codeword, scaled by 100.  iter[0] (decoded without any extra iteration) has
 * weight 0 and is counted only in the denominator; iter[1..4] carry weights 1..4.
 */
int nss_phy_c45_common_ldpc_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_ldpc_stats *stats)
{
	static const u16 iter_regs[5][4] = {
		{ NSS_PHY_MMD3_LDPC_ITER0_A, NSS_PHY_MMD3_LDPC_ITER0_B,
		  NSS_PHY_MMD3_LDPC_ITER0_C, NSS_PHY_MMD3_LDPC_ITER0_D },
		{ NSS_PHY_MMD3_LDPC_ITER1_A, NSS_PHY_MMD3_LDPC_ITER1_B,
		  NSS_PHY_MMD3_LDPC_ITER1_C, NSS_PHY_MMD3_LDPC_ITER1_D },
		{ NSS_PHY_MMD3_LDPC_ITER2_A, NSS_PHY_MMD3_LDPC_ITER2_B,
		  NSS_PHY_MMD3_LDPC_ITER2_C, NSS_PHY_MMD3_LDPC_ITER2_D },
		{ NSS_PHY_MMD3_LDPC_ITER3_A, NSS_PHY_MMD3_LDPC_ITER3_B,
		  NSS_PHY_MMD3_LDPC_ITER3_C, NSS_PHY_MMD3_LDPC_ITER3_D },
		{ NSS_PHY_MMD3_LDPC_ITER4_A, NSS_PHY_MMD3_LDPC_ITER4_B,
		  NSS_PHY_MMD3_LDPC_ITER4_C, NSS_PHY_MMD3_LDPC_ITER4_D },
	};
	static const u64 bucket_cap = U64_MAX / 1000ULL;
	u64 iter[5], iter_total, weighted;
	int i, iter_err;

	if (!stats)
		return -NSS_PHY_EINVAL;

	if (nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		return -NSS_PHY_EOPNOTSUPP;

	for (i = 0; i < 5; i++) {
		int ra, rb, rc, rd;

		ra = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM, iter_regs[i][0]);
		if (ra < 0)
			return ra;
		rb = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM, iter_regs[i][1]);
		if (rb < 0)
			return rb;
		rc = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM, iter_regs[i][2]);
		if (rc < 0)
			return rc;
		rd = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM, iter_regs[i][3]);
		if (rd < 0)
			return rd;

		iter[i] = ((u64)(u16)rd << 48) | ((u64)(u16)rc << 32) |
			  ((u64)(u16)rb << 16) | (u64)(u16)ra;
		if (iter[i] == U64_MAX)
			iter[i] = 0;
	}

	/*
	 * Clamp per-bucket values so weighted and 100*weighted cannot wrap u64.
	 * With cap = U64_MAX/1000: sum of (1+2+3+4)*cap = U64_MAX/100, and
	 * 100 * (U64_MAX/100) == U64_MAX, staying within range.
	 * iter_total uses the same clamped values for consistency.
	 */
	weighted = 1ULL * min(iter[1], bucket_cap) +
		   2ULL * min(iter[2], bucket_cap) +
		   3ULL * min(iter[3], bucket_cap) +
		   4ULL * min(iter[4], bucket_cap);

	iter_total = min(iter[0], bucket_cap) +
		     min(iter[1], bucket_cap) +
		     min(iter[2], bucket_cap) +
		     min(iter[3], bucket_cap) +
		     min(iter[4], bucket_cap);

	iter_err = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
				    NSS_PHY_MMD3_LDPC_ITER_ERR);
	if (iter_err < 0 || (u16)iter_err == 0xffff)
		iter_err = 0;

	memcpy(stats->iter, iter, sizeof(stats->iter));
	stats->uncorrected   = (u16)iter_err;
	stats->avg_iter_x100 = (iter_total == 0) ? 0 :
		(int)div64_u64(100ULL * weighted, iter_total);

	return 0;
}
