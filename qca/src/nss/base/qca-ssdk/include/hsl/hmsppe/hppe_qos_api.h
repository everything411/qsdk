/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_QOS_API_H_
#define _HPPE_QOS_API_H_


sw_error_t
hppe_tdm_depth_cfg_get(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value);

sw_error_t
hppe_tdm_depth_cfg_set(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value);

sw_error_t
hppe_tdm_depth_cfg_tdm_depth_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_tdm_depth_cfg_tdm_depth_set(
		a_uint32_t dev_id,
		unsigned int value);

#define L0_FLOW_MAP_TBL_MAX_ENTRY	L0_FLOW_MAP_TBL_NUM

sw_error_t
hppe_l0_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value);

sw_error_t
hppe_l0_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value);

sw_error_t
hppe_l0_flow_map_tbl_c_drr_credit_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_c_drr_credit_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_c_drr_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_c_drr_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_c_drr_wt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_c_drr_wt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_c_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_c_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_e_drr_credit_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_e_drr_credit_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_e_drr_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_e_drr_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_e_drr_wt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_e_drr_wt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_e_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_e_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l0_flow_map_tbl_sp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_map_tbl_sp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L0_FLOW_PORT_MAP_TBL_MAX_ENTRY	L0_FLOW_PORT_MAP_TBL_NUM

sw_error_t
hppe_l0_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value);

sw_error_t
hppe_l0_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value);

sw_error_t
hppe_l0_flow_port_map_tbl_port_num_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l0_flow_port_map_tbl_port_num_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define RFC_BLOCK_TBL_MAX_ENTRY	RFC_BLOCK_TBL_NUM

sw_error_t
hppe_rfc_block_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfc_block_tbl_u *value);

#define RFC_STATUS_TBL_MAX_ENTRY	RFC_STATUS_TBL_NUM

sw_error_t
hppe_rfc_status_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfc_status_tbl_u *value);

#define DEQ_DIS_TBL_MAX_ENTRY	DEQ_DIS_TBL_NUM

sw_error_t
hppe_deq_dis_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union deq_dis_tbl_u *value);

sw_error_t
hppe_deq_dis_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union deq_dis_tbl_u *value);

sw_error_t
hppe_deq_dis_tbl_deq_dis_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_deq_dis_tbl_deq_dis_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define RING_Q_MAP_TBL_MAX_ENTRY	RING_Q_MAP_TBL_NUM

sw_error_t
hppe_ring_q_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ring_q_map_tbl_u *value);

sw_error_t
hppe_ring_q_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ring_q_map_tbl_u *value);

sw_error_t
hppe_ring_q_map_tbl_queue_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_ring_q_map_tbl_queue_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

#define L1_FLOW_MAP_TBL_MAX_ENTRY	L1_FLOW_MAP_TBL_NUM

sw_error_t
hppe_l1_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value);

sw_error_t
hppe_l1_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value);

sw_error_t
hppe_l1_flow_map_tbl_c_drr_credit_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_c_drr_credit_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_c_drr_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_c_drr_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_c_drr_wt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_c_drr_wt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_c_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_c_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_e_drr_credit_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_e_drr_credit_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_e_drr_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_e_drr_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_e_drr_wt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_e_drr_wt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_e_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_e_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l1_flow_map_tbl_sp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_map_tbl_sp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L1_FLOW_PORT_MAP_TBL_MAX_ENTRY	L1_FLOW_PORT_MAP_TBL_NUM

sw_error_t
hppe_l1_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value);

sw_error_t
hppe_l1_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value);

sw_error_t
hppe_l1_flow_port_map_tbl_port_num_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l1_flow_port_map_tbl_port_num_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PSCH_TDM_CFG_TBL_MAX_ENTRY	PSCH_TDM_CFG_TBL_NUM

sw_error_t
hppe_psch_tdm_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_des_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_des_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_tdm_cfg_tbl_des_sec_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_des_sec_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_tdm_cfg_tbl_des_sec_port_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_des_sec_port_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_tdm_cfg_tbl_ens_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_ens_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_psch_tdm_cfg_tbl_ens_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_psch_tdm_cfg_tbl_ens_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
