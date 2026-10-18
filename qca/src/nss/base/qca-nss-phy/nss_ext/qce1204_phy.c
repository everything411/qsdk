/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "nss_phy.h"
#include "qce1204_phy.h"
#include "nss_phy_c45_common.h"

static bool qce1024_phy_autoneg_check(struct nss_phy_device *nss_phydev)
{
	int  times = 500, ret = 0;

	ret = nss_phy_c45_common_autoneg_restart(nss_phydev);
	if (ret < 0)
		return false;
	while(times--) {
		ret = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_AUTONEG_STATUS);
		if (ret < 0)
			return false;
		if ((ret & NSS_PHY_MMD7_AUTONEG_STATUS_MASK) >= NSS_PHY_MMD7_AUTONEG_ACK_DETECT)
			return true;
		nss_phy_mdelay(10);
	}
	return false;
}

static int qce1204_phy_cld_cable_length_get(struct nss_phy_device *nss_phydev, u32 *cable_len)
{
	int status, ret;
	bool link_down;

	status = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CDT_STATUS);
	if (status < 0)
		return status;

	/*
	 * CLD is supported only when all pairs are in normal status.
	 * If CLD is not available, return success (0) to allow fallback to CDT.
	 */
	if (status != QCE1204_PHY_ALL_PAIRS_NORMAL)
		return 0;

	link_down = (nss_phydev_link_get(nss_phydev) == NSS_PHY_FALSE);

	if (link_down) {
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			QCE1204_PHY_MMD3_CLD_CTRL, QCE1204_PHY_CLD_FORCE_EN,
			QCE1204_PHY_CLD_FORCE_EN);
		if (ret < 0)
			return ret;
	}

	ret = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCE1204_PHY_MMD3_CLD_RESULT);
	if (ret >= 0)
		*cable_len = ret & QCE1204_PHY_CLD_CABLE_LENGTH;

	if (link_down) {
		int restore_ret;
		restore_ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			QCE1204_PHY_MMD3_CLD_CTRL, QCE1204_PHY_CLD_FORCE_EN, 0);
		/*
		 * Error priority logic:
		 * - If read succeeded but restore failed: return restore error
		 *   (restore failure is more critical as it leaves hardware in wrong state)
		 * - If both failed: return read error (first failure)
		 * - If both succeeded: continue to return read result
		 */
		if (restore_ret < 0 && ret >= 0)
			return restore_ret;
	}

	return (ret < 0) ? ret : 0;
}

static int qce1204_phy_cdt(struct nss_phy_device *nss_phydev, u32 mdi_pair,
	enum nss_phy_cable_status *status, u32 *cable_len)
{
	int ret, cdt_with_auotneg_en = 0;

	if (nss_phydev_link_get(nss_phydev) == NSS_PHY_FALSE) {
		/* run cdt between two autoneg pulse if link partner send autoneg pulse */
		if (qce1024_phy_autoneg_check(nss_phydev))
			cdt_with_auotneg_en = QCE1204_PHY_MMD7_CDT_WITH_AUTONEG;

		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD31_NUM,
			NSS_PHY_CDT_CONTROL, QCE1204_PHY_MMD7_CDT_WITH_AUTONEG,
			cdt_with_auotneg_en);
		if (ret < 0)
			return ret;
		ret = nss_phy_c45_common_cdt_start(nss_phydev);
		if (ret < 0)
			return ret;
	}

	/* When the PHY link is up, hardware will not execute CDT even if software requests it.
	 * However, CDT status can still be read from the corresponding registers.
	 */
	ret = nss_phy_common_cdt_status_get(nss_phydev, mdi_pair, status,
		cable_len);
	if (ret < 0)
		return ret;
	/* Try to get more accurate cable length using CLD if available */
	ret = qce1204_phy_cld_cable_length_get(nss_phydev, cable_len);

	return ret;
}

static int qce1204_phy_clk_ppm_offset_get(struct nss_phy_device *nss_phydev,
	int *ppm)
{
	return nss_phy_c45_common_clk_ppm_offset_get(nss_phydev, ppm,
		QCE1204_PPM_DIV_1G,
		QCE1204_PPM_DIV_2_5G,
		0 /* 5G/10G not supported */);
}

#define QCE1204_PHY_PORT_MAX	4

static const u32 qce1204_led_gpio[LED_SOURCE_MAX][QCE1204_PHY_PORT_MAX] = {
	[NSS_PHY_LED_SOURCE0] = {
		QCE1204_GPIO1, QCE1204_GPIO2, QCE1204_GPIO3, QCE1204_GPIO4
	},
	[NSS_PHY_LED_SOURCE1] = {
		QCE1204_GPIO9, QCE1204_GPIO15, QCE1204_GPIO16, QCE1204_GPIO17
	},
	[NSS_PHY_LED_SOURCE2] = {
		QCE1204_GPIO5, QCE1204_GPIO6, QCE1204_GPIO7, QCE1204_GPIO8
	},
};

static const u32 qce1204_led_func[LED_SOURCE_MAX][QCE1204_PHY_PORT_MAX] = {
	[NSS_PHY_LED_SOURCE0] = {
		QCE1204_GPIO_FUNC_P0_LED_0, QCE1204_GPIO_FUNC_P1_LED_0,
		QCE1204_GPIO_FUNC_P2_LED_0, QCE1204_GPIO_FUNC_P3_LED_0
	},
	[NSS_PHY_LED_SOURCE1] = {
		QCE1204_GPIO_FUNC_P0_LED_1, QCE1204_GPIO_FUNC_P1_LED_1,
		QCE1204_GPIO_FUNC_P2_LED_1, QCE1204_GPIO_FUNC_P3_LED_1
	},
	[NSS_PHY_LED_SOURCE2] = {
		QCE1204_GPIO_FUNC_P0_LED_2, QCE1204_GPIO_FUNC_P1_LED_2,
		QCE1204_GPIO_FUNC_P2_LED_2, QCE1204_GPIO_FUNC_P3_LED_2
	},
};

static int qce1204_phy_led_ctrl_source_pin_cfg(
	struct nss_phy_device *nss_phydev, u32 source_id)
{
	u32 phy_index, led_pin, led_func;

	phy_index = nss_phy_addr_get(nss_phydev) - nss_phy_share_addr_get(nss_phydev);
	if (source_id >= LED_SOURCE_MAX || phy_index >= QCE1204_PHY_PORT_MAX)
		return -NSS_PHY_EOPNOTSUPP;

	led_pin = qce1204_led_gpio[source_id][phy_index];
	led_func = qce1204_led_func[source_id][phy_index];

	return nss_phy_modify_soc(nss_phydev, TO_GPIO_REG(led_pin),
		QCE1204_GPIO_FUNC_SEL | QCE1204_GPIO_PULL | QCE1204_GPIO_DRV | QCE1204_GPIO_LED_MODE,
		/* PULL cleared to NO_PULL; LED output pins require no bias */
		(led_func << 2) | QCE1204_GPIO_DRV | QCE1204_GPIO_LED_MODE);
}

static int
qce1204_phy_led_ctrl_source_set(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	int ret = 0;

	ret = nss_phy_2500m_led_ctrl_source_set(nss_phydev, source_id,
		pattern);
	if (ret < 0)
		return ret;
	if (nss_phy_id_check(nss_phydev, QCE1204_PHY, QCA_PHY_EXACT_MASK)) {
		ret = qce1204_phy_led_ctrl_source_pin_cfg(nss_phydev,
			source_id);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int qce1204_phy_ops_init(struct nss_phy_ops *ops)
{
	ops->hibernation_set = nss_phy_common_hibernation_set;
	ops->hibernation_get = nss_phy_common_hibernation_get;
	ops->function_reset = nss_phy_c45_function_reset;
	ops->eee_adv_set = nss_phy_c45_common_eee_adv_set;
	ops->eee_adv_get = nss_phy_c45_common_eee_adv_get;
	ops->eee_partner_adv_get = nss_phy_c45_common_eee_partner_adv_get;
	ops->eee_cap_get = nss_phy_c45_common_eee_cap_get;
	ops->eee_status_get = nss_phy_c45_common_eee_status_get;
	ops->ieee_8023az_set = nss_phy_c45_common_8023az_set;
	ops->ieee_8023az_get = nss_phy_c45_common_8023az_get;
	ops->local_loopback_set = nss_phy_c45_common_pma_local_loopback_set;
	ops->local_loopback_get = nss_phy_c45_common_pma_local_loopback_get;
	ops->remote_loopback_set = nss_phy_common_remote_loopback_set;
	ops->remote_loopback_get = nss_phy_common_remote_loopback_get;
	ops->cdt = qce1204_phy_cdt;
	ops->wol_set = nss_phy_common_wol_set;
	ops->wol_get = nss_phy_common_wol_get;
	ops->magic_frame_set = nss_phy_common_magic_frame_set;
	ops->magic_frame_get = nss_phy_common_magic_frame_get;
	ops->mdix_set = nss_phy_c45_common_mdix_set;
	ops->mdix_get = nss_phy_c45_common_mdix_get;
	ops->mdix_status_get = nss_phy_c45_common_mdix_status_get;
	ops->stats_status_set = nss_phy_common_stats_status_set;
	ops->stats_status_get = nss_phy_common_stats_status_get;
	ops->stats_get = nss_phy_common_stats_get;
	ops->intr_mask_set = nss_phy_c45_common_intr_mask_set;
	ops->intr_mask_get = nss_phy_c45_common_intr_mask_get;
	ops->intr_status_get = nss_phy_c45_common_intr_status_get;
	ops->led_ctrl_source_set = qce1204_phy_led_ctrl_source_set;
	ops->led_ctrl_source_get = nss_phy_2500m_led_ctrl_source_get;
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
	ops->clk_ppm_offset_get = qce1204_phy_clk_ppm_offset_get;
	ops->ldpc_stats_get = nss_phy_c45_common_ldpc_stats_get;

	return 0;
}
