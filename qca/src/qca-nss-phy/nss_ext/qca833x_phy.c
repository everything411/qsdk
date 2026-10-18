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
#include "nss_phy.h"
#include "qca833x_phy.h"
#include "nss_phy_common.h"

static int qca833x_phy_cdt(struct nss_phy_device *nss_phydev,
	u32 mdi_pair, enum nss_phy_cable_status *cable_status, u32 *cable_len)
{
	u16 status = 0, cable_delta_time = 0, org_debug_value = 0;
	u32 pair_c_len = 0, pair_c_status = 0, ii = 100,
          link_st = NSS_PHY_FALSE;
	int ret = 0;

	/* disable clock gating */
	org_debug_value = nss_phy_read_debug(nss_phydev,
		QCA833X_PHY_LOW_POWER_CONTROL);
	ret = nss_phy_write_debug(nss_phydev, QCA833X_PHY_LOW_POWER_CONTROL, 0);
	if (ret < 0)
		return ret;
	ret = nss_phy_common_cdt_start(nss_phydev);
	if (ret < 0)
		return ret;

	/* Get cable status */
	status = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CDT_STATUS);
	/* Workaround for cable length less than 20M */
	pair_c_status = (status >> 4) & 0x3;
	/* Get Cable Length value */
	cable_delta_time = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		NSS_PHY_MMD3_CDT_PAIR2);
	/* the actual cable length equals to CableDeltaTime * 0.824 */
	pair_c_len = (cable_delta_time * 824) / 1000;
	if ((pair_c_status == 1) && (pair_c_len <= 20)) {
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			QCA833X_PHY_CLD16,
			QCA833X_PHY_MMD3_BP_AUTO_VCT, 0);
		if (ret < 0)
			return ret;
		ret = nss_phy_common_soft_reset(nss_phydev);
		if (ret < 0)
			return ret;
		ret = nss_phy_common_reset_done(nss_phydev);
		if (ret < 0)
			return ret;
		do {
			link_st = nss_phydev_link_get(nss_phydev);
			nss_phy_mdelay(100);
		} while ((link_st == false) && (--ii));

		ret = nss_phy_common_cdt_start(nss_phydev);
		if (ret < 0)
			return ret;
	}
	ret = nss_phy_common_cdt_status_get(nss_phydev, mdi_pair, cable_status,
		cable_len);
	if (ret < 0)
		return ret;

	/*restore debug port value*/
	ret = nss_phy_write_debug(nss_phydev, QCA833X_PHY_LOW_POWER_CONTROL,
		org_debug_value);

	return ret;
}

int qca833x_phy_ops_init(struct nss_phy_ops *ops)
{
	ops->hibernation_set = nss_phy_common_hibernation_set;
	ops->hibernation_get = nss_phy_common_hibernation_get;
	ops->powersave_set = nss_phy_common_powersave_set;
	ops->powersave_get = nss_phy_common_powersave_get;
	ops->function_reset = nss_phy_common_function_reset;
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
	ops->cdt = qca833x_phy_cdt;
	ops->intr_mask_set = nss_phy_common_intr_mask_set;
	ops->intr_mask_get = nss_phy_common_intr_mask_get;
	ops->intr_status_get = nss_phy_common_intr_status_get;
	ops->mdix_set = nss_phy_common_mdix_set;
	ops->mdix_get = nss_phy_common_mdix_get;
	ops->mdix_status_get = nss_phy_common_mdix_status_get;

	return 0;
}
