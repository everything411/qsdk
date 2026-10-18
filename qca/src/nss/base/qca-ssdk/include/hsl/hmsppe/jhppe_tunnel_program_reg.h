/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_TUNNEL_PROGRAM_REG_H_
#define _JHPPE_TUNNEL_PROGRAM_REG_H_

/*[register] TPR_HDR_MATCH_CTRL*/
#define TPR_HDR_MATCH_CTRL
#define TPR_HDR_MATCH_CTRL_ADDRESS 0x5c0
#define TPR_HDR_MATCH_CTRL_NUM     16
#define TPR_HDR_MATCH_CTRL_INC     0x4
#define TPR_HDR_MATCH_CTRL_TYPE    REG_TYPE_RW
#define TPR_HDR_MATCH_CTRL_DEFAULT 0x0
	/*[field] PROTOCOL_POS_EN*/
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_EN
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_EN_OFFSET  0
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_EN_LEN     1
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_EN_DEFAULT 0x0
	/*[field] PROTOCOL_POS_MODE*/
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_MODE
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_MODE_OFFSET  1
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_MODE_LEN     1
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_MODE_DEFAULT 0x0
	/*[field] PROTOCOL_POS_OFFSET*/
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_OFFSET
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_OFFSET_OFFSET  2
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_OFFSET_LEN     6
	#define TPR_HDR_MATCH_CTRL_PROTOCOL_POS_OFFSET_DEFAULT 0x0

struct tpr_hdr_match_ctrl {
	a_uint32_t  protocol_pos_en:1;
	a_uint32_t  protocol_pos_mode:1;
	a_uint32_t  protocol_pos_offset:6;
	a_uint32_t  _reserved0:24;
};

union tpr_hdr_match_ctrl_u {
	a_uint32_t val;
	struct tpr_hdr_match_ctrl bf;
};

/*[register] TPR_HDR_TUPLE_MATCH*/
#define TPR_HDR_TUPLE_MATCH
#define TPR_HDR_TUPLE_MATCH_ADDRESS 0x600
#define TPR_HDR_TUPLE_MATCH_NUM     16
#define TPR_HDR_TUPLE_MATCH_INC     0x4
#define TPR_HDR_TUPLE_MATCH_TYPE    REG_TYPE_RW
#define TPR_HDR_TUPLE_MATCH_DEFAULT 0x0
	/*[field] TUPLE_ID_INCL*/
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_INCL
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_INCL_OFFSET  0
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_INCL_LEN     1
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_INCL_DEFAULT 0x0
	/*[field] TUPLE_ID*/
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_OFFSET  4
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_LEN     5
	#define TPR_HDR_TUPLE_MATCH_TUPLE_ID_DEFAULT 0x0

struct tpr_hdr_tuple_match {
	a_uint32_t  tuple_id_incl:1;
	a_uint32_t  _reserved0:3;
	a_uint32_t  tuple_id:5;
	a_uint32_t  _reserved1:23;
};

union tpr_hdr_tuple_match_u {
	a_uint32_t val;
	struct tpr_hdr_tuple_match bf;
};

#endif
