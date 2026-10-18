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
#include "qca803x_phy.h"
#include "nss_phy_common.h"

static int qca803x_phy_chip_config_get(struct nss_phy_device *nss_phydev,
	u32 cfg_sel, u32 *cfg_value)
{
	u16 phy_data;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_CHIP_CONFIGURATION);

	if (cfg_sel == QCA803X_CHIP_CFG_STAT)
		*cfg_value = (phy_data & QCA803X_PHY_CHIP_MODE_STAT) >> 4;
	else
		*cfg_value = phy_data & QCA803X_PHY_CHIP_MODE_CFG;

	return 0;
}

static int qca803x_phy_interface_set(struct nss_phy_device *nss_phydev,
	u32 interface)
{
	u16 phy_data = 0;

	switch (interface) {
	case NSS_PHY_INTERFACE_MODE_RGMII:
		phy_data |= QCA803X_PHY_RGMII_BASET;
		break;
	case NSS_PHY_INTERFACE_MODE_SGMII:
		phy_data |= QCA803X_PHY_SGMII_BASET;
		break;
	case NSS_PHY_INTERFACE_MODE_1000BASEX:
		phy_data |= QCA803X_PHY_BX1000_RGMII_50;
		break;
	case NSS_PHY_INTERFACE_MODE_100BASEX:
		phy_data |= QCA803X_PHY_FX100_RGMII_50;
		break;
	case NSS_PHY_INTERFACE_MODE_RGMII_AMDET:
		phy_data |= QCA803X_PHY_RGMII_AMDET;
		break;
	default:
		return -NSS_PHY_EOPNOTSUPP;
	}

	return nss_phy_modify(nss_phydev, NSS_PHY_CHIP_CONFIGURATION,
		QCA803X_PHY_CHIP_MODE_CFG, phy_data);
}

static int qca803x_phy_interface_get(struct nss_phy_device *nss_phydev,
	u32 *interface)
{
	u32 interface_value;
	int ret;

	ret = qca803x_phy_chip_config_get(nss_phydev, QCA803X_CHIP_CFG_SET,
		&interface_value);
	if (ret < 0)
		return ret;

	switch (interface_value) {
	case QCA803X_PHY_RGMII_BASET:
		*interface = NSS_PHY_INTERFACE_MODE_RGMII;
		break;
	case  QCA803X_PHY_SGMII_BASET:
		*interface = NSS_PHY_INTERFACE_MODE_SGMII;
		break;
	case QCA803X_PHY_BX1000_RGMII_50:
		*interface = NSS_PHY_INTERFACE_MODE_1000BASEX;
		break;
	case QCA803X_PHY_FX100_RGMII_50:
		*interface = NSS_PHY_INTERFACE_MODE_100BASEX;
		break;
	case QCA803X_PHY_RGMII_AMDET:
		*interface = NSS_PHY_INTERFACE_MODE_RGMII_AMDET;
		break;
	default:
		*interface = NSS_PHY_INTERFACE_MODE_MAX;
		break;
	}

	return 0;
}

static int qca803x_phy_cdt_start(struct nss_phy_device *nss_phydev,
	u32 mdi_pair)
{
	u16 status = 0, ii = 100;
	int ret = 0;

	ret = nss_phy_write(nss_phydev, NSS_PHY_CDT_CONTROL,
		QCA803X_PHY_RUN_CDT |
		((mdi_pair << 8) & QCA803X_PHY_CDT_PAIR_MASK));
	do {
		nss_phy_mdelay(30);
		status = nss_phy_read(nss_phydev, NSS_PHY_CDT_CONTROL);
	} while ((status & QCA803X_PHY_RUN_CDT) && (--ii));

	if (ii == 0)
		return -NSS_PHY_ETIMEOUT;

	return 0;
}

static enum nss_phy_cable_status
qca803x_phy_cdt_status_mapping(u16 phy_status)
{
	enum nss_phy_cable_status status = CABLE_INVALID;

	switch (phy_status) {
	case 3:
		status = CABLE_INVALID;
		break;
	case 2:
		status = CABLE_OPENED;
		break;
	case 1:
		status = CABLE_SHORT;
		break;
	case 0:
		status = CABLE_NORMAL;
		break;
	}
	return status;
}

static int qca803x_phy_cdt(struct nss_phy_device *nss_phydev, u32 mdi_pair,
	enum nss_phy_cable_status *cable_status, u32 *cable_len)
{
	u16 status;
	int ret;

	if (mdi_pair >= NSS_PHY_MDI_PAIR_NUM)
		return -NSS_PHY_EINVAL;

	ret = qca803x_phy_cdt_start(nss_phydev, mdi_pair);
	if (ret < 0)
		return ret;

	/* Get cable status */
	status = nss_phy_read(nss_phydev, QCA803X_PHY_CDT_STATUS);
	*cable_status = qca803x_phy_cdt_status_mapping((status >> 8) & 0x3);
	/* the actual cable length equals to CableDeltaTime * 0.824 */
	*cable_len = ((status & 0xff) * 824) / 1000;

	return 0;
}

int qca803x_phy_fixup(struct nss_phy_device *nss_phydev)
{
	int ret = 0;

	/* config the times that MSE is over threshold as max value */
	ret = nss_phy_modify_debug(nss_phydev,
		QCA803X_PHY_DEBUG_MSE_OVER_THRESH_TIMES,
		QCA803X_PHY_MSE_OVER_THRESH_TIMES_MAX,
		QCA803X_PHY_MSE_OVER_THRESH_TIMES_MAX);
	if (ret < 0)
		return ret;
	/* disable Extended next page s*/
	ret = nss_phy_modify(nss_phydev, NSS_PHY_AUTONEG_ADV,
		NSS_PHY_EXTENDED_NEXT_PAGE_EN, 0);
	if (ret < 0)
		return ret;

	return 0;
}

int qca803x_phy_ops_init(struct nss_phy_ops *ops)
{
	ops->hibernation_set = nss_phy_common_hibernation_set;
	ops->hibernation_get = nss_phy_common_hibernation_get;
	ops->powersave_set = nss_phy_common_powersave_set;
	ops->powersave_get = nss_phy_common_powersave_get;
	ops->function_reset = nss_phy_common_function_reset;
	ops->interface_set = qca803x_phy_interface_set;
	ops->interface_get = qca803x_phy_interface_get;
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
	ops->cdt = qca803x_phy_cdt;
	ops->wol_set = nss_phy_common_wol_set;
	ops->wol_get = nss_phy_common_wol_get;
	ops->magic_frame_set = nss_phy_common_magic_frame_set;
	ops->magic_frame_get = nss_phy_common_magic_frame_get;
	ops->intr_mask_set = nss_phy_common_intr_mask_set;
	ops->intr_mask_get = nss_phy_common_intr_mask_get;
	ops->intr_status_get = nss_phy_common_intr_status_get;
	ops->mdix_set = nss_phy_common_mdix_set;
	ops->mdix_get = nss_phy_common_mdix_get;
	ops->mdix_status_get = nss_phy_common_mdix_status_get;

	return 0;
}
