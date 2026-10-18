/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#include "hsl_reg.h"
#include "fal_acl.h"
#include "adpt_jhppe_acl.h"

sw_error_t
_adpt_jhppe_acl_ext_vlan_rule_sw_2_hw(fal_acl_rule_t *rule,
	ADPT_JHPPE_ACL_EXT_VLAN_RULE * ext_vlanrule, ADPT_JHPPE_ACL_EXT_VLAN_RULE_MASK *ext_vlanrule_mask,
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

sw_error_t _adpt_jhppe_acl_ext_vlan_rule_hw_2_sw(ADPT_JHPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_JHPPE_ACL_EXT_VLAN_RULE_MASK * ext_vlanrule_mask, a_uint8_t range_en,
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

sw_error_t
adpt_jhppe_acl_dscp_pcp_mapping_set(a_uint32_t dev_id,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp)
{
	sw_error_t rv;

	switch(group_id) {
	case 0:
		rv = jhppe_l2_dscp_pbit_map_tbl_pcp_0_set(dev_id, dscp, pcp);
		break;
	case 1:
		rv = jhppe_l2_dscp_pbit_map_tbl_pcp_1_set(dev_id, dscp, pcp);
		break;
	default:
		rv = SW_NOT_SUPPORTED;
		break;
	}

	return rv;
}

sw_error_t
adpt_jhppe_acl_dscp_pcp_mapping_get(a_uint32_t dev_id,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	switch(group_id) {
	case 0:
		rv = jhppe_l2_dscp_pbit_map_tbl_pcp_0_get(dev_id, dscp, &val);
		break;
	case 1:
		rv = jhppe_l2_dscp_pbit_map_tbl_pcp_1_get(dev_id, dscp, &val);
		break;
	default:
		rv = SW_NOT_SUPPORTED;
	}

	*pcp = val;

	return rv;
}
