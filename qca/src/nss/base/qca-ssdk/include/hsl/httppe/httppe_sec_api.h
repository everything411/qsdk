/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_SEC_API_H_
#define _HTTPPE_SEC_API_H_


sw_error_t
httppe_l3_exception_parsing_ctrl_reg_get(
		a_uint32_t dev_id,
		union l3_exception_parsing_ctrl_reg_u *value);

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_set(
		a_uint32_t dev_id,
		union l3_exception_parsing_ctrl_reg_u *value);

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_small_hop_limit_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_small_hop_limit_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_small_ttl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_small_ttl_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_0_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_0_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags0_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags0_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags1_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_tcp_flags1_mask_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_1_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_1_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags2_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags2_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags2_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags2_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags3_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags3_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags3_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_tcp_flags3_mask_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_2_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_2_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags4_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags4_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags4_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags4_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags5_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags5_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags5_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_tcp_flags5_mask_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_3_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_3_reg_u *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags6_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags6_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags6_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags6_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags7_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags7_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags7_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_tcp_flags7_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

#define L3_EXCEPTION_CMD_MAX_ENTRY	L3_EXCEPTION_CMD_NUM

sw_error_t
httppe_l3_exception_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exception_cmd_u *value);

sw_error_t
httppe_l3_exception_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exception_cmd_u *value);

sw_error_t
httppe_l3_exception_cmd_de_acce_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_l3_exception_cmd_de_acce_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_l3_exception_cmd_l3_excep_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_l3_exception_cmd_l3_excep_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L3_EXP_L2_ONLY_CTRL_MAX_ENTRY	L3_EXP_L2_ONLY_CTRL_NUM

sw_error_t
httppe_l3_exp_l2_only_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exp_l2_only_ctrl_u *value);

sw_error_t
httppe_l3_exp_l2_only_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exp_l2_only_ctrl_u *value);

sw_error_t
httppe_l3_exp_l2_only_ctrl_excep_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_l3_exp_l2_only_ctrl_excep_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
