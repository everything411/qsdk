/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_HPPE_UNIPHY_H_
#define _ADPT_HPPE_UNIPHY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#define UNIPHY_XPCS_TSL_TIMER         0xa
#define UNIPHY_XPCS_TLU_TIMER         0x3
#define UNIPHY_XPCS_TWL_TIMER         0x16
#define UNIPHY_XPCS_100US_TIMER       0xc8
#define UNIPHY_XPCS_TWR_TIMER         0x1c
#define UNIPHY_UQXGMII_MODE           0x5
#define UNIPHY_UDXGMII_MODE           0x3
#define UNIPHY_UQXGMII_AM_COUNT       0x6018
#define UNIPHY_SEL_RX_LOS0            0x1
#define UNIPHY_SEL_RX_LOS1            0x2
#define UNIPHY_SEL_RX_LOS2            0x4

union sr_mii_ctrl_u;
union vr_mii_an_intr_sts_u;
sw_error_t
adpt_hppe_uniphy_usxgmii_status_get(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union sr_mii_ctrl_u *sr_mii_ctrl);
sw_error_t
adpt_hppe_uniphy_usxgmii_status_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union sr_mii_ctrl_u *sr_mii_ctrl);
sw_error_t
adpt_hppe_uniphy_usxgmii_autoneg_status_get(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union vr_mii_an_intr_sts_u *vr_mii_an_intr_sts);
sw_error_t
adpt_hppe_uniphy_usxgmii_autoneg_status_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union vr_mii_an_intr_sts_u *vr_mii_an_intr_sts);
a_uint32_t
adpt_hppe_port_get_by_uniphy(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t channel);
a_bool_t
adpt_hppe_uniphy_usxgmii_port_check(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id);
void
__adpt_ppe_gcc_uniphy_software_reset(a_uint32_t dev_id, a_uint32_t uniphy_index);
sw_error_t
__adpt_hppe_uniphy_calibrate(a_uint32_t dev_id, a_uint32_t uniphy_index);
a_bool_t
adpt_hppe_uniphy_xpcs_eee_support(a_uint32_t dev_id, a_uint32_t uniphy_index);
void
__adpt_hppe_gcc_uniphy_xpcs_reset(a_uint32_t dev_id, a_uint32_t uniphy_index, a_bool_t enable);
void
__adpt_hppe_uniphy_mode_ctrl_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
				 a_uint32_t mode, a_uint32_t channel);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
