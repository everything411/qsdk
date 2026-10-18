/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _HTTPPE_QOS_API_H_
#define _HTTPPE_QOS_API_H_

#define L0_FLOW_MAP_TBL_MAX_ENTRY		300
#define L0_C_SP_CFG_TBL_MAX_ENTRY		512
#define L0_E_SP_CFG_TBL_MAX_ENTRY		512
#define L0_FLOW_PORT_MAP_TBL_MAX_ENTRY		300
#define L0_C_DRR_HEAD_TBL_MAX_ENTRY		160
#define L0_E_DRR_HEAD_TBL_MAX_ENTRY		160
#define L0_DRR_CREDIT_TBL_MAX_ENTRY		300
#define L0_C_DRR_LL_TBL_MAX_ENTRY		300
#define L0_C_DRR_REVERSE_LL_TBL_MAX_ENTRY	300
#define L0_E_DRR_LL_TBL_MAX_ENTRY		300
#define L0_E_DRR_REVERSE_LL_TBL_MAX_ENTRY	300
#define L0_SP_ENTRY_TBL_MAX_ENTRY		64
#define L0_ENS_Q_LL_TBL_MAX_ENTRY		300
#define L0_ENS_Q_HEAD_TBL_MAX_ENTRY		8
#define L0_ENS_Q_ENTRY_TBL_MAX_ENTRY		300
#define L0_FLOW_STATUS_TBL_MAX_ENTRY		300
#define L1_FLOW_MAP_TBL_MAX_ENTRY		64
#define L1_C_SP_CFG_TBL_MAX_ENTRY		64
#define L1_E_SP_CFG_TBL_MAX_ENTRY		64
#define L1_FLOW_PORT_MAP_TBL_MAX_ENTRY		64
#define L1_C_DRR_HEAD_TBL_MAX_ENTRY		36
#define L1_E_DRR_HEAD_TBL_MAX_ENTRY		36
#define L1_DRR_CREDIT_TBL_MAX_ENTRY		64
#define L1_C_DRR_LL_TBL_MAX_ENTRY		64
#define L1_C_DRR_REVERSE_LL_TBL_MAX_ENTRY	64
#define L1_E_DRR_LL_TBL_MAX_ENTRY		64
#define L1_E_DRR_REVERSE_LL_TBL_MAX_ENTRY	64
#define L1_A_FLOW_ENTRY_TBL_MAX_ENTRY		64
#define L1_B_FLOW_ENTRY_TBL_MAX_ENTRY		64
#define L1_SP_ENTRY_TBL_MAX_ENTRY		8

#define L1_ENS_Q_LL_TBL_MAX_ENTRY		64
#define L1_ENS_Q_HEAD_TBL_MAX_ENTRY		8
#define L1_ENS_Q_ENTRY_TBL_MAX_ENTRY		64
#define L1_FLOW_STATUS_TBL_MAX_ENTRY		64
#define L0_COMP_CFG_TBL_MAX_ENTRY		L0_COMP_CFG_TBL_NUM
#define L1_COMP_CFG_TBL_MAX_ENTRY		L1_COMP_CFG_TBL_NUM
#define PSCH_TDM_CFG_TBL_MAX_ENTRY		PSCH_TDM_CFG_TBL_NUM

sw_error_t
httppe_l0_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value);
sw_error_t
httppe_l0_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value);
sw_error_t
httppe_l0_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value);
sw_error_t
httppe_l0_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value);
sw_error_t
httppe_l1_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value);
sw_error_t
httppe_l1_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value);
sw_error_t
httppe_l1_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value);
sw_error_t
httppe_l1_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value);
sw_error_t
httppe_l0_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value);
sw_error_t
httppe_l0_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value);
sw_error_t
httppe_l1_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value);
sw_error_t
httppe_l1_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value);
sw_error_t
httppe_tdm_depth_cfg_get(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value);
sw_error_t
httppe_tdm_depth_cfg_set(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value);
sw_error_t
httppe_psch_tdm_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value);
sw_error_t
httppe_psch_tdm_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value);

sw_error_t
httppe_qos_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value);

sw_error_t
httppe_qos_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value);
#endif

