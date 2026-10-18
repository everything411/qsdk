/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _QCA803X_PHY_H_
#define _QCA803X_PHY_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

/*MII registers*/
#define QCA803X_PHY_CDT_STATUS		28

/*MII registers field*/
#define QCA803X_PHY_CHIP_MODE_CFG		0x000f
#define QCA803X_PHY_CHIP_MODE_STAT		0x00f0
#define QCA803X_PHY_RGMII_BASET		0
#define QCA803X_PHY_SGMII_BASET		1
#define QCA803X_PHY_BX1000_RGMII_50		2
#define QCA803X_PHY_FX100_RGMII_50		6
#define QCA803X_PHY_RGMII_AMDET		11
#define QCA803X_PHY_RUN_CDT		0x1
#define QCA803X_PHY_CDT_PAIR_MASK		0x0300

/* CLD registers */
#define QCA803X_PHY_MMD3_CLD_LEN	0x8006
#define QCA803X_PHY_CLD_LEN_MASK	0xff

/* DEBUG registers */
#define QCA803X_PHY_DEBUG_MSE_OVER_THRESH_TIMES		28
#define QCA803X_PHY_MSE_OVER_THRESH_TIMES_MAX		0x7000

enum qca803x_phy_cfg_type {
	QCA803X_CHIP_CFG_SET,
	QCA803X_CHIP_CFG_STAT
};

int qca803x_phy_fixup(struct nss_phy_device *nss_phydev);
int qca803x_phy_ops_init(struct nss_phy_ops *ops);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _QCA803X_PHY_H_ */
