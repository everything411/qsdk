/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_FDB_API_H_
#define _HTTPPE_FDB_API_H_


sw_error_t
httppe_l2_global_conf_get(
		a_uint32_t dev_id,
		union l2_global_conf_u *value);

sw_error_t
httppe_l2_global_conf_set(
		a_uint32_t dev_id,
		union l2_global_conf_u *value);

sw_error_t
httppe_l2_global_conf_age_ctrl_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_age_ctrl_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_age_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_age_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_bc_pvlan_isol_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_bc_pvlan_isol_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_dot1p_mapper_pcp_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_dot1p_mapper_pcp_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_dot1p_mapper_vlan_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_dot1p_mapper_vlan_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_failover_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_failover_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_fdb_hash_full_fwd_cmd_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_fdb_hash_full_fwd_cmd_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_fdb_hash_mode_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_fdb_hash_mode_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_fdb_hash_mode_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_fdb_hash_mode_1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_flow_cpy_escape_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_flow_cpy_escape_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_ipmc_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_ipmc_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_ipmc_hash_mode_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_ipmc_hash_mode_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_ipmc_hash_mode_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_ipmc_hash_mode_1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_ipmc_mismatch_act_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_ipmc_mismatch_act_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_lrn_ctrl_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_lrn_ctrl_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_lrn_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_lrn_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_mc_dmac_check_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_mc_dmac_check_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_mc_pvlan_isol_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_mc_pvlan_isol_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_mc_vlan_match_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_mc_vlan_match_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_l2_global_conf_service_code_loop_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_l2_global_conf_service_code_loop_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
