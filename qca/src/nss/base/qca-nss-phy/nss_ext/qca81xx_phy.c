/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "nss_phy.h"
#include "qca81xx_phy.h"
#include "nss_phy_c45_common.h"

static int qca81xx_phy_soft_reset(struct nss_phy_device *nss_phydev)
{
	int ret;

	/* enable auto soft reset when power on */
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_SPEC_CONTROL,
		QCA81XX_PHY_MMD31_AUTO_SOFT_RESET_EN,
		QCA81XX_PHY_MMD31_AUTO_SOFT_RESET_EN);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_PMA_CONTROL,
		NSS_PHY_POWER_DOWN,
		NSS_PHY_POWER_DOWN);
	if (ret < 0)
		return ret;
	nss_phy_mdelay(10);
	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD1_NUM,
		NSS_PHY_MMD1_PMA_CONTROL,
		NSS_PHY_POWER_DOWN,
		NSS_PHY_POWER_UP);
	if (ret < 0)
		return ret;
	nss_phy_mdelay(1);

	/* disable auto soft reset when power on */
	return nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
		NSS_PHY_SPEC_CONTROL,
		QCA81XX_PHY_MMD31_AUTO_SOFT_RESET_EN, 0);
}

static int qca81xx_phy_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	int ret;

	/* there may be packets lost when enable PMA loopback of 2.5G/5G/10G, */
	/* so enable PCS loopback instead of PMA loopback */
	if (nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500) {
		ret = nss_phy_c45_common_pma_local_loopback_set(nss_phydev, enable);
		if (ret < 0)
			return ret;
	} else {
		/* the link would drop when enable PCS loopback, so need special */
		/* sequence to work around it */
		nss_phydev_loopback_update(nss_phydev, enable);
		ret = nss_phy_common_hibernation_set(nss_phydev, !enable);
		if (ret  < 0)
			return ret;
		ret = nss_phy_c45_common_pcs_local_loopback_set(nss_phydev, enable);
		if (ret < 0)
			return ret;
		ret = qca81xx_phy_soft_reset(nss_phydev);
		if (ret < 0)
			return ret;
		/* the autoneg would be enabled after software reset, */
		/* so need to configure it again */
		ret = nss_phy_c45_common_autoneg_set(nss_phydev, !enable);
		if (ret < 0)
			return ret;
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			QCA81XX_PHY_MMD3_BYPASS_SIGNAL,
			QCA81XX_PHY_MMD3_PCS_BYPASS_LINK,
			enable ? QCA81XX_PHY_MMD3_PCS_BYPASS_LINK : 0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int qca81xx_phy_local_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	int ret;

	if (nss_phydev_speed_get(nss_phydev) < NSS_PHY_SPEED_2500)
		ret = nss_phy_c45_common_pma_local_loopback_get(nss_phydev, enable);
	else
		ret = nss_phy_c45_common_pcs_local_loopback_get(nss_phydev, enable);

	return ret;
}

static int qca81xx_phy_function_reset(struct nss_phy_device *nss_phydev,
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
		ret = qca81xx_phy_soft_reset(nss_phydev);
		if (ret < 0)
			return ret;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}

	return 0;
}

static int qca81xx_phy_cdt(struct nss_phy_device *nss_phydev, u32 mdi_pair,
	enum nss_phy_cable_status *status, u32 *cable_len)
{
	int ret, cdt_ret;
	u16 dac8_tmp, dac9_tmp;

	/* if PHY link up, report cable length via CLD; mdi_pair is not applicable */
	if (nss_phydev_link_get(nss_phydev) != NSS_PHY_FALSE) {
		*status = CABLE_NORMAL;
		return nss_phy_common_cld_cable_len_read(nss_phydev,
			NSS_PHY_MMD3_CLD_LEN, NSS_PHY_MMD3_CLD_LEN_MASK,
			cable_len);
	}

	dac8_tmp = nss_phy_read_debug(nss_phydev,
		QCA81XX_PHY_DEBUG_AFE_DAC8_DP);
	dac9_tmp = nss_phy_read_debug(nss_phydev,
		QCA81XX_PHY_DEBUG_AFE_DAC9_DP);

	ret = nss_phy_write_debug(nss_phydev,
		QCA81XX_PHY_DEBUG_AFE_DAC8_DP, 0);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_debug(nss_phydev,
		QCA81XX_PHY_DEBUG_AFE_DAC9_DP, 0);
	if (ret < 0)
		return ret;

	ret = nss_phy_c45_common_cdt_start(nss_phydev);
	if (ret < 0)
		return ret;

	/* Get cable status */
	cdt_ret = nss_phy_common_cdt_status_get(nss_phydev, mdi_pair, status,
		cable_len);

	/* recover the analog AFE DAC value */
	ret = nss_phy_write_debug(nss_phydev,
		QCA81XX_PHY_DEBUG_AFE_DAC8_DP, dac8_tmp);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_debug(nss_phydev,
		QCA81XX_PHY_DEBUG_AFE_DAC9_DP, dac9_tmp);
	if (ret < 0)
		return ret;

	if (cdt_ret < 0)
		return cdt_ret;

	return 0;
}

static int qca81xx_phy_mdix_set(struct nss_phy_device *nss_phydev,
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

	ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM, NSS_PHY_SPEC_CONTROL,
		NSS_PHY_MDIX_AUTO | QCA81XX_PHY_MMD31_AUTO_SOFT_RESET_EN,
		phy_data | QCA81XX_PHY_MMD31_AUTO_SOFT_RESET_EN);

	if (ret < 0)
		return ret;

	return qca81xx_phy_soft_reset(nss_phydev);
}

static int qca81xx_phy_stats_status_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	int ret;

	ret = nss_phy_c45_common_stats_status_set(nss_phydev, enable);
	if (ret < 0)
		return ret;

	return nss_phy_common_stats_status_set(nss_phydev, enable);
}

static int qca81xx_phy_stats_status_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	int ret;
	u32 enable0, enable1;

	ret = nss_phy_common_stats_status_get(nss_phydev, &enable0);
	if (ret < 0)
		return ret;
	ret = nss_phy_c45_common_stats_status_get(nss_phydev, &enable1);
	if (ret < 0)
		return ret;

	if (enable0 && enable1)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

static int qca81xx_phy_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_stats_info *cnt_info)
{
	int ret;

	if (nss_phydev_speed_get(nss_phydev) >= NSS_PHY_SPEED_2500)
		ret = nss_phy_c45_common_stats_get(nss_phydev, cnt_info);
	else
		ret = nss_phy_common_stats_get(nss_phydev, cnt_info);

	return ret;
}

static int qca81xx_phy_adjust_link_post(struct nss_phy_device *nss_phydev)
{
	atomic64_inc(&nss_phydev->adjust_link_post_count);
	return 0;
}

static int qca81xx_phy_clk_ppm_offset_get(struct nss_phy_device *nss_phydev,
	int *ppm)
{
	return nss_phy_c45_common_clk_ppm_offset_get(nss_phydev, ppm,
		QCA81XX_PPM_DIV_1G,
		QCA81XX_PPM_DIV_2_5G,
		QCA81XX_PPM_DIV_5G_10G);
}

int qca81xx_phy_ops_init(struct nss_phy_ops *ops)
{
	ops->hibernation_set = nss_phy_common_hibernation_set;
	ops->hibernation_get = nss_phy_common_hibernation_get;
	ops->function_reset = qca81xx_phy_function_reset;
	ops->eee_adv_set = nss_phy_c45_common_eee_adv_set;
	ops->eee_adv_get = nss_phy_c45_common_eee_adv_get;
	ops->eee_partner_adv_get = nss_phy_c45_common_eee_partner_adv_get;
	ops->eee_cap_get = nss_phy_c45_common_eee_cap_get;
	ops->eee_status_get = nss_phy_c45_common_eee_status_get;
	ops->ieee_8023az_set = nss_phy_c45_common_8023az_set;
	ops->ieee_8023az_get = nss_phy_c45_common_8023az_get;
	ops->local_loopback_set = qca81xx_phy_local_loopback_set;
	ops->local_loopback_get = qca81xx_phy_local_loopback_get;
	ops->remote_loopback_set = nss_phy_common_remote_loopback_set;
	ops->remote_loopback_get = nss_phy_common_remote_loopback_get;
	ops->cdt = qca81xx_phy_cdt;
	ops->wol_set = nss_phy_common_wol_set;
	ops->wol_get = nss_phy_common_wol_get;
	ops->magic_frame_set = nss_phy_common_magic_frame_set;
	ops->magic_frame_get = nss_phy_common_magic_frame_get;
	ops->mdix_set = qca81xx_phy_mdix_set;
	ops->mdix_get = nss_phy_c45_common_mdix_get;
	ops->mdix_status_get = nss_phy_c45_common_mdix_status_get;
	ops->stats_status_set = qca81xx_phy_stats_status_set;
	ops->stats_status_get = qca81xx_phy_stats_status_get;
	ops->stats_get = qca81xx_phy_stats_get;
	ops->intr_mask_set = nss_phy_c45_common_intr_mask_set;
	ops->intr_mask_get = nss_phy_c45_common_intr_mask_get;
	ops->intr_status_get = nss_phy_c45_common_intr_status_get;
	ops->led_ctrl_source_set = nss_phy_c45_common_led_ctrl_source_set;
	ops->led_ctrl_source_get = nss_phy_c45_common_led_ctrl_source_get;
	ops->adjust_link_post = qca81xx_phy_adjust_link_post;
	ops->fr_cfg_set = nss_phy_c45_common_fr_cfg_set;
	ops->fr_cfg_get = nss_phy_c45_common_fr_cfg_get;
	ops->fr_status_get = nss_phy_c45_common_fr_status_get;
	ops->fr_trigger = nss_phy_c45_common_fr_trigger;
	ops->pcs_status_get = nss_phy_c45_common_pcs_status_get;
	ops->link_training_completion_get = nss_phy_c45_common_link_training_completion_get;
	ops->an_fail_counter_reset = nss_phy_common_an_fail_counter_reset;
	ops->an_fail_counter_get = nss_phy_common_an_fail_counter_get;
	ops->flap_stats_get = nss_phy_common_flap_stats_get;
	ops->flap_stats_reset = nss_phy_common_flap_stats_reset;
	ops->mse_get = nss_phy_c45_common_mse_get;
	ops->clk_ppm_offset_get = qca81xx_phy_clk_ppm_offset_get;
	ops->ldpc_stats_get = nss_phy_c45_common_ldpc_stats_get;

	return 0;
}
