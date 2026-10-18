/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_BM_API_H_
#define _HPPE_BM_API_H_

#define PORT_FC_MODE_MAX_ENTRY	PORT_FC_MODE_NUM

sw_error_t
hppe_port_fc_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_mode_u *value);

sw_error_t
hppe_port_fc_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_mode_u *value);

sw_error_t
hppe_port_fc_mode_fc_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_mode_fc_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_GROUP_ID_MAX_ENTRY	PORT_GROUP_ID_NUM

sw_error_t
hppe_port_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_group_id_u *value);

sw_error_t
hppe_port_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_group_id_u *value);

sw_error_t
hppe_port_group_id_port_shared_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_group_id_port_shared_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_CNT_MAX_ENTRY	PORT_CNT_NUM

sw_error_t
hppe_port_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_cnt_u *value);

sw_error_t
hppe_port_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_cnt_u *value);

sw_error_t
hppe_port_cnt_port_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_cnt_port_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_REACTED_CNT_MAX_ENTRY	PORT_REACTED_CNT_NUM

sw_error_t
hppe_port_reacted_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_reacted_cnt_u *value);

sw_error_t
hppe_port_reacted_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_reacted_cnt_u *value);

sw_error_t
hppe_port_reacted_cnt_port_reacted_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_reacted_cnt_port_reacted_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define SHARED_GROUP_CFG_MAX_ENTRY	SHARED_GROUP_CFG_NUM

sw_error_t
hppe_shared_group_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union shared_group_cfg_u *value);

sw_error_t
hppe_shared_group_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union shared_group_cfg_u *value);

sw_error_t
hppe_shared_group_cfg_shared_group_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_shared_group_cfg_shared_group_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_FC_CFG_MAX_ENTRY	PORT_FC_CFG_NUM

sw_error_t
hppe_port_fc_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_cfg_u *value);

sw_error_t
hppe_port_fc_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_cfg_u *value);

sw_error_t
hppe_port_fc_cfg_port_pre_alloc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_pre_alloc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_fc_cfg_port_react_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_react_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_fc_cfg_port_resume_floor_th_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_resume_floor_th_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_fc_cfg_port_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_fc_cfg_port_shared_ceiling_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_shared_ceiling_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_fc_cfg_port_shared_dynamic_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_shared_dynamic_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_fc_cfg_port_shared_weight_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_fc_cfg_port_shared_weight_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
