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
