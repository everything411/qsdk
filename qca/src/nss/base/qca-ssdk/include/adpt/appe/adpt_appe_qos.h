/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_APPE_QOS_H_
#define _ADPT_APPE_QOS_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_appe_qos_port_pri_set(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_pri_precedence_t *pri);
sw_error_t
adpt_appe_qos_port_pri_get(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_pri_precedence_t *pri);
sw_error_t
adpt_appe_qos_cosmap_pcp_get(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t pcp,
					fal_qos_cosmap_t *cosmap);
sw_error_t
adpt_appe_qos_cosmap_pcp_set(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t pcp,
					fal_qos_cosmap_t *cosmap);
sw_error_t
adpt_appe_qos_cosmap_dscp_get(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t dscp,
					fal_qos_cosmap_t *cosmap);
sw_error_t
adpt_appe_qos_cosmap_flow_set(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint16_t flow,
					fal_qos_cosmap_t *cosmap);
sw_error_t
adpt_appe_qos_port_group_set(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_group_t *group);
sw_error_t
adpt_appe_qos_cosmap_dscp_set(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint8_t dscp,
					fal_qos_cosmap_t *cosmap);
sw_error_t
adpt_appe_qos_cosmap_flow_get(a_uint32_t dev_id, a_uint8_t group_id,
					a_uint16_t flow,
					fal_qos_cosmap_t *cosmap);
sw_error_t
adpt_appe_qos_port_group_get(a_uint32_t dev_id, fal_port_t port_id,
					fal_qos_group_t *group);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif

