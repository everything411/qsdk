/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _QCA833X_PHY_H_
#define _QCA833X_PHY_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */
/*debug registers*/
#define QCA833X_PHY_LOW_POWER_CONTROL		0x3f

/*mmd registers fields*/
#define QCA833X_PHY_MMD3_BP_AUTO_VCT		0x8000

int qca833x_phy_ops_init(struct nss_phy_ops *ops);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _QCA833X_PHY_H_ */
