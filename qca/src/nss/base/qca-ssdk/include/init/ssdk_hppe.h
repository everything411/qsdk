/*
 * Copyright (c) 2017, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _SSDK_HPPE_H_
#define _SSDK_HPPE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "init/ssdk_init.h"

sw_error_t qca_hppe_vsi_hw_init(a_uint32_t dev_id);
sw_error_t qca_hppe_portvlan_hw_init(a_uint32_t dev_id);
sw_error_t qca_hppe_flow_hw_init(a_uint32_t dev_id);
sw_error_t qca_hppe_fdb_hw_init(a_uint32_t dev_id);
sw_error_t qca_hppe_ctlpkt_hw_init(a_uint32_t dev_id);
sw_error_t qca_hppe_acl_byp_intf_mac_learn(a_uint32_t dev_id);
sw_error_t qca_hppe_acl_remark_ptp_servcode(a_uint32_t dev_id);
sw_error_t qca_hppe_interface_mode_init(a_uint32_t dev_id);
sw_error_t
qca_hppe_qos_scheduler_hw_init(a_uint32_t dev_id);
sw_error_t
qca_hppe_bm_hw_init(a_uint32_t dev_id);
sw_error_t
qca_hppe_qm_hw_init(a_uint32_t dev_id);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _SSDK_HPPE_H */

