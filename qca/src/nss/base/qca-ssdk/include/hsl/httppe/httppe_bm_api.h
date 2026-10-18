/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_BM_API_H_
#define _HTTPPE_BM_API_H_

#define PORT_GROUP_ID_MAX_ENTRY	PORT_GROUP_ID_NUM

sw_error_t
httppe_port_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_group_id_u *value);

sw_error_t
httppe_port_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_group_id_u *value);

#define PORT_CNT_MAX_ENTRY	PORT_CNT_NUM

sw_error_t
httppe_port_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_cnt_u *value);

sw_error_t
httppe_port_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_cnt_u *value);

#define PORT_REACTED_CNT_MAX_ENTRY	PORT_REACTED_CNT_NUM

sw_error_t
httppe_port_reacted_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_reacted_cnt_u *value);

sw_error_t
httppe_port_reacted_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_reacted_cnt_u *value);

#define SHARED_GROUP_CFG_MAX_ENTRY	SHARED_GROUP_CFG_NUM

sw_error_t
httppe_shared_group_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union shared_group_cfg_u *value);

sw_error_t
httppe_shared_group_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union shared_group_cfg_u *value);

#define PORT_FC_CFG_MAX_ENTRY	PORT_FC_CFG_NUM

sw_error_t
httppe_port_fc_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_cfg_u *value);

sw_error_t
httppe_port_fc_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_cfg_u *value);

sw_error_t
httppe_port_fc_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_status_u *value);

sw_error_t
httppe_port_fc_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_status_u *value);

sw_error_t
httppe_drop_stat_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value);

#endif
