/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MRPPE_PKTEDIT_REG_H_
#define _MRPPE_PKTEDIT_REG_H_

/*[register] IPR_SPARE_REG*/
#define IPR_SPARE_REG
#define IPR_SPARE_REG_ADDRESS 0x74
#define IPR_SPARE_REG_NUM     1
#define IPR_SPARE_REG_INC     0x4
#define IPR_SPARE_REG_TYPE    REG_TYPE_RW
#define IPR_SPARE_REG_DEFAULT 0x0
	/*[field] INNER_IP_LEN_UPDATE_EN*/
	#define IPR_SPARE_REG_INNER_IP_LEN_UPDATE_EN
	#define IPR_SPARE_REG_INNER_IP_LEN_UPDATE_EN_OFFSET  0
	#define IPR_SPARE_REG_INNER_IP_LEN_UPDATE_EN_LEN     1
	#define IPR_SPARE_REG_INNER_IP_LEN_UPDATE_EN_DEFAULT 0x0
	/*[field] INNER_IP_LEN_GAP_EXP_EN*/
	#define IPR_SPARE_REG_INNER_IP_LEN_GAP_EXP_EN
	#define IPR_SPARE_REG_INNER_IP_LEN_GAP_EXP_EN_OFFSET  1
	#define IPR_SPARE_REG_INNER_IP_LEN_GAP_EXP_EN_LEN     1
	#define IPR_SPARE_REG_INNER_IP_LEN_GAP_EXP_EN_DEFAULT 0x0
	/*[field] INNER_IP_PADDING_EXP_EN*/
	#define IPR_SPARE_REG_INNER_IP_PADDING_EXP_EN
	#define IPR_SPARE_REG_INNER_IP_PADDING_EXP_EN_OFFSET  2
	#define IPR_SPARE_REG_INNER_IP_PADDING_EXP_EN_LEN     1
	#define IPR_SPARE_REG_INNER_IP_PADDING_EXP_EN_DEFAULT 0x0
	/*[field] PAYLOAD_CHECKSUM_STRIP_PADDING_EN*/
	#define IPR_SPARE_REG_PAYLOAD_CHECKSUM_STRIP_PADDING_EN
	#define IPR_SPARE_REG_PAYLOAD_CHECKSUM_STRIP_PADDING_EN_OFFSET  3
	#define IPR_SPARE_REG_PAYLOAD_CHECKSUM_STRIP_PADDING_EN_LEN     1
	#define IPR_SPARE_REG_PAYLOAD_CHECKSUM_STRIP_PADDING_EN_DEFAULT 0x0
	/*[field] SPARE_REG*/
	#define IPR_SPARE_REG_SPARE_REG
	#define IPR_SPARE_REG_SPARE_REG_OFFSET  4
	#define IPR_SPARE_REG_SPARE_REG_LEN     28
	#define IPR_SPARE_REG_SPARE_REG_DEFAULT 0x0

struct ipr_spare_reg {
	a_uint32_t  inner_ip_len_update_en:1;
	a_uint32_t  inner_ip_len_gap_exp_en:1;
	a_uint32_t  inner_ip_padding_exp_en:1;
	a_uint32_t  payload_checksum_strip_padding_en:1;
	a_uint32_t  spare_reg:28;
};

union ipr_spare_reg_u {
	a_uint32_t val;
	struct ipr_spare_reg bf;
};

#endif
