/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_HTTPPE_ACL_H_
#define _ADPT_HTTPPE_ACL_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

typedef struct {
	a_uint32_t  cvid:12;
	a_uint32_t  cpcp:3;
	a_uint32_t  cdei:1;
	a_uint32_t  svid:12;
	a_uint32_t  spcp:3;
	a_uint32_t  sdei:1;
	a_uint32_t  l2_prot:5;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctag_fmt:3;
	a_uint32_t  stag_fmt:3;
	a_uint32_t  dhcp_type:3;
	a_uint32_t  mc_type:3;
}ADPT_HTTPPE_ACL_EXT_VLAN_RULE;

typedef struct {
	a_uint32_t  cvid_mask:12; /* it is max cvid when ragne is enabled */
	a_uint32_t  cpcp_mask:3;
	a_uint32_t  cdei_mask:1;
	a_uint32_t  svid_mask:12;
	a_uint32_t  spcp_mask:3;
	a_uint32_t  sdei_mask:1;
	a_uint32_t  l2_prot_mask:5;
	a_uint32_t  ctpid_index_mask:2;
	a_uint32_t  stpid_index_mask:2;
	a_uint32_t  ctag_fmt_mask:3;
	a_uint32_t  stag_fmt_mask:3;
	a_uint32_t  dhcp_type_mask:3;
	a_uint32_t  mc_type_mask:3;
}ADPT_HTTPPE_ACL_EXT_VLAN_RULE_MASK;

sw_error_t
_adpt_httppe_acl_ext_vlan_rule_sw_2_hw(fal_acl_rule_t *rule,
	ADPT_HTTPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_HTTPPE_ACL_EXT_VLAN_RULE_MASK *ext_vlanrule_mask,
	a_uint8_t *range_en, a_uint8_t inverse_en);

sw_error_t _adpt_httppe_acl_ext_vlan_rule_hw_2_sw(ADPT_HTTPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_HTTPPE_ACL_EXT_VLAN_RULE_MASK * ext_vlanrule_mask, a_uint8_t range_en,
	a_uint8_t inverse_en, fal_acl_rule_t * rule);

sw_error_t
adpt_httppe_acl_dscp_pcp_mapping_set(a_uint32_t dev_id, a_uint8_t group_id,
		a_uint8_t dscp, a_uint8_t pcp);
sw_error_t
adpt_httppe_acl_dscp_pcp_mapping_get(a_uint32_t dev_id, a_uint8_t group_id,
		a_uint8_t dscp, a_uint8_t *pcp);
sw_error_t
_adpt_httppe_acl_rule_hw_add(a_uint32_t dev_id, a_uint32_t list_pri,
		a_uint32_t hw_list_id, a_uint32_t rule_id, a_uint32_t rule_nr,
		fal_acl_rule_t *rule, ADPT_HPPE_ACL_RULE_MAP *rule_map,
		a_uint32_t allocated_entries);

sw_error_t
_adpt_httppe_acl_rule_sw_query(a_uint32_t dev_id,
		a_uint32_t hw_list_id, a_uint32_t hw_entries, fal_acl_rule_t *rule);

sw_error_t
_adpt_httppe_acl_rule_hw_delete(a_uint32_t dev_id,
		a_uint32_t hw_list_id, a_uint32_t hw_entries, a_uint32_t rule_nr);

sw_error_t
_adpt_httppe_acl_rule_dump(a_uint32_t dev_id, a_uint32_t hw_list_id, a_uint32_t hw_entries);

sw_error_t
_adpt_httppe_acl_rule_bind(a_uint32_t dev_id, a_uint32_t hw_list_id, a_uint32_t hw_entries,
		fal_acl_direc_t direc, fal_acl_bind_obj_t obj_t, a_uint32_t obj_idx);

sw_error_t
_adpt_httppe_acl_rule_unbind(a_uint32_t dev_id, a_uint32_t hw_list_id, a_uint32_t hw_entries,
		fal_acl_direc_t direc, fal_acl_bind_obj_t obj_t, a_uint32_t obj_idx);

sw_error_t
_adpt_httppe_acl_rule_priority_set(a_uint32_t dev_id, a_uint32_t hw_list_id,
		a_uint32_t hw_entries, a_uint32_t priority);

sw_error_t
_adpt_httppe_acl_rule_priority_get(a_uint32_t dev_id, a_uint32_t hw_list_id,
		a_uint32_t hw_entries, a_uint32_t *priority);

a_bool_t
_adpt_httppe_get_udf_profile_entry_by_index(a_uint32_t dev_id,
		a_uint32_t index, fal_acl_udf_profile_entry_t * entry, a_uint32_t * profile_id);

sw_error_t
_adpt_httppe_insert_udf_profile_entry_by_index(a_uint32_t dev_id,
		a_uint32_t index, fal_acl_udf_profile_entry_t * entry, a_uint32_t profile_id);

sw_error_t
_adpt_httppe_clear_udf_profile_entry(a_uint32_t dev_id, a_uint32_t index);

sw_error_t
adpt_httppe_acl_udf_profile_cfg_set(a_uint32_t dev_id, a_uint32_t profile_id,
		a_uint32_t udf_idx, fal_acl_udf_type_t udf_type, a_uint32_t offset);

sw_error_t
adpt_httppe_acl_udf_profile_cfg_get(a_uint32_t dev_id, a_uint32_t profile_id,
		a_uint32_t udf_idx, fal_acl_udf_type_t * udf_type, a_uint32_t * offset);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
/**
 * @}
 */
