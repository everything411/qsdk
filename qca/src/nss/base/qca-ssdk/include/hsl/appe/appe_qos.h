/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _APPE_QOS_H_
#define _APPE_QOS_H_

sw_error_t
appe_qos_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value);

sw_error_t
appe_qos_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value);

#if defined(MPPE)
sw_error_t
mppe_qos_mapping_tbl_flow_policer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *policer_en,
		a_uint32_t *policer_index);

sw_error_t
mppe_qos_mapping_tbl_flow_policer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t policer_en,
		a_uint32_t policer_index);
#endif

#endif
