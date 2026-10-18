/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_JHPPE_ACL_
#define _ADPT_JHPPE_ACL_

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
}ADPT_JHPPE_ACL_EXT_VLAN_RULE;

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
}ADPT_JHPPE_ACL_EXT_VLAN_RULE_MASK;

sw_error_t
_adpt_jhppe_acl_ext_vlan_rule_sw_2_hw(fal_acl_rule_t *rule,
	ADPT_JHPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_JHPPE_ACL_EXT_VLAN_RULE_MASK *ext_vlanrule_mask,
	a_uint8_t *range_en, a_uint8_t inverse_en);

sw_error_t _adpt_jhppe_acl_ext_vlan_rule_hw_2_sw(ADPT_JHPPE_ACL_EXT_VLAN_RULE * ext_vlanrule,
	ADPT_JHPPE_ACL_EXT_VLAN_RULE_MASK * ext_vlanrule_mask, a_uint8_t range_en,
	a_uint8_t inverse_en, fal_acl_rule_t * rule);

sw_error_t
adpt_jhppe_acl_dscp_pcp_mapping_set(a_uint32_t dev_id, a_uint8_t group_id,
		a_uint8_t dscp, a_uint8_t pcp);
sw_error_t
adpt_jhppe_acl_dscp_pcp_mapping_get(a_uint32_t dev_id, a_uint8_t group_id,
		a_uint8_t dscp, a_uint8_t *pcp);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
