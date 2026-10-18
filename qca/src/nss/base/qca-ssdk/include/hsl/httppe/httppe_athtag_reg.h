/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_ATHTAG_REG_H_
#define _HTTPPE_ATHTAG_REG_H_

/*[register] EG_HDR_XMIT_PRI_MAPPING*/
#define EG_HDR_XMIT_PRI_MAPPING
#define EG_HDR_XMIT_PRI_MAPPING_ADDRESS 0xc8
#define EG_HDR_XMIT_PRI_MAPPING_NUM     16
#define EG_HDR_XMIT_PRI_MAPPING_INC     0x4
#define EG_HDR_XMIT_PRI_MAPPING_TYPE    REG_TYPE_RW
#define EG_HDR_XMIT_PRI_MAPPING_DEFAULT 0x0
	/*[field] PRI*/
	#define EG_HDR_XMIT_PRI_MAPPING_PRI
	#define EG_HDR_XMIT_PRI_MAPPING_PRI_OFFSET  0
	#define EG_HDR_XMIT_PRI_MAPPING_PRI_LEN     3
	#define EG_HDR_XMIT_PRI_MAPPING_PRI_DEFAULT 0x0

struct eg_hdr_xmit_pri_mapping {
	a_uint32_t  pri:3;
	a_uint32_t  _reserved0:29;
};

union eg_hdr_xmit_pri_mapping_u {
	a_uint32_t val;
	struct eg_hdr_xmit_pri_mapping bf;
};

/*[register] EG_GEN_CTRL*/
#define EG_GEN_CTRL
#define EG_GEN_CTRL_ADDRESS 0x110
#define EG_GEN_CTRL_NUM     1
#define EG_GEN_CTRL_INC     0x4
#define EG_GEN_CTRL_TYPE    REG_TYPE_RW
#define EG_GEN_CTRL_DEFAULT 0xe00000
	/*[field] ATH_HDR_TYPE*/
	#define EG_GEN_CTRL_ATH_HDR_TYPE
	#define EG_GEN_CTRL_ATH_HDR_TYPE_OFFSET  0
	#define EG_GEN_CTRL_ATH_HDR_TYPE_LEN     16
	#define EG_GEN_CTRL_ATH_HDR_TYPE_DEFAULT 0x0
	/*[field] FLOW_COOKIE_PRI*/
	#define EG_GEN_CTRL_FLOW_COOKIE_PRI
	#define EG_GEN_CTRL_FLOW_COOKIE_PRI_OFFSET  16
	#define EG_GEN_CTRL_FLOW_COOKIE_PRI_LEN     4
	#define EG_GEN_CTRL_FLOW_COOKIE_PRI_DEFAULT 0x0
	/*[field] STRIP_PADDING_EN*/
	#define EG_GEN_CTRL_STRIP_PADDING_EN
	#define EG_GEN_CTRL_STRIP_PADDING_EN_OFFSET  20
	#define EG_GEN_CTRL_STRIP_PADDING_EN_LEN     1
	#define EG_GEN_CTRL_STRIP_PADDING_EN_DEFAULT 0x0
	/*[field] STRIP_PADDING_ROUTE_EN*/
	#define EG_GEN_CTRL_STRIP_PADDING_ROUTE_EN
	#define EG_GEN_CTRL_STRIP_PADDING_ROUTE_EN_OFFSET  21
	#define EG_GEN_CTRL_STRIP_PADDING_ROUTE_EN_LEN     1
	#define EG_GEN_CTRL_STRIP_PADDING_ROUTE_EN_DEFAULT 0x1
	/*[field] STRIP_PADDING_BRIDGE_EN*/
	#define EG_GEN_CTRL_STRIP_PADDING_BRIDGE_EN
	#define EG_GEN_CTRL_STRIP_PADDING_BRIDGE_EN_OFFSET  22
	#define EG_GEN_CTRL_STRIP_PADDING_BRIDGE_EN_LEN     1
	#define EG_GEN_CTRL_STRIP_PADDING_BRIDGE_EN_DEFAULT 0x1
	/*[field] STRIP_INNER_PADDING_EN*/
	#define EG_GEN_CTRL_STRIP_INNER_PADDING_EN
	#define EG_GEN_CTRL_STRIP_INNER_PADDING_EN_OFFSET  23
	#define EG_GEN_CTRL_STRIP_INNER_PADDING_EN_LEN     1
	#define EG_GEN_CTRL_STRIP_INNER_PADDING_EN_DEFAULT 0x1
	/*[field] SNAP_LEN_UPDATE_EN*/
	#define EG_GEN_CTRL_SNAP_LEN_UPDATE_EN
	#define EG_GEN_CTRL_SNAP_LEN_UPDATE_EN_OFFSET  24
	#define EG_GEN_CTRL_SNAP_LEN_UPDATE_EN_LEN     1
	#define EG_GEN_CTRL_SNAP_LEN_UPDATE_EN_DEFAULT 0x0
	/*[field] STRIP_PADDING_DATA_EN*/
	#define EG_GEN_CTRL_STRIP_PADDING_DATA_EN
	#define EG_GEN_CTRL_STRIP_PADDING_DATA_EN_OFFSET  25
	#define EG_GEN_CTRL_STRIP_PADDING_DATA_EN_LEN     1
	#define EG_GEN_CTRL_STRIP_PADDING_DATA_EN_DEFAULT 0x0
	/*[field] SPARE_BIT0*/
	#define EG_GEN_CTRL_SPARE_BIT0
	#define EG_GEN_CTRL_SPARE_BIT0_OFFSET  26
	#define EG_GEN_CTRL_SPARE_BIT0_LEN     1
	#define EG_GEN_CTRL_SPARE_BIT0_DEFAULT 0x0
	/*[field] SPARE_BIT1*/
	#define EG_GEN_CTRL_SPARE_BIT1
	#define EG_GEN_CTRL_SPARE_BIT1_OFFSET  27
	#define EG_GEN_CTRL_SPARE_BIT1_LEN     1
	#define EG_GEN_CTRL_SPARE_BIT1_DEFAULT 0x0
	/*[field] SPARE_BIT2*/
	#define EG_GEN_CTRL_SPARE_BIT2
	#define EG_GEN_CTRL_SPARE_BIT2_OFFSET  28
	#define EG_GEN_CTRL_SPARE_BIT2_LEN     1
	#define EG_GEN_CTRL_SPARE_BIT2_DEFAULT 0x0
	/*[field] SPARE_BIT3*/
	#define EG_GEN_CTRL_SPARE_BIT3
	#define EG_GEN_CTRL_SPARE_BIT3_OFFSET  29
	#define EG_GEN_CTRL_SPARE_BIT3_LEN     1
	#define EG_GEN_CTRL_SPARE_BIT3_DEFAULT 0x0
	/*[field] SPARE_BIT4*/
	#define EG_GEN_CTRL_SPARE_BIT4
	#define EG_GEN_CTRL_SPARE_BIT4_OFFSET  30
	#define EG_GEN_CTRL_SPARE_BIT4_LEN     1
	#define EG_GEN_CTRL_SPARE_BIT4_DEFAULT 0x0
	/*[field] SPARE_BIT5*/
	#define EG_GEN_CTRL_SPARE_BIT5
	#define EG_GEN_CTRL_SPARE_BIT5_OFFSET  31
	#define EG_GEN_CTRL_SPARE_BIT5_LEN     1
	#define EG_GEN_CTRL_SPARE_BIT5_DEFAULT 0x0

struct eg_gen_ctrl {
	a_uint32_t  ath_hdr_type:16;
	a_uint32_t  flow_cookie_pri:4;
	a_uint32_t  strip_padding_en:1;
	a_uint32_t  strip_padding_route_en:1;
	a_uint32_t  strip_padding_bridge_en:1;
	a_uint32_t  strip_inner_padding_en:1;
	a_uint32_t  snap_len_update_en:1;
	a_uint32_t  strip_padding_data_en:1;
	a_uint32_t  spare_bit0:1;
	a_uint32_t  spare_bit1:1;
	a_uint32_t  spare_bit2:1;
	a_uint32_t  spare_bit3:1;
	a_uint32_t  spare_bit4:1;
	a_uint32_t  spare_bit5:1;
};

union eg_gen_ctrl_u {
	a_uint32_t val;
	struct eg_gen_ctrl bf;
};

/*[register] SERVICE_CODE_MAPPING*/
#define SERVICE_CODE_MAPPING
#define SERVICE_CODE_MAPPING_ADDRESS 0x400
#define SERVICE_CODE_MAPPING_NUM     16
#define SERVICE_CODE_MAPPING_INC     0x4
#define SERVICE_CODE_MAPPING_TYPE    REG_TYPE_RW
#define SERVICE_CODE_MAPPING_DEFAULT 0x0
	/*[field] SERVICE_CODE*/
	#define SERVICE_CODE_MAPPING_SERVICE_CODE
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_OFFSET  0
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_LEN     8
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_DEFAULT 0x0
	/*[field] SERVICE_CODE_VALID*/
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_VALID
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_VALID_OFFSET  8
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_VALID_LEN     1
	#define SERVICE_CODE_MAPPING_SERVICE_CODE_VALID_DEFAULT 0x0

struct service_code_mapping {
	a_uint32_t  service_code:8;
	a_uint32_t  service_code_valid:1;
	a_uint32_t  _reserved0:23;
};

union service_code_mapping_u {
	a_uint32_t val;
	struct service_code_mapping bf;
};

/*[register] DST_PORT_MAPPING_TBL*/
#define DST_PORT_MAPPING_TBL
#define DST_PORT_MAPPING_TBL_ADDRESS 0x3800
#define DST_PORT_MAPPING_TBL_NUM     256
#define DST_PORT_MAPPING_TBL_INC     0x4
#define DST_PORT_MAPPING_TBL_TYPE    REG_TYPE_RW
#define DST_PORT_MAPPING_TBL_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define DST_PORT_MAPPING_TBL_DST_INFO
	#define DST_PORT_MAPPING_TBL_DST_INFO_OFFSET  0
	#define DST_PORT_MAPPING_TBL_DST_INFO_LEN     14
	#define DST_PORT_MAPPING_TBL_DST_INFO_DEFAULT 0x0
	/*[field] DST_INFO_VALID*/
	#define DST_PORT_MAPPING_TBL_DST_INFO_VALID
	#define DST_PORT_MAPPING_TBL_DST_INFO_VALID_OFFSET  14
	#define DST_PORT_MAPPING_TBL_DST_INFO_VALID_LEN     1
	#define DST_PORT_MAPPING_TBL_DST_INFO_VALID_DEFAULT 0x0

struct dst_port_mapping_tbl {
	a_uint32_t  dst_info:14;
	a_uint32_t  dst_info_valid:1;
	a_uint32_t  _reserved0:17;
};

union dst_port_mapping_tbl_u {
	a_uint32_t val;
	struct dst_port_mapping_tbl bf;
};
#endif
