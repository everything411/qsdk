/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_queue_tx_counter_tbl_get(
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
hppe_queue_tx_counter_tbl_set(
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
hppe_flush_cfg_get(
		a_uint32_t dev_id,
		union flush_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + FLUSH_CFG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_flush_cfg_set(
		a_uint32_t dev_id,
		union flush_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + FLUSH_CFG_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_mirror_priority_ctrl_get(
		a_uint32_t dev_id,
		union in_mirror_priority_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + IN_MIRROR_PRIORITY_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_mirror_priority_ctrl_set(
		a_uint32_t dev_id,
		union in_mirror_priority_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + IN_MIRROR_PRIORITY_CTRL_ADDRESS,
				value->val);
}

sw_error_t
hppe_eg_mirror_priority_ctrl_get(
		a_uint32_t dev_id,
		union eg_mirror_priority_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + EG_MIRROR_PRIORITY_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
hppe_eg_mirror_priority_ctrl_set(
		a_uint32_t dev_id,
		union eg_mirror_priority_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + EG_MIRROR_PRIORITY_CTRL_ADDRESS,
				value->val);
}

sw_error_t
hppe_ucast_default_hash_get(
		a_uint32_t dev_id,
		union ucast_default_hash_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_DEFAULT_HASH_ADDRESS,
				&value->val);
}

sw_error_t
hppe_ucast_default_hash_set(
		a_uint32_t dev_id,
		union ucast_default_hash_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_DEFAULT_HASH_ADDRESS,
				value->val);
}

sw_error_t
hppe_mcast_priority_map0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map0_u *value)
{
	if (index >= MCAST_PRIORITY_MAP0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP0_ADDRESS + \
				index * MCAST_PRIORITY_MAP0_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map0_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP0_ADDRESS + \
				index * MCAST_PRIORITY_MAP0_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map1_u *value)
{
	if (index >= MCAST_PRIORITY_MAP1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP1_ADDRESS + \
				index * MCAST_PRIORITY_MAP1_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map1_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP1_ADDRESS + \
				index * MCAST_PRIORITY_MAP1_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map2_u *value)
{
	if (index >= MCAST_PRIORITY_MAP2_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP2_ADDRESS + \
				index * MCAST_PRIORITY_MAP2_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map2_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP2_ADDRESS + \
				index * MCAST_PRIORITY_MAP2_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map3_u *value)
{
	if (index >= MCAST_PRIORITY_MAP3_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP3_ADDRESS + \
				index * MCAST_PRIORITY_MAP3_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map3_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP3_ADDRESS + \
				index * MCAST_PRIORITY_MAP3_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map4_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map4_u *value)
{
	if (index >= MCAST_PRIORITY_MAP4_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP4_ADDRESS + \
				index * MCAST_PRIORITY_MAP4_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map4_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map4_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP4_ADDRESS + \
				index * MCAST_PRIORITY_MAP4_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map5_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map5_u *value)
{
	if (index >= MCAST_PRIORITY_MAP5_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP5_ADDRESS + \
				index * MCAST_PRIORITY_MAP5_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map5_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map5_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP5_ADDRESS + \
				index * MCAST_PRIORITY_MAP5_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map6_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map6_u *value)
{
	if (index >= MCAST_PRIORITY_MAP6_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP6_ADDRESS + \
				index * MCAST_PRIORITY_MAP6_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map6_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map6_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP6_ADDRESS + \
				index * MCAST_PRIORITY_MAP6_INC,
				value->val);
}

sw_error_t
hppe_mcast_priority_map7_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map7_u *value)
{
	if (index >= MCAST_PRIORITY_MAP7_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP7_ADDRESS + \
				index * MCAST_PRIORITY_MAP7_INC,
				&value->val);
}

sw_error_t
hppe_mcast_priority_map7_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map7_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP7_ADDRESS + \
				index * MCAST_PRIORITY_MAP7_INC,
				value->val);
}

sw_error_t
hppe_ucast_queue_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_tbl_u *value)
{
	if (index >= UCAST_QUEUE_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_MAP_TBL_ADDRESS + \
				index * UCAST_QUEUE_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_ucast_queue_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_MAP_TBL_ADDRESS + \
				index * UCAST_QUEUE_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_ucast_hash_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_hash_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_HASH_MAP_TBL_ADDRESS + \
				index * UCAST_HASH_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_ucast_hash_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_hash_map_tbl_u *value)
{
	if (index >= UCAST_HASH_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_HASH_MAP_TBL_ADDRESS + \
				index * UCAST_HASH_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_ucast_priority_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_priority_map_tbl_u *value)
{
	if (index >= UCAST_PRIORITY_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_PRIORITY_MAP_TBL_ADDRESS + \
				index * UCAST_PRIORITY_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_ucast_priority_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_priority_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_PRIORITY_MAP_TBL_ADDRESS + \
				index * UCAST_PRIORITY_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_mcast_queue_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_queue_map_tbl_u *value)
{
	if (index >= MCAST_QUEUE_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_QUEUE_MAP_TBL_ADDRESS + \
				index * MCAST_QUEUE_MAP_TBL_INC,
				&value->val);
}

sw_error_t
hppe_mcast_queue_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_queue_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_QUEUE_MAP_TBL_ADDRESS + \
				index * MCAST_QUEUE_MAP_TBL_INC,
				value->val);
}

sw_error_t
hppe_ac_uni_queue_cfg_tbl_get(
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
hppe_ac_uni_queue_cfg_tbl_set(
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
hppe_ac_mul_queue_cfg_tbl_get(
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
hppe_ac_mul_queue_cfg_tbl_set(
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
hppe_ac_grp_cfg_tbl_get(
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
hppe_ac_grp_cfg_tbl_set(
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
hppe_ac_uni_queue_cnt_tbl_get(
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
hppe_ac_uni_queue_cnt_tbl_set(
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
hppe_ac_mul_queue_cnt_tbl_get(
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
hppe_ac_mul_queue_cnt_tbl_set(
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
hppe_ac_uni_queue_drop_state_tbl_get(
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
hppe_ac_uni_queue_drop_state_tbl_set(
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
hppe_ac_mul_queue_drop_state_tbl_get(
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
hppe_ac_mul_queue_drop_state_tbl_set(
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
hppe_ac_grp_drop_state_tbl_get(
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
hppe_ac_grp_drop_state_tbl_set(
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
hppe_oq_enq_opr_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_enq_opr_tbl_u *value)
{
	if (index >= OQ_ENQ_OPR_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_ENQ_OPR_TBL_ADDRESS + \
				index * OQ_ENQ_OPR_TBL_INC,
				&value->val);
}

sw_error_t
hppe_oq_enq_opr_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_enq_opr_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_ENQ_OPR_TBL_ADDRESS + \
				index * OQ_ENQ_OPR_TBL_INC,
				value->val);
}

sw_error_t
hppe_oq_deq_opr_tbl_get(
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
hppe_oq_deq_opr_tbl_set(
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
hppe_uni_drop_cnt_tbl_get(
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
hppe_uni_drop_cnt_tbl_set(
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

sw_error_t
hppe_mul_p0_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p0_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P0_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P0_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p0_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p0_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p0_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P0_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P0_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p0_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p1_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p1_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P1_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P1_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p1_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p1_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p1_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P1_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P1_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p1_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p2_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p2_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P2_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P2_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p2_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p2_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p2_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P2_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P2_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p2_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p3_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p3_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P3_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P3_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p3_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p3_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p3_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P3_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P3_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p3_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p4_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p4_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P4_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P4_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p4_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p4_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p4_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P4_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P4_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p4_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p5_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p5_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P5_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P5_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p5_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p5_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p5_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P5_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P5_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p5_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p6_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p6_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P6_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P6_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p6_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p6_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p6_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P6_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P6_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p6_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p7_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p7_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P7_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P7_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p7_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_mul_p7_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p7_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P7_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P7_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p7_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_ucast_hash_map_tbl_hash_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ucast_hash_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ucast_hash_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.hash;
	return ret;
}

sw_error_t
hppe_ucast_hash_map_tbl_hash_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ucast_hash_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ucast_hash_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.hash = value;
	ret = hppe_ucast_hash_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union uni_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_uni_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.uni_drop_byte_1 << 32 | \
		reg_val.bf.uni_drop_byte_0;
	return ret;
}

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union uni_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_uni_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.uni_drop_byte_1 = value >> 32;
	reg_val.bf.uni_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_uni_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union uni_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_uni_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.uni_drop_pkt;
	return ret;
}

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union uni_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_uni_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.uni_drop_pkt = value;
	ret = hppe_uni_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p0_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p0_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p0_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p0_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p0_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p0_drop_pkt = value;
	ret = hppe_mul_p0_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p0_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p0_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p0_drop_byte_1 << 32 | \
		reg_val.bf.mul_p0_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p0_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p0_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p0_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p0_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p0_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p1_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p1_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p1_drop_byte_1 << 32 | \
		reg_val.bf.mul_p1_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p1_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p1_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p1_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p1_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p1_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p1_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p1_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p1_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p1_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p1_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p1_drop_pkt = value;
	ret = hppe_mul_p1_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p2_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p2_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p2_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p2_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p2_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p2_drop_pkt = value;
	ret = hppe_mul_p2_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p2_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p2_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p2_drop_byte_1 << 32 | \
		reg_val.bf.mul_p2_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p2_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p2_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p2_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p2_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p2_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p3_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p3_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p3_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p3_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p3_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p3_drop_pkt = value;
	ret = hppe_mul_p3_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p3_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p3_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p3_drop_byte_1 << 32 | \
		reg_val.bf.mul_p3_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p3_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p3_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p3_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p3_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p3_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p4_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p4_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p4_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p4_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p4_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p4_drop_pkt = value;
	ret = hppe_mul_p4_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p4_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p4_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p4_drop_byte_1 << 32 | \
		reg_val.bf.mul_p4_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p4_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p4_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p4_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p4_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p4_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p5_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p5_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p5_drop_byte_1 << 32 | \
		reg_val.bf.mul_p5_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p5_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p5_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p5_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p5_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p5_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p5_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p5_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p5_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p5_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p5_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p5_drop_pkt = value;
	ret = hppe_mul_p5_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p6_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p6_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p6_drop_byte_1 << 32 | \
		reg_val.bf.mul_p6_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p6_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p6_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p6_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p6_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p6_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p6_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p6_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p6_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p6_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p6_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p6_drop_pkt = value;
	ret = hppe_mul_p6_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p7_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p7_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p7_drop_pkt;
	return ret;
}

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p7_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p7_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p7_drop_pkt = value;
	ret = hppe_mul_p7_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p7_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p7_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p7_drop_byte_1 << 32 | \
		reg_val.bf.mul_p7_drop_byte_0;
	return ret;
}

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p7_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_mul_p7_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p7_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p7_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_mul_p7_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_queue_tx_counter_tbl_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union queue_tx_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_queue_tx_counter_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.tx_bytes_1 << 32 | \
		reg_val.bf.tx_bytes_0;
	return ret;
}

sw_error_t
hppe_queue_tx_counter_tbl_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union queue_tx_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_queue_tx_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_bytes_1 = value >> 32;
	reg_val.bf.tx_bytes_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_queue_tx_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_queue_tx_counter_tbl_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union queue_tx_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_queue_tx_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_packets;
	return ret;
}

sw_error_t
hppe_queue_tx_counter_tbl_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union queue_tx_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_queue_tx_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_packets = value;
	ret = hppe_queue_tx_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

