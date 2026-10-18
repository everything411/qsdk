/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_SHAPER_API_H_
#define _HPPE_SHAPER_API_H_


sw_error_t
hppe_ipg_pre_len_cfg_get(
		a_uint32_t dev_id,
		union ipg_pre_len_cfg_u *value);

sw_error_t
hppe_ipg_pre_len_cfg_set(
		a_uint32_t dev_id,
		union ipg_pre_len_cfg_u *value);

sw_error_t
hppe_ipg_pre_len_cfg_ipg_pre_len_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_ipg_pre_len_cfg_ipg_pre_len_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_shp_slot_cfg_l0_get(
		a_uint32_t dev_id,
		union shp_slot_cfg_l0_u *value);

sw_error_t
hppe_shp_slot_cfg_l0_set(
		a_uint32_t dev_id,
		union shp_slot_cfg_l0_u *value);

sw_error_t
hppe_shp_slot_cfg_l0_l0_shp_slot_time_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_shp_slot_cfg_l0_l0_shp_slot_time_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_shp_slot_cfg_l1_get(
		a_uint32_t dev_id,
		union shp_slot_cfg_l1_u *value);

sw_error_t
hppe_shp_slot_cfg_l1_set(
		a_uint32_t dev_id,
		union shp_slot_cfg_l1_u *value);

sw_error_t
hppe_shp_slot_cfg_l1_l1_shp_slot_time_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_shp_slot_cfg_l1_l1_shp_slot_time_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_shp_slot_cfg_port_get(
		a_uint32_t dev_id,
		union shp_slot_cfg_port_u *value);

sw_error_t
hppe_shp_slot_cfg_port_set(
		a_uint32_t dev_id,
		union shp_slot_cfg_port_u *value);

sw_error_t
hppe_shp_slot_cfg_port_port_shp_slot_time_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_shp_slot_cfg_port_port_shp_slot_time_set(
		a_uint32_t dev_id,
		unsigned int value);

#define L0_SHP_CREDIT_TBL_MAX_ENTRY	L0_SHP_CREDIT_TBL_NUM

sw_error_t
hppe_l0_shp_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_credit_tbl_u *value);

sw_error_t
hppe_l0_shp_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_credit_tbl_u *value);

sw_error_t
hppe_l0_shp_credit_tbl_c_shaper_credit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_credit_tbl_c_shaper_credit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_credit_tbl_c_shaper_credit_neg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_credit_tbl_c_shaper_credit_neg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_credit_tbl_e_shaper_credit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_credit_tbl_e_shaper_credit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_credit_tbl_e_shaper_credit_neg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_credit_tbl_e_shaper_credit_neg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L0_SHP_CFG_TBL_MAX_ENTRY	L0_SHP_CFG_TBL_NUM

sw_error_t
hppe_l0_shp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_cfg_tbl_u *value);

sw_error_t
hppe_l0_shp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_cfg_tbl_u *value);

sw_error_t
hppe_l0_shp_cfg_tbl_c_shaper_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_c_shaper_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_cbs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_cbs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_cf_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_cf_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_cir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_cir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_cir_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_cir_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_e_shaper_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_e_shaper_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_ebs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_ebs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_eir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_eir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_eir_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_eir_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_grp_cf_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_grp_cf_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_grp_end_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_grp_end_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_meter_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_meter_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_shp_refresh_nxt_ptr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_shp_refresh_nxt_ptr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_shp_cfg_tbl_token_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_shp_cfg_tbl_token_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L0_COMP_CFG_TBL_MAX_ENTRY	L0_COMP_CFG_TBL_NUM

sw_error_t
hppe_l0_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value);

sw_error_t
hppe_l0_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value);

sw_error_t
hppe_l0_comp_cfg_tbl_drr_meter_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_comp_cfg_tbl_drr_meter_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_comp_cfg_tbl_shaper_meter_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_comp_cfg_tbl_shaper_meter_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L1_SHP_CREDIT_TBL_MAX_ENTRY	L1_SHP_CREDIT_TBL_NUM

sw_error_t
hppe_l1_shp_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_credit_tbl_u *value);

sw_error_t
hppe_l1_shp_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_credit_tbl_u *value);

sw_error_t
hppe_l1_shp_credit_tbl_c_shaper_credit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_credit_tbl_c_shaper_credit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_credit_tbl_c_shaper_credit_neg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_credit_tbl_c_shaper_credit_neg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_credit_tbl_e_shaper_credit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_credit_tbl_e_shaper_credit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_credit_tbl_e_shaper_credit_neg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_credit_tbl_e_shaper_credit_neg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L1_SHP_CFG_TBL_MAX_ENTRY	L1_SHP_CFG_TBL_NUM

sw_error_t
hppe_l1_shp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_cfg_tbl_u *value);

sw_error_t
hppe_l1_shp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_cfg_tbl_u *value);

sw_error_t
hppe_l1_shp_cfg_tbl_c_shaper_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_c_shaper_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_cbs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_cbs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_cf_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_cf_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_cir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_cir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_cir_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_cir_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_e_shaper_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_e_shaper_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_ebs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_ebs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_eir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_eir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_eir_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_eir_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_grp_cf_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_grp_cf_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_grp_end_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_grp_end_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_meter_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_meter_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_shp_refresh_nxt_ptr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_shp_refresh_nxt_ptr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_shp_cfg_tbl_token_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_shp_cfg_tbl_token_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L1_COMP_CFG_TBL_MAX_ENTRY	L1_COMP_CFG_TBL_NUM

sw_error_t
hppe_l1_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value);

sw_error_t
hppe_l1_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value);

sw_error_t
hppe_l1_comp_cfg_tbl_drr_meter_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_comp_cfg_tbl_drr_meter_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_comp_cfg_tbl_shaper_meter_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_comp_cfg_tbl_shaper_meter_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PSCH_SHP_SIGN_TBL_MAX_ENTRY	PSCH_SHP_SIGN_TBL_NUM

sw_error_t
hppe_psch_shp_sign_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_sign_tbl_u *value);

sw_error_t
hppe_psch_shp_sign_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_sign_tbl_u *value);

sw_error_t
hppe_psch_shp_sign_tbl_shaper_credit_neg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_sign_tbl_shaper_credit_neg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PSCH_SHP_CREDIT_TBL_MAX_ENTRY	PSCH_SHP_CREDIT_TBL_NUM

sw_error_t
hppe_psch_shp_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_credit_tbl_u *value);

sw_error_t
hppe_psch_shp_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_credit_tbl_u *value);

sw_error_t
hppe_psch_shp_credit_tbl_shaper_credit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_credit_tbl_shaper_credit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PSCH_SHP_CFG_TBL_MAX_ENTRY	PSCH_SHP_CFG_TBL_NUM

sw_error_t
hppe_psch_shp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_cfg_tbl_u *value);

sw_error_t
hppe_psch_shp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_cfg_tbl_u *value);

sw_error_t
hppe_psch_shp_cfg_tbl_cbs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_cfg_tbl_cbs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_shp_cfg_tbl_cir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_cfg_tbl_cir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_shp_cfg_tbl_meter_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_cfg_tbl_meter_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_shp_cfg_tbl_shaper_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_cfg_tbl_shaper_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_shp_cfg_tbl_token_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_shp_cfg_tbl_token_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PSCH_COMP_CFG_TBL_MAX_ENTRY	PSCH_COMP_CFG_TBL_NUM

sw_error_t
hppe_psch_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_comp_cfg_tbl_u *value);

sw_error_t
hppe_psch_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_comp_cfg_tbl_u *value);

sw_error_t
hppe_psch_comp_cfg_tbl_shaper_meter_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_comp_cfg_tbl_shaper_meter_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
