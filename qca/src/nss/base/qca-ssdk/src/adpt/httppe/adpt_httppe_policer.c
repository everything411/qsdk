/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "hsl_htt_reg.h"
#include "adpt.h"
#include "adpt_appe_policer.h"
#include "adpt_httppe_policer.h"

sw_error_t adpt_httppe_acl_meter_cfg_read_ir(a_uint32_t dev_id, a_uint32_t index,
			__adpt_ppe_acl_meter_cfg_ir_t *ir)
{
	union in_acl_meter_cfg_tbl_u reg;
	sw_error_t rv = SW_OK;

	memset(&reg, 0, sizeof(reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ir);

	/* Caller (upper layer) already validated index range */
	rv = httppe_in_acl_meter_cfg_tbl_get(dev_id, index, &reg);
	SW_RTN_ON_ERROR(rv);

	/* refresh join */
	ir->cir_refresh = (a_uint32_t)reg.bf.cir_1 <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_OFFSET);
	ir->cir_refresh |= reg.bf.cir_0;

	ir->eir_refresh = (a_uint32_t)reg.bf.eir_1 <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_EIR_OFFSET);
	ir->eir_refresh |= reg.bf.eir_0;

	/* buckets */
	ir->cbs_bucket  = reg.bf.cbs;
	ir->ebs_bucket  = reg.bf.ebs;

	/* MEF10.3 max refresh join */
	ir->cir_max_refresh = (a_uint32_t)reg.bf.cir_max_1 <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_MAX_OFFSET);
	ir->cir_max_refresh |= reg.bf.cir_max_0;

	ir->eir_max_refresh = reg.bf.eir_max;

	/* flags / modes */
	ir->meter_en   = reg.bf.meter_en;
	ir->color_mode = reg.bf.color_mode;
	ir->couple_en  = reg.bf.coupling_flag;
	ir->meter_mode = reg.bf.meter_mode;
	ir->meter_unit = reg.bf.meter_unit;
	ir->token_unit = reg.bf.token_unit;

	/* group fields */
	ir->grp_end = reg.bf.grp_end;
	ir->grp_cf  = reg.bf.grp_cf;
	ir->nxt_ptr = (a_uint32_t)reg.bf.nxt_ptr_1 <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_NXT_PTR_OFFSET);
	ir->nxt_ptr |= reg.bf.nxt_ptr_0;

	/* exceed actions */
	ir->exceed_chg_pri_cmd = reg.bf.exceed_chg_pri_cmd;
	ir->exceed_chg_dp_cmd  = reg.bf.exceed_chg_dp_cmd;
	ir->exceed_chg_pcp_cmd = reg.bf.exceed_chg_pcp_cmd;
	ir->exceed_chg_dei_cmd = reg.bf.exceed_chg_dei_cmd;
	ir->exceed_pri         = reg.bf.exceed_pri;
	ir->exceed_dp          = reg.bf.exceed_dp;
	ir->exceed_pcp         = reg.bf.exceed_pcp;
	ir->exceed_dei         = reg.bf.exceed_dei;

	/* violate actions */
	ir->violate_cmd         = reg.bf.violate_cmd;
	ir->violate_chg_pri_cmd = reg.bf.violate_chg_pri_cmd;
	ir->violate_chg_dp_cmd  = reg.bf.violate_chg_dp_cmd;
	ir->violate_chg_pcp_cmd = reg.bf.violate_chg_pcp_cmd;
	ir->violate_chg_dei_cmd = reg.bf.violate_chg_dei_cmd;

	ir->violate_pri = (a_uint32_t)reg.bf.violate_pri_1 <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_VIOLATE_PRI_OFFSET);
	ir->violate_pri |= reg.bf.violate_pri_0;

	ir->violate_dp  = reg.bf.violate_dp;
	ir->violate_pcp = reg.bf.violate_pcp;
	ir->violate_dei = reg.bf.violate_dei;

	/* DSCP / remap */
	ir->exceed_chg_dscp_cmd = reg.bf.exceed_chg_dscp_cmd;
	ir->exceed_dscp         = reg.bf.exceed_dscp;
	ir->violate_chg_dscp_cmd= reg.bf.violate_chg_dscp_cmd;
	ir->violate_dscp        = reg.bf.violate_dscp;
	ir->exceed_remap_cmd    = reg.bf.exceed_remap_cmd;
	ir->violate_remap_cmd   = reg.bf.violate_remap_cmd;

	ir->length_mod = reg.bf.length_mod;

	return SW_OK;
}

sw_error_t adpt_httppe_acl_meter_cfg_write_ir(a_uint32_t dev_id, a_uint32_t index,
			const __adpt_ppe_acl_meter_cfg_ir_t *ir)
{
	union in_acl_meter_cfg_tbl_u reg;
	sw_error_t rv = SW_OK;

	memset(&reg, 0, sizeof(reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ir);

	/* Caller (upper layer) already validated index range */
	rv = httppe_in_acl_meter_cfg_tbl_get(dev_id, index, &reg);
	SW_RTN_ON_ERROR(rv);

	/* flags / modes */
	reg.bf.meter_en      = ir->meter_en;
	reg.bf.color_mode    = ir->color_mode;
	reg.bf.coupling_flag = ir->couple_en;
	reg.bf.meter_mode    = ir->meter_mode;
	reg.bf.token_unit    = ir->token_unit;
	reg.bf.meter_unit    = ir->meter_unit;

	/* buckets */
	reg.bf.cbs = ir->cbs_bucket;
	reg.bf.ebs = ir->ebs_bucket;

	/* refresh split */
	reg.bf.cir_0 = ir->cir_refresh &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_OFFSET)) - 1U);
	reg.bf.cir_1 = ir->cir_refresh >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_OFFSET);

	reg.bf.eir_0 = ir->eir_refresh &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_EIR_OFFSET)) - 1U);
	reg.bf.eir_1 = ir->eir_refresh >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_EIR_OFFSET);

	/* exceed actions */
	reg.bf.exceed_chg_pri_cmd = ir->exceed_chg_pri_cmd;
	reg.bf.exceed_chg_dp_cmd  = ir->exceed_chg_dp_cmd;
	reg.bf.exceed_chg_pcp_cmd = ir->exceed_chg_pcp_cmd;
	reg.bf.exceed_chg_dei_cmd = ir->exceed_chg_dei_cmd;
	reg.bf.exceed_pri         = ir->exceed_pri;
	reg.bf.exceed_dp          = ir->exceed_dp;
	reg.bf.exceed_pcp         = ir->exceed_pcp;
	reg.bf.exceed_dei         = ir->exceed_dei;

	/* violate actions */
	reg.bf.violate_cmd         = ir->violate_cmd;
	reg.bf.violate_chg_pri_cmd = ir->violate_chg_pri_cmd;
	reg.bf.violate_chg_dp_cmd  = ir->violate_chg_dp_cmd;
	reg.bf.violate_chg_pcp_cmd = ir->violate_chg_pcp_cmd;
	reg.bf.violate_chg_dei_cmd = ir->violate_chg_dei_cmd;

	reg.bf.violate_pri_0 = ir->violate_pri &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_VIOLATE_PRI_OFFSET)) - 1U);
	reg.bf.violate_pri_1 = ir->violate_pri >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_VIOLATE_PRI_OFFSET);

	reg.bf.violate_dp  = ir->violate_dp;
	reg.bf.violate_pcp = ir->violate_pcp;
	reg.bf.violate_dei = ir->violate_dei;

	/* DSCP / remap */
	reg.bf.exceed_chg_dscp_cmd = ir->exceed_chg_dscp_cmd;
	reg.bf.exceed_dscp         = ir->exceed_dscp;
	reg.bf.violate_chg_dscp_cmd= ir->violate_chg_dscp_cmd;
	reg.bf.violate_dscp        = ir->violate_dscp;
	reg.bf.exceed_remap_cmd    = ir->exceed_remap_cmd;
	reg.bf.violate_remap_cmd   = ir->violate_remap_cmd;

	/* MEF10.3 max refresh split */
	reg.bf.cir_max_0 = ir->cir_max_refresh &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_MAX_OFFSET)) - 1U);
	reg.bf.cir_max_1 = ir->cir_max_refresh >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_CIR_MAX_OFFSET);
	reg.bf.eir_max   = ir->eir_max_refresh;

	/* group fields */
	reg.bf.grp_end   = ir->grp_end;
	reg.bf.grp_cf    = ir->grp_cf;
	reg.bf.nxt_ptr_0 = ir->nxt_ptr &
		((1U <<
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_NXT_PTR_OFFSET)) - 1U);
	reg.bf.nxt_ptr_1 = ir->nxt_ptr >>
		SW_FIELD_OFFSET_IN_WORD(IN_ACL_METER_CFG_TBL_NXT_PTR_OFFSET);

	reg.bf.length_mod = ir->length_mod;

	return httppe_in_acl_meter_cfg_tbl_set(dev_id, index, &reg);
}

sw_error_t
adpt_httppe_policer_time_slot_get(a_uint32_t dev_id, a_uint32_t *time_slot)
{
	sw_error_t rv = SW_OK;
	union time_slot_reg_u time_slot_reg;

	memset(&time_slot_reg, 0, sizeof(time_slot_reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(time_slot);


	rv = httppe_time_slot_reg_get(dev_id, &time_slot_reg);

	if( rv != SW_OK )
		return rv;

	*time_slot = time_slot_reg.bf.time_slot;

	return SW_OK;
}

sw_error_t
adpt_httppe_policer_time_slot_set(a_uint32_t dev_id, a_uint32_t time_slot)
{
	union time_slot_reg_u time_slot_reg;

	memset(&time_slot_reg, 0, sizeof(time_slot_reg));
	ADPT_DEV_ID_CHECK(dev_id);

	time_slot_reg.bf.time_slot = time_slot;
	return httppe_time_slot_reg_set(dev_id, &time_slot_reg);
}

sw_error_t
adpt_httppe_policer_bypass_en_get(a_uint32_t dev_id, fal_policer_frame_type_t frame_type,
	a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union pc_drop_bypass_reg_u drop_bypass_reg;

	memset(&drop_bypass_reg, 0, sizeof(drop_bypass_reg));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	rv = httppe_pc_drop_bypass_reg_get(dev_id, &drop_bypass_reg);
	SW_RTN_ON_ERROR (rv);

	if (frame_type == FAL_FRAME_DROPPED) {
		*enable = drop_bypass_reg.bf.drop_bypass_en;
	} else {
		return SW_BAD_PARAM;
	}

	return SW_OK;
}

sw_error_t
adpt_httppe_policer_bypass_en_set(a_uint32_t dev_id, fal_policer_frame_type_t frame_type,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union pc_drop_bypass_reg_u drop_bypass_reg;

	memset(&drop_bypass_reg, 0, sizeof(drop_bypass_reg));
	ADPT_DEV_ID_CHECK(dev_id);

	if (frame_type == FAL_FRAME_DROPPED) {
		drop_bypass_reg.bf.drop_bypass_en = enable;
	} else {
		return SW_BAD_PARAM;
	}

	rv = httppe_pc_drop_bypass_reg_set(dev_id, &drop_bypass_reg);
	SW_RTN_ON_ERROR (rv);

	return SW_OK;
}

#ifndef IN_POLICER_MINI
sw_error_t
adpt_httppe_policer_global_counter_get(a_uint32_t dev_id,
		fal_policer_global_counter_t *counter)
{
	union pc_global_cnt_tbl_u pc_global_cnt_tbl;
	a_uint32_t index = 0;

	memset(&pc_global_cnt_tbl, 0, sizeof(pc_global_cnt_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(counter);

	httppe_pc_global_cnt_tbl_get(dev_id, index, &pc_global_cnt_tbl);
	counter->policer_drop_packet_counter = pc_global_cnt_tbl.bf.pkt_cnt;
	counter->policer_drop_byte_counter = pc_global_cnt_tbl.bf.byte_cnt_1;
	counter->policer_drop_byte_counter = (counter->policer_drop_byte_counter << 32) |
		pc_global_cnt_tbl.bf.byte_cnt_0;

	httppe_pc_global_cnt_tbl_get(dev_id, index + 1, &pc_global_cnt_tbl);
	counter->policer_forward_packet_counter = pc_global_cnt_tbl.bf.pkt_cnt;
	counter->policer_forward_byte_counter = pc_global_cnt_tbl.bf.byte_cnt_1;
	counter->policer_forward_byte_counter = (counter->policer_forward_byte_counter << 32) |
		pc_global_cnt_tbl.bf.byte_cnt_0;

	httppe_pc_global_cnt_tbl_get(dev_id, index + 2, &pc_global_cnt_tbl);
	counter->policer_bypass_packet_counter = pc_global_cnt_tbl.bf.pkt_cnt;
	counter->policer_bypass_byte_counter = pc_global_cnt_tbl.bf.byte_cnt_1;
	counter->policer_bypass_byte_counter = (counter->policer_bypass_byte_counter << 32) |
		pc_global_cnt_tbl.bf.byte_cnt_0;

	return SW_OK;
}
#endif

/**
 * @}
 */
