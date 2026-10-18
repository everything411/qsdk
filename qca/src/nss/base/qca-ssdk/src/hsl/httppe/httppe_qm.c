/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_mc_enq_ctrl_get(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + MC_ENQ_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
httppe_mc_enq_ctrl_set(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + MC_ENQ_CTRL_ADDRESS,
				value->val);
}

sw_error_t
httppe_port_vsi_enqueue_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vsi_enqueue_map_u *value)
{
	if (index >= PORT_VSI_ENQUEUE_MAP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_VSI_ENQUEUE_MAP_ADDRESS + \
				index * PORT_VSI_ENQUEUE_MAP_INC,
				&value->val);
}

sw_error_t
httppe_port_vsi_enqueue_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vsi_enqueue_map_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_VSI_ENQUEUE_MAP_ADDRESS + \
				index * PORT_VSI_ENQUEUE_MAP_INC,
				value->val);
}

sw_error_t
httppe_flush_cfg_get(
		a_uint32_t dev_id,
		union flush_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + FLUSH_CFG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_flush_cfg_set(
		a_uint32_t dev_id,
		union flush_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + FLUSH_CFG_ADDRESS,
				value->val);
}

sw_error_t
httppe_ac_uni_queue_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_UNI_QUEUE_CFG_TBL_ADDRESS + \
				index * AC_UNI_QUEUE_CFG_TBL_INC,
				value->val,
				sizeof(union ac_uni_queue_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_uni_queue_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_UNI_QUEUE_CFG_TBL_ADDRESS + \
				index * AC_UNI_QUEUE_CFG_TBL_INC,
				value->val,
				sizeof(union ac_uni_queue_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_mul_queue_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_MUL_QUEUE_CFG_TBL_ADDRESS + \
				index * AC_MUL_QUEUE_CFG_TBL_INC,
				value->val,
				sizeof(union ac_mul_queue_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_mul_queue_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_MUL_QUEUE_CFG_TBL_ADDRESS + \
				index * AC_MUL_QUEUE_CFG_TBL_INC,
				value->val,
				sizeof(union ac_mul_queue_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_grp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_GRP_CFG_TBL_ADDRESS + \
				index * AC_GRP_CFG_TBL_INC,
				value->val,
				sizeof(union ac_grp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_grp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_GRP_CFG_TBL_ADDRESS + \
				index * AC_GRP_CFG_TBL_INC,
				value->val,
				sizeof(union ac_grp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_uni_queue_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cnt_tbl_u *value)
{
	if (index >= AC_UNI_QUEUE_CNT_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_UNI_QUEUE_CNT_TBL_ADDRESS + \
				index * AC_UNI_QUEUE_CNT_TBL_INC,
				&value->val);
}

sw_error_t
httppe_ac_uni_queue_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cnt_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_UNI_QUEUE_CNT_TBL_ADDRESS + \
				index * AC_UNI_QUEUE_CNT_TBL_INC,
				value->val);
}

sw_error_t
httppe_ac_mul_queue_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cnt_tbl_u *value)
{
	if (index >= AC_MUL_QUEUE_CNT_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_MUL_QUEUE_CNT_TBL_ADDRESS + \
				index * AC_MUL_QUEUE_CNT_TBL_INC,
				&value->val);
}

sw_error_t
httppe_ac_mul_queue_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cnt_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_MUL_QUEUE_CNT_TBL_ADDRESS + \
				index * AC_MUL_QUEUE_CNT_TBL_INC,
				value->val);
}

sw_error_t
httppe_ac_grp_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_GRP_CNT_TBL_ADDRESS + \
				index * AC_GRP_CNT_TBL_INC,
				value->val,
				sizeof(union ac_grp_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_grp_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_GRP_CNT_TBL_ADDRESS + \
				index * AC_GRP_CNT_TBL_INC,
				value->val,
				sizeof(union ac_grp_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_drop_state_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_UNI_QUEUE_DROP_STATE_TBL_ADDRESS + \
				index * AC_UNI_QUEUE_DROP_STATE_TBL_INC,
				value->val,
				sizeof(union ac_uni_queue_drop_state_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_drop_state_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_UNI_QUEUE_DROP_STATE_TBL_ADDRESS + \
				index * AC_UNI_QUEUE_DROP_STATE_TBL_INC,
				value->val,
				sizeof(union ac_uni_queue_drop_state_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_drop_state_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_MUL_QUEUE_DROP_STATE_TBL_ADDRESS + \
				index * AC_MUL_QUEUE_DROP_STATE_TBL_INC,
				value->val,
				sizeof(union ac_mul_queue_drop_state_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_drop_state_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_MUL_QUEUE_DROP_STATE_TBL_ADDRESS + \
				index * AC_MUL_QUEUE_DROP_STATE_TBL_INC,
				value->val,
				sizeof(union ac_mul_queue_drop_state_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_grp_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_drop_state_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_GRP_DROP_STATE_TBL_ADDRESS + \
				index * AC_GRP_DROP_STATE_TBL_INC,
				value->val,
				sizeof(union ac_grp_drop_state_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ac_grp_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_drop_state_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + AC_GRP_DROP_STATE_TBL_ADDRESS + \
				index * AC_GRP_DROP_STATE_TBL_INC,
				value->val,
				sizeof(union ac_grp_drop_state_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_oq_deq_opr_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_deq_opr_tbl_u *value)
{
	if (index >= OQ_DEQ_OPR_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_DEQ_OPR_TBL_ADDRESS + \
				index * OQ_DEQ_OPR_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_deq_opr_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_deq_opr_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_DEQ_OPR_TBL_ADDRESS + \
				index * OQ_DEQ_OPR_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_head_uni_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_uni_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_HEAD_UNI_TBL_ADDRESS + \
				index * OQ_HEAD_UNI_TBL_INC,
				value->val,
				sizeof(union oq_head_uni_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_oq_head_uni_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_uni_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_HEAD_UNI_TBL_ADDRESS + \
				index * OQ_HEAD_UNI_TBL_INC,
				value->val,
				sizeof(union oq_head_uni_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_oq_head_mul_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_mul_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_HEAD_MUL_TBL_ADDRESS + \
				index * OQ_HEAD_MUL_TBL_INC,
				value->val,
				sizeof(union oq_head_mul_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_oq_head_mul_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_mul_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_HEAD_MUL_TBL_ADDRESS + \
				index * OQ_HEAD_MUL_TBL_INC,
				value->val,
				sizeof(union oq_head_mul_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_oq_ll_uni_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_uni_tbl_u *value)
{
	if (index >= OQ_LL_UNI_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_UNI_TBL_ADDRESS + \
				index * OQ_LL_UNI_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_uni_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_uni_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_UNI_TBL_ADDRESS + \
				index * OQ_LL_UNI_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_ll_mul_p0_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p0_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P0_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P0_TBL_ADDRESS + \
				index * OQ_LL_MUL_P0_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_mul_p0_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p0_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P0_TBL_ADDRESS + \
				index * OQ_LL_MUL_P0_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_ll_mul_p1_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p1_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P1_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P1_TBL_ADDRESS + \
				index * OQ_LL_MUL_P1_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_mul_p1_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p1_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P1_TBL_ADDRESS + \
				index * OQ_LL_MUL_P1_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_ll_mul_p2_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p2_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P2_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P2_TBL_ADDRESS + \
				index * OQ_LL_MUL_P2_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_mul_p2_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p2_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P2_TBL_ADDRESS + \
				index * OQ_LL_MUL_P2_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_ll_mul_p3_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p3_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P3_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P3_TBL_ADDRESS + \
				index * OQ_LL_MUL_P3_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_mul_p3_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p3_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P3_TBL_ADDRESS + \
				index * OQ_LL_MUL_P3_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_ll_mul_p4_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p4_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P4_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P4_TBL_ADDRESS + \
				index * OQ_LL_MUL_P4_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_mul_p4_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p4_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P4_TBL_ADDRESS + \
				index * OQ_LL_MUL_P4_TBL_INC,
				value->val);
}

sw_error_t
httppe_oq_ll_mul_p5_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p5_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P5_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P5_TBL_ADDRESS + \
				index * OQ_LL_MUL_P5_TBL_INC,
				&value->val);
}

sw_error_t
httppe_oq_ll_mul_p5_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p5_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P5_TBL_ADDRESS + \
				index * OQ_LL_MUL_P5_TBL_INC,
				value->val);
}

sw_error_t
httppe_queue_tx_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_tx_counter_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + QUEUE_TX_COUNTER_TBL_ADDRESS + \
				index * QUEUE_TX_COUNTER_TBL_INC,
				value->val,
				sizeof(union queue_tx_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_queue_tx_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_tx_counter_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + QUEUE_TX_COUNTER_TBL_ADDRESS + \
				index * QUEUE_TX_COUNTER_TBL_INC,
				value->val,
				sizeof(union queue_tx_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_uni_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uni_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UNI_DROP_CNT_TBL_ADDRESS + \
				index * UNI_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union uni_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_uni_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uni_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UNI_DROP_CNT_TBL_ADDRESS + \
				index * UNI_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union uni_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

