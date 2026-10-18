/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#include "adpt.h"
#include "adpt_hppe_acl.h"
#include "adpt_httppe_acl.h"
#include "hsl_htt_reg.h"

static sw_error_t
_adpt_httppe_acl_action_sw_2_hw(a_uint32_t dev_id,fal_acl_rule_t *rule, union ipo_action_u *hw_act)
{
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REDPT))
	{
		a_uint32_t dest_type = FAL_ACL_DEST_TYPE(rule->ports);
		a_uint32_t dest_val = FAL_ACL_DEST_VALUE(rule->ports);

		SSDK_DEBUG("rule->ports = %x\n", rule->ports);

		hw_act->bf.dest_info_change_en = 1;
		if(dest_type == FAL_ACL_DEST_NEXTHOP) /*nexthop*/
		{
			hw_act->bf.dest_info =
				HPPE_ACL_DEST_INFO(HPPE_ACL_DEST_NEXTHOP, dest_val);
		}
		else if(FAL_ACL_DEST_TYPE(rule->ports) == FAL_ACL_DEST_PORT_ID)/*vp*/
		{
			hw_act->bf.dest_info =
				HPPE_ACL_DEST_INFO(HPPE_ACL_DEST_PORT_ID, dest_val);
		}
		else if(FAL_ACL_DEST_TYPE(rule->ports) == FAL_ACL_DEST_PORT_BMP)/*bitmap*/
		{
			hw_act->bf.dest_info =
				HPPE_ACL_DEST_INFO(HPPE_ACL_DEST_PORT_BMP, dest_val);
		}
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_PERMIT))
	{
		hw_act->bf.dest_info_change_en = 1;
		hw_act->bf.fwd_cmd = 0;/*forward*/
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_DENY))
	{
		hw_act->bf.dest_info_change_en = 1;
		hw_act->bf.fwd_cmd = 1;/*drop*/
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_CPYCPU))
	{
		hw_act->bf.dest_info_change_en = 1;
		hw_act->bf.fwd_cmd = 2;/*copy to cpu*/
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_RDTCPU))
	{
		hw_act->bf.dest_info_change_en = 1;
		hw_act->bf.fwd_cmd = 3;/*redirect to cpu*/
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_MIRROR))
	{
		hw_act->bf.mirror_en= 1;
	}
	hw_act->bf.bypass_bitmap_0 = rule->bypass_bitmap[0] & 0x3fff;
	hw_act->bf.bypass_bitmap_1 = (rule->bypass_bitmap[0]>>14) & 0x3ffff;
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_STAG_VID))
	{
		hw_act->bf.svid_change_en = 1;
		hw_act->bf.stag_fmt = rule->stag_fmt;
		hw_act->bf.svid_0 = rule->stag_vid & 0x7ff;
		hw_act->bf.svid_1 = (rule->stag_vid >> 11) & 0x1;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_STAG_PRI))
	{
		hw_act->bf.stag_pcp_change_en = rule->stag_pri_change_cmd;
		hw_act->bf.stag_pcp = rule->stag_pri;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_STAG_DEI))
	{
		hw_act->bf.stag_dei_change_en = rule->stag_dei_change_cmd;
		hw_act->bf.stag_dei = rule->stag_dei;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_CTAG_VID))
	{
		hw_act->bf.cvid_change_en = 1;
		hw_act->bf.ctag_fmt = rule->ctag_fmt;
		hw_act->bf.cvid = rule->ctag_vid;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_CTAG_PRI))
	{
		hw_act->bf.ctag_pcp_change_en = rule->ctag_pri_change_cmd;
		hw_act->bf.ctag_pcp = rule->ctag_pri & 0x7;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_CTAG_CFI))
	{
		hw_act->bf.ctag_dei_change_en = rule->ctag_cfi_change_cmd;
		hw_act->bf.ctag_dei = rule->ctag_cfi;
	}

	if(FAL_ACTION_FLG_TST(rule->action_flg_ext, FAL_ACL_ACTION_REMOVE_TAGS))
	{
		hw_act->bf.tags_to_remove = rule->tags_to_remove;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg_ext, FAL_ACL_ACTION_REMARK_STAG_TPID))
	{
		hw_act->bf.stpid_cmd = rule->stag_tpid_cmd;
		hw_act->bf.stpid_index = rule->stag_tpid_index;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg_ext, FAL_ACL_ACTION_REMARK_CTAG_TPID))
	{
		hw_act->bf.ctpid_cmd = rule->ctag_tpid_cmd;
		hw_act->bf.ctpid_index = rule->ctag_tpid_index;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg_ext, FAL_ACL_ACTION_COUNTER))
	{
		hw_act->bf.counter_en = 1;
		hw_act->bf.counter_id = rule->counter_id;
		hw_act->bf.counter_mode = rule->counter_mode;
	}
	hw_act->bf.dscp_pbit_mapping_index = rule->dscp_pcp_mapping_index;

	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_DSCP))
	{
		hw_act->bf.dscp_tc_change_en = 1;
		hw_act->bf.dscp_tc = rule->dscp;
		hw_act->bf.dscp_tc_mask = rule->dscp_mask;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_INT_DP))
	{
		hw_act->bf.int_dp_change_en = 1;
		hw_act->bf.int_dp = rule->int_dp;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_POLICER_EN))
	{
		hw_act->bf.policer_en = 1;
		hw_act->bf.policer_index = rule->policer_ptr;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_REMARK_QUEUE))
	{
		hw_act->bf.qid_en = 1;
		hw_act->bf.qid = rule->queue;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_ENQUEUE_PRI))
	{
		hw_act->bf.enqueue_pri_change_en = 1;
		hw_act->bf.enqueue_pri = rule->enqueue_pri;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_SERVICE_CODE))
	{
		hw_act->bf.service_code_en = 1;
		hw_act->bf.service_code_0 = rule->service_code;
		hw_act->bf.service_code_1 = rule->service_code >>
			SW_FIELD_OFFSET_IN_WORD(IPO_ACTION_SERVICE_CODE_OFFSET);
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_SYN_TOGGLE))
	{
		hw_act->bf.syn_toggle = 1;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_CPU_CODE))
	{
		hw_act->bf.cpu_code_en = 1;
		hw_act->bf.cpu_code = rule->cpu_code;
	}
	if(FAL_ACTION_FLG_TST(rule->action_flg, FAL_ACL_ACTION_METADATA_EN))
	{
		hw_act->bf.metadata_en = 1;
		hw_act->bf.metadata_pri = rule->metadata_pri&0xf;
	}
	hw_act->bf.qos_res_prec = rule->qos_res_prec;
	return SW_OK;
}

static sw_error_t
_adpt_httppe_acl_action_hw_2_sw(a_uint32_t dev_id,union ipo_action_u *hw_act, fal_acl_rule_t *rule)
{
	if(hw_act->bf.dest_info_change_en)
	{
		a_uint32_t dest_type = HPPE_ACL_DEST_TYPE(hw_act->bf.dest_info);
		a_uint32_t dest_val = HPPE_ACL_DEST_VALUE(hw_act->bf.dest_info);
		SSDK_DEBUG("hw_act->bf.dest_info = %x\n", hw_act->bf.dest_info);
		if(dest_type == HPPE_ACL_DEST_NEXTHOP) /*nexthop*/
		{
			rule->ports = FAL_ACL_DEST_OFFSET(FAL_ACL_DEST_NEXTHOP,
					dest_val);
		}
		else if(dest_type == HPPE_ACL_DEST_PORT_ID) /*vp or trunk*/
		{
			rule->ports = FAL_ACL_DEST_OFFSET(FAL_ACL_DEST_PORT_ID,
					dest_val);
		}
		else if(dest_type == HPPE_ACL_DEST_PORT_BMP) /*bitmap*/
		{
			rule->ports = FAL_ACL_DEST_OFFSET(FAL_ACL_DEST_PORT_BMP,
					dest_val);
		}
		if(rule->ports != 0)
		{
			FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REDPT);
		}
		else if(hw_act->bf.fwd_cmd == HPPE_ACL_ACTION_RDTCPU)
		{
			FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_RDTCPU);
		}
		else if(hw_act->bf.fwd_cmd == HPPE_ACL_ACTION_COPYCPU)
		{
			FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_CPYCPU);
		}
		else if(hw_act->bf.fwd_cmd == HPPE_ACL_ACTION_DROP)
		{
			FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_DENY);
		}
		else if(hw_act->bf.fwd_cmd == HPPE_ACL_ACTION_FWD)
		{
			FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_PERMIT);
		}
	}

	if(hw_act->bf.mirror_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_MIRROR);
	}
	if(hw_act->bf.bypass_bitmap_0 != 0 ||
		hw_act->bf.bypass_bitmap_1 != 0)
	{
		rule->bypass_bitmap[0] = (hw_act->bf.bypass_bitmap_1<<14)|hw_act->bf.bypass_bitmap_0;
	}
	if(hw_act->bf.svid_change_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_STAG_VID);
		rule->stag_fmt = hw_act->bf.stag_fmt;
		rule->stag_vid = (hw_act->bf.svid_0 & 0x7ff) |
						((hw_act->bf.svid_1 & 0x1) << 11);
	}
	if(hw_act->bf.stag_pcp_change_en)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_STAG_PRI);
		rule->stag_pri = hw_act->bf.stag_pcp;
		rule->stag_pri_change_cmd = hw_act->bf.stag_pcp_change_en; /* stag pcp change cmd */
	}
	if(hw_act->bf.stag_dei_change_en)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_STAG_DEI);
		rule->stag_dei = hw_act->bf.stag_dei;
		rule->stag_dei_change_cmd = hw_act->bf.stag_dei_change_en; /* stag dei change cmd */
	}
	if(hw_act->bf.cvid_change_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_CTAG_VID);
		rule->ctag_fmt = hw_act->bf.ctag_fmt;
		rule->ctag_vid = hw_act->bf.cvid;
	}
	if(hw_act->bf.ctag_pcp_change_en)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_CTAG_PRI);
		rule->ctag_pri_change_cmd = hw_act->bf.ctag_pcp_change_en; /* ctag pcp change cmd */
		rule->ctag_pri = hw_act->bf.ctag_pcp & 0x7;
	}
	if(hw_act->bf.ctag_dei_change_en)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_CTAG_CFI);
		rule->ctag_cfi = hw_act->bf.ctag_dei;
		rule->ctag_cfi_change_cmd = hw_act->bf.ctag_dei_change_en; /* ctag dei change cmd */
	}

	if(hw_act->bf.tags_to_remove)
	{
		FAL_ACTION_FLG_SET(rule->action_flg_ext, FAL_ACL_ACTION_REMOVE_TAGS);
		rule->tags_to_remove = hw_act->bf.tags_to_remove;
	}
	if(hw_act->bf.stpid_cmd)
	{
		FAL_ACTION_FLG_SET(rule->action_flg_ext, FAL_ACL_ACTION_REMARK_STAG_TPID);
		rule->stag_tpid_cmd = hw_act->bf.stpid_cmd;
		rule->stag_tpid_index = hw_act->bf.stpid_index;
	}
	if(hw_act->bf.ctpid_cmd)
	{
		FAL_ACTION_FLG_SET(rule->action_flg_ext, FAL_ACL_ACTION_REMARK_CTAG_TPID);
		rule->ctag_tpid_cmd = hw_act->bf.ctpid_cmd;
		rule->ctag_tpid_index = hw_act->bf.ctpid_index;
	}
	if (hw_act->bf.counter_en)
	{
		FAL_ACTION_FLG_SET(rule->action_flg_ext, FAL_ACL_ACTION_COUNTER);
		rule->counter_id = hw_act->bf.counter_id;
		rule->counter_mode = hw_act->bf.counter_mode;
	}
	rule->dscp_pcp_mapping_index = hw_act->bf.dscp_pbit_mapping_index;

	if(hw_act->bf.dscp_tc_change_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_DSCP);
		rule->dscp = hw_act->bf.dscp_tc;
		rule->dscp_mask = hw_act->bf.dscp_tc_mask;
	}
	if(hw_act->bf.int_dp_change_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_INT_DP);
		rule->int_dp = hw_act->bf.int_dp;
	}
	if(hw_act->bf.policer_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_POLICER_EN);
		rule->policer_ptr = hw_act->bf.policer_index;
	}
	if(hw_act->bf.qid_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_REMARK_QUEUE);
		rule->queue = hw_act->bf.qid;
	}
	if(hw_act->bf.enqueue_pri_change_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_ENQUEUE_PRI);
		rule->enqueue_pri = hw_act->bf.enqueue_pri;
	}
	if(hw_act->bf.service_code_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_SERVICE_CODE);
		rule->service_code = (hw_act->bf.service_code_1 <<
				SW_FIELD_OFFSET_IN_WORD(IPO_ACTION_SERVICE_CODE_OFFSET)) |
				hw_act->bf.service_code_0;
	}
	if(hw_act->bf.syn_toggle)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_SYN_TOGGLE);
	}
	if(hw_act->bf.cpu_code_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_CPU_CODE);
		rule->cpu_code = hw_act->bf.cpu_code;
	}
	if(hw_act->bf.metadata_en == 1)
	{
		FAL_ACTION_FLG_SET(rule->action_flg, FAL_ACL_ACTION_METADATA_EN);
		rule->metadata_pri = hw_act->bf.metadata_pri & 0xf;
	}
	rule->qos_res_prec = hw_act->bf.qos_res_prec;
	return SW_OK;
}

sw_error_t
_adpt_httppe_acl_rule_hw_add(a_uint32_t dev_id, a_uint32_t list_pri,
		a_uint32_t hw_list_id, a_uint32_t rule_id, a_uint32_t rule_nr,
		fal_acl_rule_t *rule, ADPT_HPPE_ACL_RULE_MAP *rule_map,
		a_uint32_t allocated_entries)
{
	union ipo_rule_reg_u hw_reg = {0};
	union ipo_mask_reg_u hw_mask = {0};
	union ipo_action_u hw_act = {0};
	sw_error_t rv = 0;
	a_uint32_t hw_entry = 0, rule_type = 0;
	a_uint8_t range_en = 0;

	hw_reg.bf.post_routing_en = rule->post_routing;
	hw_reg.bf.res_chain = rule->acl_pool;
	hw_reg.bf.pri = (list_pri<<3)|rule->pri;

	for(rule_type = 0; rule_type < ADPT_ACL_HPPE_RULE_TYPE_NUM; rule_type++)
	{
		if ((BIT(rule_type)) & rule_map->rule_type_map) {
			hw_reg.bf.rule_field_0 = 0;
			hw_reg.bf.rule_field_1 = 0;
			memset(&hw_mask, 0, sizeof(hw_mask));
			memset(&hw_act, 0, sizeof(hw_act));

			/*set 53bit rule fields of hw rule reg*/
			rv = _adpt_hppe_acl_rule_sw_2_hw(dev_id, rule, rule_type,
				&hw_entry, &allocated_entries, &range_en,
				&hw_reg, &hw_mask, 0);
			SW_RTN_ON_ERROR(rv);

			/*set rule_type, range_en, inverse_en fields of hw rule reg*/
			hw_reg.bf.rule_type = rule_type;
			hw_reg.bf.range_en = range_en;
			hw_reg.bf.inverse_en = 0;
			if(FAL_FIELD_FLG_TST(rule->field_flg, FAL_ACL_FIELD_INVERSE_ALL))
				hw_reg.bf.inverse_en = 1;

			SSDK_DEBUG("post_route %d, chain %d, pri %d, src_1 %d, src_0 %d, src_type %d"
				" rule_type %d, inverse %d, range %d\n", hw_reg.bf.post_routing_en,
				hw_reg.bf.res_chain, hw_reg.bf.pri, hw_reg.bf.src_1,
				hw_reg.bf.src_0, hw_reg.bf.src_type, hw_reg.bf.rule_type,
				hw_reg.bf.inverse_en, hw_reg.bf.range_en);
			SSDK_DEBUG("rule and mask set hw_entry = %d\n",
					hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry);

			rv = _adpt_httppe_acl_action_sw_2_hw(dev_id,rule, &hw_act);
			SW_RTN_ON_ERROR(rv);
			/*_adpt_acl_reg_dump((a_uint8_t *)&hw_act, sizeof(hw_act));*/
			rv = httppe_ipo_action_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry, &hw_act);
			SW_RTN_ON_ERROR(rv);
			/*_adpt_acl_reg_dump((a_uint8_t *)&hw_reg, sizeof(hw_reg));*/
			rv = httppe_ipo_rule_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry, &hw_reg);
			SW_RTN_ON_ERROR(rv);
			/*_adpt_acl_reg_dump((a_uint8_t *)&hw_mask, sizeof(hw_mask));*/
			rv = httppe_ipo_mask_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry, &hw_mask);
			SW_RTN_ON_ERROR(rv);
		}

		/* inverse rule */
		for (; rule_map->inverse_rule_type_count[rule_type] > 0;
				rule_map->inverse_rule_type_count[rule_type] --) {
			hw_reg.bf.rule_field_0 = 0;
			hw_reg.bf.rule_field_1 = 0;
			memset(&hw_mask, 0, sizeof(hw_mask));
			memset(&hw_act, 0, sizeof(hw_act));

			/*set 53bit rule fields of hw rule reg*/
			rv = _adpt_hppe_acl_rule_sw_2_hw(dev_id, rule, rule_type,
				&hw_entry, &allocated_entries, &range_en,
				&hw_reg, &hw_mask, 1);
			SW_RTN_ON_ERROR(rv);

			/*set rule_type, range_en, inverse_en fields of hw rule reg*/
			hw_reg.bf.rule_type = rule_type;
			hw_reg.bf.range_en = range_en;
			hw_reg.bf.inverse_en = 1;

			SSDK_DEBUG("rule_type %d inverse_rule_type_count %d rule_hw_entry %d\n",
				rule_type, rule_map->inverse_rule_type_count[rule_type],
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry);

			rv = _adpt_httppe_acl_action_sw_2_hw(dev_id, rule, &hw_act);
			SW_RTN_ON_ERROR(rv);

			/*_adpt_acl_reg_dump((a_uint8_t *)&hw_act, sizeof(hw_act));*/
			rv = httppe_ipo_action_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry, &hw_act);
			SW_RTN_ON_ERROR(rv);
			/*_adpt_acl_reg_dump((a_uint8_t *)&hw_reg, sizeof(hw_reg));*/
			rv = httppe_ipo_rule_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry, &hw_reg);
			SW_RTN_ON_ERROR(rv);
			/*_adpt_acl_reg_dump((a_uint8_t *)&hw_mask, sizeof(hw_mask));*/
			rv = httppe_ipo_mask_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST + hw_entry, &hw_mask);
			SW_RTN_ON_ERROR(rv);
		}
	}

	return SW_OK;
}

sw_error_t
_adpt_httppe_acl_rule_sw_query(a_uint32_t dev_id,
	a_uint32_t hw_list_id, a_uint32_t hw_entries, fal_acl_rule_t *rule)
{
	a_uint32_t hw_index;
	a_uint64_t byte_cnt;
	sw_error_t rv = SW_OK;
	union ipo_rule_reg_u hw_reg = {0};
	union ipo_mask_reg_u hw_mask = {0};
	union ipo_action_u hw_act = {0};
	union ipo_cnt_tbl_u hw_match = {0};

	while(hw_entries != 0)
	{
		hw_index = _acl_bit_index(hw_entries, ADPT_ACL_ENTRY_NUM_PER_LIST, 0);
		if(hw_index >= ADPT_ACL_ENTRY_NUM_PER_LIST)
			return SW_FAIL;

		rv = httppe_ipo_rule_reg_get(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_reg);
		SW_RTN_ON_ERROR(rv);

		rv = httppe_ipo_mask_reg_get(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_mask);
		SW_RTN_ON_ERROR(rv);

		rv = httppe_ipo_action_get(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_act);
		SW_RTN_ON_ERROR(rv);

		rule->post_routing = hw_reg.bf.post_routing_en;
		rule->acl_pool = hw_reg.bf.res_chain;
		rule->pri = hw_reg.bf.pri&0x7;

		/*get sw rule info from first 53bit hw rule reg fields*/
		rv = _adpt_hppe_acl_rule_hw_2_sw(dev_id, hw_reg.bf.rule_type,
				hw_reg.bf.range_en, hw_reg.bf.inverse_en, &hw_reg, &hw_mask, rule);
		SW_RTN_ON_ERROR(rv);

		rv = _adpt_httppe_acl_action_hw_2_sw(dev_id, &hw_act, rule);
		SW_RTN_ON_ERROR(rv);

		rv = httppe_ipo_cnt_tbl_get(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_match);
		SW_RTN_ON_ERROR(rv);

		rule->match_cnt += hw_match.bf.hit_pkt_cnt;
		byte_cnt = hw_match.bf.hit_byte_cnt_1;
		rule->match_bytes += byte_cnt<<32|hw_match.bf.hit_byte_cnt_0;

		hw_entries &= (~(1<<hw_index));
	}
	return rv;
}

sw_error_t
_adpt_httppe_acl_rule_hw_delete(a_uint32_t dev_id,
		a_uint32_t hw_list_id, a_uint32_t hw_entries, a_uint32_t rule_nr)
{
	a_uint32_t hw_index = 0;
	sw_error_t rv = SW_OK;
	union ipo_rule_reg_u hw_reg = {0};
	union ipo_mask_reg_u hw_mask = {0};
	union ipo_action_u hw_act = {0};
	union ipo_cnt_tbl_u counters = {0};
	while(hw_entries != 0)
	{
		hw_index = _acl_bit_index(hw_entries, ADPT_ACL_ENTRY_NUM_PER_LIST, 0);
		if(hw_index >= ADPT_ACL_ENTRY_NUM_PER_LIST)
			break;

		rv = httppe_ipo_rule_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_reg);
		SW_RTN_ON_ERROR(rv);

		rv = httppe_ipo_mask_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_mask);
		SW_RTN_ON_ERROR(rv);

		rv = httppe_ipo_action_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_act);
		SW_RTN_ON_ERROR(rv);

		SSDK_DEBUG("ACL destroy entry %d\n",
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index);
		hw_entries &= (~(1<<hw_index));

		/*clean counters*/
		httppe_ipo_cnt_tbl_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &counters);
	}
	return rv;
}

sw_error_t
_adpt_httppe_acl_rule_dump(a_uint32_t dev_id, a_uint32_t hw_list_id, a_uint32_t hw_entries)
{
	a_uint8_t i = 0;
	union ipo_rule_reg_u hw_reg = {0};
	union ipo_mask_reg_u hw_mask = {0};
	union ipo_action_u hw_act = {0};

	if(hw_entries != 0)
	{
		for(i = 0; i < ADPT_ACL_ENTRY_NUM_PER_LIST; i++)
		{
			if((1<<i) & hw_entries)
			{
				httppe_ipo_rule_reg_get(dev_id,
					hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+i, &hw_reg);
				httppe_ipo_mask_reg_get(dev_id,
					hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+i, &hw_mask);
				httppe_ipo_action_get(dev_id,
					hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+i, &hw_act);
				printk("hw_entry %d\n", hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+i);
				_adpt_acl_reg_dump((u_int8_t *)&hw_reg, sizeof(hw_reg));
				printk("hw_entry_mask %d\n",
					hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+i);
				_adpt_acl_reg_dump((u_int8_t *)&hw_mask, sizeof(hw_mask));
				printk("hw_action %d\n", hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+i);
				_adpt_acl_reg_dump((u_int8_t *)&hw_act, sizeof(hw_act));
			}
		}
	}

	return SW_OK;
}

sw_error_t
_adpt_httppe_acl_rule_bind(a_uint32_t dev_id, a_uint32_t hw_list_id, a_uint32_t hw_entries,
	fal_acl_direc_t direc, fal_acl_bind_obj_t obj_t, a_uint32_t obj_idx)
{
	a_uint32_t hw_index = 0, hw_srctype = 0;
	union ipo_rule_reg_u hw_reg = {0};

	while(hw_entries != 0)
	{
		hw_index = _acl_bit_index(hw_entries, ADPT_ACL_ENTRY_NUM_PER_LIST, 0);
		if(hw_index >= ADPT_ACL_ENTRY_NUM_PER_LIST)
		{
			break;
		}

		httppe_ipo_rule_reg_get(dev_id, hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index,
			&hw_reg);

		if(obj_t == FAL_ACL_BIND_PORT && obj_idx < SSDK_MAX_PORT_NUM)
		{
			/*convert port to bitmap if it is physical port*/
			obj_t = FAL_ACL_BIND_PORTBITMAP;
			obj_idx = (1<<obj_idx);
		}

		hw_srctype = _adpt_hppe_acl_srctype_to_hw(dev_id, obj_t);

		if(hw_srctype == HPPE_ACL_TYPE_INVALID)
		{
			SSDK_ERROR("Invalid source type %d\n", obj_t);
			return SW_BAD_PARAM;
		}
		else if(hw_srctype == HPPE_ACL_TYPE_PORTBITMAP &&
			hw_reg.bf.src_type == HPPE_ACL_TYPE_PORTBITMAP)
		{
			hw_reg.bf.src_0 |= obj_idx;
			hw_reg.bf.src_1 |= obj_idx>>
				SW_FIELD_OFFSET_IN_WORD(IPO_RULE_REG_SRC_OFFSET);
		}
		else if(hw_srctype == APPE_ACL_TYPE_SERVICE_PORTBITMAP &&
			hw_reg.bf.src_type == APPE_ACL_TYPE_SERVICE_PORTBITMAP)
		{
			hw_reg.bf.src_0 |= obj_idx;
			hw_reg.bf.src_1 |= obj_idx>>
				SW_FIELD_OFFSET_IN_WORD(IPO_RULE_REG_SRC_OFFSET);
		}
		else
		{
			hw_reg.bf.src_0 = obj_idx;
			hw_reg.bf.src_1 = obj_idx>>
				SW_FIELD_OFFSET_IN_WORD(IPO_RULE_REG_SRC_OFFSET);
		}
		hw_reg.bf.src_type = hw_srctype;

		httppe_ipo_rule_reg_set(dev_id, hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index,
			&hw_reg);
		SSDK_DEBUG("ACL bind entry %d source type %d, source value 0x%x\n",
			hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, obj_t, obj_idx);
		hw_entries &= (~(1<<hw_index));
	}

	return SW_OK;
}

sw_error_t
_adpt_httppe_acl_rule_unbind(a_uint32_t dev_id, a_uint32_t hw_list_id, a_uint32_t hw_entries,
	fal_acl_direc_t direc, fal_acl_bind_obj_t obj_t, a_uint32_t obj_idx)
{
	a_uint32_t hw_index = 0;
	union ipo_rule_reg_u hw_reg = {0};

	while(hw_entries != 0)
	{
		hw_index = _acl_bit_index(hw_entries, ADPT_ACL_ENTRY_NUM_PER_LIST, 0);
		if(hw_index >= ADPT_ACL_ENTRY_NUM_PER_LIST)
		{
			break;
		}

		httppe_ipo_rule_reg_get(dev_id, hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index,
			&hw_reg);

		if(obj_t == FAL_ACL_BIND_PORT && obj_idx < SSDK_MAX_PORT_NUM)
		{
			/*convert port to bitmap if it is physical port*/
			obj_t = FAL_ACL_BIND_PORTBITMAP;
			obj_idx = (1<<obj_idx);
		}

		if(hw_reg.bf.src_type != _adpt_hppe_acl_srctype_to_hw(dev_id, obj_t))
		{
			SSDK_ERROR("ACL unbind fail obj_t %d\n", obj_t);
			return SW_NOT_FOUND;
		}
		if(hw_reg.bf.src_type == HPPE_ACL_TYPE_PORTBITMAP)
		{
			hw_reg.bf.src_0 &= ~obj_idx;
			hw_reg.bf.src_1 &= ~(obj_idx>>
				SW_FIELD_OFFSET_IN_WORD(IPO_RULE_REG_SRC_OFFSET));
		}
		else if(hw_reg.bf.src_type == APPE_ACL_TYPE_SERVICE_PORTBITMAP)
		{
			hw_reg.bf.src_0 &= ~obj_idx;
			hw_reg.bf.src_1 &= ~(obj_idx>>
				SW_FIELD_OFFSET_IN_WORD(IPO_RULE_REG_SRC_OFFSET));
		}
		else
		{
			hw_reg.bf.src_type = HPPE_ACL_TYPE_PORTBITMAP;
			hw_reg.bf.src_0 = 0;
			hw_reg.bf.src_1 = 0;
		}
		httppe_ipo_rule_reg_set(dev_id, hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index,
			&hw_reg);
		SSDK_DEBUG("ACL unbind entry %d source type %d, source value 0x%x\n",
			hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, obj_t, obj_idx);
		hw_entries &= (~(1<<hw_index));
	}

	return SW_OK;
}

sw_error_t
_adpt_httppe_acl_rule_priority_set(a_uint32_t dev_id, a_uint32_t hw_list_id,
		a_uint32_t hw_entries, a_uint32_t priority)
{
	union ipo_rule_reg_u hw_reg = {0};
	a_uint32_t hw_index;
	sw_error_t rv = SW_OK;

	while(hw_entries != 0)
	{
		hw_index = _acl_bit_index(hw_entries, ADPT_ACL_ENTRY_NUM_PER_LIST, 0);
		if(hw_index >= ADPT_ACL_ENTRY_NUM_PER_LIST)
			return SW_FAIL;

		rv = httppe_ipo_rule_reg_get(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_reg);
		SW_RTN_ON_ERROR(rv);
		hw_reg.bf.pri = priority;
		rv = httppe_ipo_rule_reg_set(dev_id,
				hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_reg);
		SW_RTN_ON_ERROR(rv);
		hw_entries &= (~(1<<hw_index));
	}

	return rv;
}

sw_error_t
_adpt_httppe_acl_rule_priority_get(a_uint32_t dev_id, a_uint32_t hw_list_id,
		a_uint32_t hw_entries, a_uint32_t *priority)
{
	union ipo_rule_reg_u hw_reg = {0};
	a_uint32_t hw_index;
	sw_error_t rv = SW_OK;

	hw_index = _acl_bit_index(hw_entries, ADPT_ACL_ENTRY_NUM_PER_LIST, 0);
	if(hw_index >= ADPT_ACL_ENTRY_NUM_PER_LIST)
		return SW_FAIL;

	rv = httppe_ipo_rule_reg_get(dev_id,
			hw_list_id*ADPT_ACL_ENTRY_NUM_PER_LIST+hw_index, &hw_reg);
	SW_RTN_ON_ERROR(rv);

	*priority = hw_reg.bf.pri;

	return rv;
}

sw_error_t
_adpt_httppe_acl_ext_vlan_rule_sw_2_hw(fal_acl_rule_t *rule,
	ADPT_HTTPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_HTTPPE_ACL_EXT_VLAN_RULE_MASK *ext_vlanrule_mask,
	a_uint8_t *range_en, a_uint8_t inverse_en)
{
	fal_acl_field_map_t field_flg = {0};

	if (inverse_en)
		FAL_FIELD_FLG_CPY(field_flg, rule->inverse_field_flg);
	else
		FAL_FIELD_FLG_CPY(field_flg, rule->field_flg);

	/*ctag*/
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_CTAG_VID))
	{
		if(FAL_ACL_FIELD_MASK == rule->ctag_vid_op)
		{
			ext_vlanrule->cvid = rule->ctag_vid_val;
			ext_vlanrule_mask->cvid_mask = rule->ctag_vid_mask;
		}
		else
		{
			a_uint16_t min, max;
			if(FAL_ACL_FIELD_LE == rule->ctag_vid_op)
			{
				min = 0;
				max = rule->ctag_vid_val;
			}
			else if(FAL_ACL_FIELD_GE == rule->ctag_vid_op)
			{
				min = rule->ctag_vid_val;
				max = 0xfff;
			}
			else if(FAL_ACL_FIELD_RANGE == rule->ctag_vid_op)
			{
				min = rule->ctag_vid_val;
				max = rule->ctag_vid_mask;
			}
			else
				return SW_NOT_SUPPORTED;
			ext_vlanrule->cvid = min;
			ext_vlanrule_mask->cvid_mask = max;
			*range_en = 1;
		}
		if (inverse_en) {
			FAL_FIELD_FLG_CLR(rule->inverse_field_flg, FAL_ACL_FIELD_MAC_CTAG_VID);
			return SW_OK;
		}
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_CTAG_PRI))
	{
		ext_vlanrule->cpcp = rule->ctag_pri_val;
		ext_vlanrule_mask->cpcp_mask = rule->ctag_pri_mask;
		if (inverse_en) {
			FAL_FIELD_FLG_CLR(rule->inverse_field_flg, FAL_ACL_FIELD_MAC_CTAG_PRI);
			return SW_OK;
		}
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_CTAG_CFI))
	{
		ext_vlanrule->cdei = rule->ctag_cfi_val;
		ext_vlanrule_mask->cdei_mask = rule->ctag_cfi_mask;
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_CTAGGED))
	{
		ext_vlanrule->ctag_fmt = rule->ctagged_val;
		ext_vlanrule_mask->ctag_fmt_mask = rule->ctagged_mask;
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_CTAG_TPID_INDEX))
	{
		ext_vlanrule->ctpid_index = rule->ctag_tpid_index_val;
		ext_vlanrule_mask->ctpid_index_mask = rule->ctag_tpid_index_mask;
		if (inverse_en) {
			FAL_FIELD_FLG_CLR(rule->inverse_field_flg, FAL_ACL_FIELD_CTAG_TPID_INDEX);
			return SW_OK;
		}
	}

	/*stag*/
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_STAG_VID) &&
		(rule->stag_vid_op == FAL_ACL_FIELD_MASK))
	{
		ext_vlanrule->svid = rule->stag_vid_val;
		ext_vlanrule_mask->svid_mask = rule->stag_vid_mask;
		if (inverse_en) {
			FAL_FIELD_FLG_CLR(rule->inverse_field_flg, FAL_ACL_FIELD_MAC_STAG_VID);
			return SW_OK;
		}
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_STAG_PRI))
	{
		ext_vlanrule->spcp = rule->stag_pri_val;
		ext_vlanrule_mask->spcp_mask = rule->stag_pri_mask;
		if (inverse_en) {
			FAL_FIELD_FLG_CLR(rule->inverse_field_flg, FAL_ACL_FIELD_MAC_STAG_PRI);
			return SW_OK;
		}
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_STAG_DEI))
	{
		ext_vlanrule->sdei = rule->stag_dei_val;
		ext_vlanrule_mask->sdei_mask = rule->stag_dei_mask;
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MAC_STAGGED))
	{
		ext_vlanrule->stag_fmt = rule->stagged_val;
		ext_vlanrule_mask->stag_fmt_mask = rule->stagged_mask;
	}
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_STAG_TPID_INDEX))
	{
		ext_vlanrule->stpid_index = rule->stag_tpid_index_val;
		ext_vlanrule_mask->stpid_index_mask = rule->stag_tpid_index_mask;
		if (inverse_en) {
			FAL_FIELD_FLG_CLR(rule->inverse_field_flg, FAL_ACL_FIELD_STAG_TPID_INDEX);
			return SW_OK;
		}
	}
	/* l2 proto type */
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_L2_PROTO))
	{
		ext_vlanrule->l2_prot= rule->l2_proto_type;
		ext_vlanrule_mask->l2_prot_mask = rule->l2_proto_type_mask;
	}
	/* DHCP type */
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_DHCP_TYPE))
	{
		ext_vlanrule->dhcp_type= rule->dhcp_type;
		ext_vlanrule_mask->dhcp_type_mask = rule->dhcp_type_mask;
	}
	/* MC type */
	if(FAL_FIELD_FLG_TST(field_flg, FAL_ACL_FIELD_MC_TYPE))
	{
		ext_vlanrule->mc_type= rule->mc_type;
		ext_vlanrule_mask->mc_type_mask = rule->mc_type_mask;
	}

	return SW_OK;
}

sw_error_t _adpt_httppe_acl_ext_vlan_rule_hw_2_sw(ADPT_HTTPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_HTTPPE_ACL_EXT_VLAN_RULE_MASK * ext_vlanrule_mask, a_uint8_t range_en,
	a_uint8_t inverse_en, fal_acl_rule_t * rule)
{
	fal_acl_field_map_t field_flg = {0};

	/*ctag*/
	if(ext_vlanrule_mask->cvid_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_CTAG_VID);
		rule->ctag_vid_mask = ext_vlanrule_mask->cvid_mask;
		if(range_en)
		{
			if(ext_vlanrule->cvid == 0)
			{
				rule->ctag_vid_op = FAL_ACL_FIELD_LE;
				rule->ctag_vid_val = ext_vlanrule_mask->cvid_mask;
			}
			else if(ext_vlanrule_mask->cvid_mask == 0xfff)
			{
				rule->ctag_vid_op = FAL_ACL_FIELD_GE;
				rule->ctag_vid_val = ext_vlanrule->cvid;
			}
			else
			{
				rule->ctag_vid_op = FAL_ACL_FIELD_RANGE;
				rule->ctag_vid_val = ext_vlanrule->cvid;
			}

		}
		else
		{
			rule->ctag_vid_op = FAL_ACL_FIELD_MASK;
			rule->ctag_vid_val = ext_vlanrule->cvid;
		}
	}

	if(ext_vlanrule_mask->cpcp_mask)
	{

		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_CTAG_PRI);
		rule->ctag_pri_val = ext_vlanrule->cpcp;
		rule->ctag_pri_mask = ext_vlanrule_mask->cpcp_mask;
	}

	if(ext_vlanrule_mask->cdei_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_CTAG_CFI);
		rule->ctag_cfi_val = ext_vlanrule->cdei;
		rule->ctag_cfi_mask = ext_vlanrule_mask->cdei_mask;
	}

	if(ext_vlanrule_mask->ctag_fmt_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_CTAGGED);
		rule->ctagged_val = ext_vlanrule->ctag_fmt;
		rule->ctagged_mask = ext_vlanrule_mask->ctag_fmt_mask;
	}
	if(ext_vlanrule_mask->ctpid_index_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_CTAG_TPID_INDEX);
		rule->ctag_tpid_index_val = ext_vlanrule->ctpid_index;
		rule->ctag_tpid_index_mask = ext_vlanrule_mask->ctpid_index_mask;
	}

	/*stag*/
	if(ext_vlanrule_mask->svid_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_STAG_VID);
		rule->stag_vid_val = ext_vlanrule->svid;
		rule->stag_vid_mask = ext_vlanrule_mask->svid_mask;
	}
	if(ext_vlanrule_mask->spcp_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_STAG_PRI);
		rule->stag_pri_val = ext_vlanrule->spcp;
		rule->stag_pri_mask = ext_vlanrule_mask->spcp_mask;
	}
	if(ext_vlanrule_mask->sdei_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_STAG_DEI);
		rule->stag_dei_val = ext_vlanrule->sdei;
		rule->stag_dei_mask = ext_vlanrule_mask->sdei_mask;
	}
	if(ext_vlanrule_mask->stag_fmt_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MAC_STAGGED);
		rule->stagged_val = ext_vlanrule->stag_fmt;
		rule->stagged_mask = ext_vlanrule_mask->stag_fmt_mask;
	}
	if(ext_vlanrule_mask->stpid_index_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_STAG_TPID_INDEX);
		rule->stag_tpid_index_val = ext_vlanrule->stpid_index;
		rule->stag_tpid_index_mask = ext_vlanrule_mask->stpid_index_mask;
	}
	/* l2 proto type */
	if(ext_vlanrule_mask->l2_prot_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_L2_PROTO);
		rule->l2_proto_type = ext_vlanrule->l2_prot;
		rule->l2_proto_type_mask = ext_vlanrule_mask->l2_prot_mask;
	}
	/* DHCP type */
	if(ext_vlanrule_mask->dhcp_type_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_DHCP_TYPE);
		rule->dhcp_type = ext_vlanrule->dhcp_type;
		rule->dhcp_type_mask = ext_vlanrule_mask->dhcp_type_mask;
	}
	/* MC type */
	if(ext_vlanrule_mask->mc_type_mask)
	{
		FAL_FIELD_FLG_SET(field_flg, FAL_ACL_FIELD_MC_TYPE);
		rule->mc_type = ext_vlanrule->mc_type;
		rule->mc_type_mask = ext_vlanrule_mask->mc_type_mask;
	}

	if (inverse_en)
		FAL_FIELD_FLG_CPY(rule->inverse_field_flg, field_flg);
	else
		FAL_FIELD_FLG_CPY(rule->field_flg, field_flg);

	return SW_OK;
}

a_bool_t
_adpt_httppe_get_udf_profile_entry_by_index(a_uint32_t dev_id,
		a_uint32_t index, fal_acl_udf_profile_entry_t * entry, a_uint32_t * profile_id)
{
	union ipr_udf_ctrl_u udf_ctrl = {0};
	sw_error_t rv = SW_OK;

	rv = httppe_ipr_udf_ctrl_get(dev_id, index, &udf_ctrl);
	if (rv != SW_OK)
		return A_FALSE;

	if (!udf_ctrl.bf.valid)
	{
		aos_mem_zero(&udf_ctrl, sizeof (udf_ctrl));
	}
	entry->l3_type = udf_ctrl.bf.l3_type;
	entry->l4_type = udf_ctrl.bf.l4_type;
	*profile_id = udf_ctrl.bf.udf_profile;

	if (udf_ctrl.bf.l3_type_incl)
	{
		FAL_FIELD_FLG_SET(entry->field_flag, FAL_ACL_UDF_PROFILE_ENTRY_FIELD_L3_TYPE);
	}
	else
	{
		FAL_FIELD_FLG_CLR(entry->field_flag, FAL_ACL_UDF_PROFILE_ENTRY_FIELD_L3_TYPE);
	}
	if (udf_ctrl.bf.l4_type_incl)
	{
		FAL_FIELD_FLG_SET(entry->field_flag, FAL_ACL_UDF_PROFILE_ENTRY_FIELD_L4_TYPE);
	}
	else
	{
		FAL_FIELD_FLG_CLR(entry->field_flag, FAL_ACL_UDF_PROFILE_ENTRY_FIELD_L4_TYPE);
	}
	return udf_ctrl.bf.valid;
}

sw_error_t
_adpt_httppe_insert_udf_profile_entry_by_index(a_uint32_t dev_id,
		a_uint32_t index, fal_acl_udf_profile_entry_t * entry, a_uint32_t profile_id)
{
	union ipr_udf_ctrl_u udf_ctrl = {0};

	udf_ctrl.bf.valid = A_TRUE;
	udf_ctrl.bf.l3_type = entry->l3_type;
	udf_ctrl.bf.l4_type = entry->l4_type;
	udf_ctrl.bf.udf_profile = profile_id;

	if (FAL_FIELD_FLG_TST(entry->field_flag, FAL_ACL_UDF_PROFILE_ENTRY_FIELD_L3_TYPE))
	{
		udf_ctrl.bf.l3_type_incl = A_TRUE;
	}
	if (FAL_FIELD_FLG_TST(entry->field_flag, FAL_ACL_UDF_PROFILE_ENTRY_FIELD_L4_TYPE))
	{
		udf_ctrl.bf.l4_type_incl = A_TRUE;
	}
	return httppe_ipr_udf_ctrl_set(dev_id, index, &udf_ctrl);
}

sw_error_t
_adpt_httppe_clear_udf_profile_entry(a_uint32_t dev_id, a_uint32_t index)
{
		union ipr_udf_ctrl_u udf_ctrl_zero_entry = {0};
		return httppe_ipr_udf_ctrl_set(dev_id, index, &udf_ctrl_zero_entry);
}

sw_error_t
adpt_httppe_acl_udf_profile_cfg_set(a_uint32_t dev_id, a_uint32_t profile_id,
		a_uint32_t udf_idx, fal_acl_udf_type_t udf_type, a_uint32_t offset)
{
	a_uint8_t udf_base = 0;
	union ipr_udf_profile_base_u udf_profile_base = {0};
	union ipr_udf_profile_offset_u udf_profile_offset = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	if (offset % 2)
	{ /*only support even data*/
		return SW_BAD_VALUE;
	}

	switch(udf_type)
	{
		case FAL_ACL_UDF_TYPE_L2:
			udf_base = 0;
			break;
		case FAL_ACL_UDF_TYPE_L3:
			udf_base = 1;
			break;
		case FAL_ACL_UDF_TYPE_L4:
			udf_base = 2;
			break;
		default:
			return SW_NOT_SUPPORTED;
	}

	SW_RTN_ON_ERROR(httppe_ipr_udf_profile_base_get(dev_id, profile_id, &udf_profile_base));
	SW_RTN_ON_ERROR(httppe_ipr_udf_profile_offset_get(dev_id, profile_id, &udf_profile_offset));

	switch(udf_idx)
	{
		case 0:
			udf_profile_base.bf.udf0_base = udf_base;
			udf_profile_offset.bf.udf0_offset = offset/2;
			break;
		case 1:
			udf_profile_base.bf.udf1_base = udf_base;
			udf_profile_offset.bf.udf1_offset = offset/2;
			break;
		case 2:
			udf_profile_base.bf.udf2_base = udf_base;
			udf_profile_offset.bf.udf2_offset = offset/2;
			break;
		case 3:
			udf_profile_base.bf.udf3_base = udf_base;
			udf_profile_offset.bf.udf3_offset = offset/2;
			break;
		default:
			return SW_OUT_OF_RANGE;
	}

	SW_RTN_ON_ERROR(httppe_ipr_udf_profile_base_set(dev_id, profile_id, &udf_profile_base));
	return httppe_ipr_udf_profile_offset_set(dev_id, profile_id, &udf_profile_offset);
}

sw_error_t
adpt_httppe_acl_udf_profile_cfg_get(a_uint32_t dev_id, a_uint32_t profile_id,
		a_uint32_t udf_idx, fal_acl_udf_type_t * udf_type, a_uint32_t * offset)
{
	a_uint8_t udf_base = 0;
	union ipr_udf_profile_base_u udf_profile_base = {0};
	union ipr_udf_profile_offset_u udf_profile_offset = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(udf_type);
	ADPT_NULL_POINT_CHECK(offset);

	SW_RTN_ON_ERROR(httppe_ipr_udf_profile_base_get(dev_id, profile_id, &udf_profile_base));
	SW_RTN_ON_ERROR(httppe_ipr_udf_profile_offset_get(dev_id, profile_id, &udf_profile_offset));

	switch(udf_idx)
	{
		case 0:
			udf_base = udf_profile_base.bf.udf0_base;
			*offset = udf_profile_offset.bf.udf0_offset*2;
			break;
		case 1:
			udf_base = udf_profile_base.bf.udf1_base;
			*offset = udf_profile_offset.bf.udf1_offset*2;
			break;
		case 2:
			udf_base = udf_profile_base.bf.udf2_base;
			*offset = udf_profile_offset.bf.udf2_offset*2;
			break;
		case 3:
			udf_base = udf_profile_base.bf.udf3_base;
			*offset = udf_profile_offset.bf.udf3_offset*2;
			break;
		default:
			return SW_OUT_OF_RANGE;
	}

	switch(udf_base)
	{
		case 0:
			*udf_type = FAL_ACL_UDF_TYPE_L2;
			break;
		case 1:
			*udf_type = FAL_ACL_UDF_TYPE_L3;
			break;
		case 2:
			*udf_type = FAL_ACL_UDF_TYPE_L4;
			break;
		default:
			return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

sw_error_t
adpt_httppe_acl_dscp_pcp_mapping_set(a_uint32_t dev_id,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp)
{
	sw_error_t rv;

	switch(group_id) {
	case 0:
		rv = httppe_l2_dscp_pbit_map_tbl_pcp_0_set(dev_id, dscp, pcp);
		break;
	case 1:
		rv = httppe_l2_dscp_pbit_map_tbl_pcp_1_set(dev_id, dscp, pcp);
		break;
	default:
		rv = SW_NOT_SUPPORTED;
		break;
	}

	return rv;
}

sw_error_t
adpt_httppe_acl_dscp_pcp_mapping_get(a_uint32_t dev_id,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	switch(group_id) {
	case 0:
		rv = httppe_l2_dscp_pbit_map_tbl_pcp_0_get(dev_id, dscp, &val);
		break;
	case 1:
		rv = httppe_l2_dscp_pbit_map_tbl_pcp_1_get(dev_id, dscp, &val);
		break;
	default:
		rv = SW_NOT_SUPPORTED;
	}

	*pcp = val;

	return rv;
}
