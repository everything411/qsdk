/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _HTTPPE_MISC_H_
#define _HTTPPE_MISC_H_

/* IPO_RULE_REG */
#define IPO_RULE_REG
#define IPO_RULE_REG_ADDRESS 0x0
#define IPO_RULE_REG_NUM     128
#define IPO_RULE_REG_INC     0x10
#define IPO_RULE_REG_TYPE    REG_TYPE_RW
#define IPO_RULE_REG_DEFAULT 0x0

#define IPO_MASK_REG
#define IPO_MASK_REG_ADDRESS 0x2000
#define IPO_MASK_REG_NUM     128
#define IPO_MASK_REG_INC     0x10
#define IPO_MASK_REG_TYPE    REG_TYPE_RW
#define IPO_MASK_REG_DEFAULT 0x0

#define IPO_RULE_REG_SRC_OFFSET  63

struct ipo_rule_reg {
	a_uint32_t rule_field_0:32;
	a_uint32_t rule_field_1:20;
	a_uint32_t fake_mac_header:1;
	a_uint32_t range_en:1;
	a_uint32_t inverse_en:1;
	a_uint32_t rule_type:5;
	a_uint32_t src_type:3;
	a_uint32_t src_0:1;
	a_uint32_t src_1:8;
	a_uint32_t pri:9;
	a_uint32_t res_chain:1;
	a_uint32_t post_routing_en:1;
	a_uint32_t _reserved0:13;
};

union ipo_rule_reg_u {
	a_uint32_t val[3];
	struct ipo_rule_reg bf;
};

struct ipo_mask_reg {
	a_uint32_t  maskfield_0:32;
	a_uint32_t  maskfield_1:21;
	a_uint32_t  _reserved0:11;
};

union ipo_mask_reg_u {
	a_uint32_t val[2];
	struct ipo_mask_reg bf;
};

sw_error_t
httppe_ipo_rule_reg_get(
	a_uint32_t dev_id,
	a_uint32_t index,
	union ipo_rule_reg_u *value);

sw_error_t
httppe_ipo_rule_reg_set(
	a_uint32_t dev_id,
	a_uint32_t index,
	union ipo_rule_reg_u *value);

sw_error_t
httppe_ipo_mask_reg_get(
	a_uint32_t dev_id,
	a_uint32_t index,
	union ipo_mask_reg_u *value);

sw_error_t
httppe_ipo_mask_reg_set(
	a_uint32_t dev_id,
	a_uint32_t index,
	union ipo_mask_reg_u *value);

#endif
