/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _SSDK_HTTPPE_H_
#define _SSDK_HTTPPE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t qca_httppe_hw_init(a_uint32_t dev_id);

sw_error_t ssdk_httppe_init_with_dsa(struct qca_phy_priv* priv);

sw_error_t qca_httppe_mdio_master_init(a_uint32_t dev_id);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SSDK_HTTPPE_H */

