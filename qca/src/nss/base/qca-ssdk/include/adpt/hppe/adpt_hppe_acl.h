/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_HPPE_ACL_H_
#define _ADPT_HPPE_ACL_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#define ADPT_ACL_ENTRY_NUM_PER_LIST 8 /* hw rule entries number per hw list */

#define ADPT_ACL_HPPE_UDF0_RULE 13
#define ADPT_ACL_HPPE_UDF1_RULE 14
#define ADPT_ACL_HPPE_RULE_TYPE_NUM 19

#define HPPE_ACL_DEST_INFO(type,value) (((type)<<12)|((value)&0xfff))
#define HPPE_ACL_DEST_TYPE(dest) (((dest)>>12)&0x3)
#define HPPE_ACL_DEST_VALUE(dest) ((dest)&0xfff)

enum{
	HPPE_ACL_ACTION_FWD = 0,
	HPPE_ACL_ACTION_DROP,
	HPPE_ACL_ACTION_COPYCPU,
	HPPE_ACL_ACTION_RDTCPU,
};

enum{
	HPPE_ACL_DEST_INVALID = 0,
	HPPE_ACL_DEST_NEXTHOP,
	HPPE_ACL_DEST_PORT_ID,
	HPPE_ACL_DEST_PORT_BMP,
};

enum {
	HPPE_ACL_TYPE_PORTBITMAP = 0,
	HPPE_ACL_TYPE_PORT,
	HPPE_ACL_TYPE_SERVICE_CODE,
	HPPE_ACL_TYPE_L3_IF,
	APPE_ACL_TYPE_VP_GROUP,
	APPE_ACL_TYPE_SERVICE_PORTBITMAP,
#if defined(JHPPE) || defined(HTTPPE)
	JHPPE_ACL_TYPE_L3_DST_PORT,
	JHPPE_ACL_TYPE_DST_PORT,
#endif
	HPPE_ACL_TYPE_INVALID,
};

typedef struct {
	a_uint32_t rule_type_map; /* rule type map */
	a_uint8_t inverse_rule_type_count[ADPT_ACL_HPPE_RULE_TYPE_NUM]; /* inverse rule type count */
} ADPT_HPPE_ACL_RULE_MAP;

a_uint32_t _acl_bit_index(a_uint32_t bits, a_uint32_t max, a_uint32_t type);

void _adpt_acl_reg_dump(a_uint8_t *reg, a_uint32_t len);

void acl_rule_field_convert(fal_acl_rule_t * rule,
	fal_acl_rule_field_t * rule_field, a_bool_t to_inner);

sw_error_t
_adpt_hppe_acl_rule_sw_2_hw(a_uint32_t dev_id, fal_acl_rule_t *rule, a_uint32_t rule_type,
	a_uint32_t *hw_entry, a_uint32_t *allocated_entries, a_uint8_t *range_en,
	void *hw_rule, void *hw_rule_mask, a_uint8_t inverse_en);

sw_error_t
_adpt_hppe_acl_rule_hw_2_sw(a_uint32_t dev_id, a_uint32_t rule_type,
	a_uint8_t range_en, a_uint8_t inverse_en,
	void * hw_rule, void *hw_rule_mask, fal_acl_rule_t * rule);

a_uint32_t _adpt_hppe_acl_srctype_to_hw(a_uint32_t dev_id, fal_acl_bind_obj_t obj_t);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
/**
 * @}
 */
