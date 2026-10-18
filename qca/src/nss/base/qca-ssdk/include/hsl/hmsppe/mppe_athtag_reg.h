/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MPPE_ATHTAG_REG_H_
#define _MPPE_ATHTAG_REG_H_

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

/*[register] EDMA_VP_REMAP_0*/
#define EDMA_VP_REMAP_0
#define EDMA_VP_REMAP_0_ADDRESS 0x40
#define EDMA_VP_REMAP_0_NUM     1
#define EDMA_VP_REMAP_0_INC     0x4
#define EDMA_VP_REMAP_0_TYPE    REG_TYPE_RW
#define EDMA_VP_REMAP_0_DEFAULT 0x76543210
	/*[field] REMAP_VP_0*/
	#define EDMA_VP_REMAP_0_REMAP_VP_0
	#define EDMA_VP_REMAP_0_REMAP_VP_0_OFFSET  0
	#define EDMA_VP_REMAP_0_REMAP_VP_0_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_0_DEFAULT 0x0
	/*[field] REMAP_VP_1*/
	#define EDMA_VP_REMAP_0_REMAP_VP_1
	#define EDMA_VP_REMAP_0_REMAP_VP_1_OFFSET  4
	#define EDMA_VP_REMAP_0_REMAP_VP_1_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_1_DEFAULT 0x1
	/*[field] REMAP_VP_2*/
	#define EDMA_VP_REMAP_0_REMAP_VP_2
	#define EDMA_VP_REMAP_0_REMAP_VP_2_OFFSET  8
	#define EDMA_VP_REMAP_0_REMAP_VP_2_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_2_DEFAULT 0x2
	/*[field] REMAP_VP_3*/
	#define EDMA_VP_REMAP_0_REMAP_VP_3
	#define EDMA_VP_REMAP_0_REMAP_VP_3_OFFSET  12
	#define EDMA_VP_REMAP_0_REMAP_VP_3_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_3_DEFAULT 0x3
	/*[field] REMAP_VP_4*/
	#define EDMA_VP_REMAP_0_REMAP_VP_4
	#define EDMA_VP_REMAP_0_REMAP_VP_4_OFFSET  16
	#define EDMA_VP_REMAP_0_REMAP_VP_4_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_4_DEFAULT 0x4
	/*[field] REMAP_VP_5*/
	#define EDMA_VP_REMAP_0_REMAP_VP_5
	#define EDMA_VP_REMAP_0_REMAP_VP_5_OFFSET  20
	#define EDMA_VP_REMAP_0_REMAP_VP_5_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_5_DEFAULT 0x5
	/*[field] REMAP_VP_6*/
	#define EDMA_VP_REMAP_0_REMAP_VP_6
	#define EDMA_VP_REMAP_0_REMAP_VP_6_OFFSET  24
	#define EDMA_VP_REMAP_0_REMAP_VP_6_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_6_DEFAULT 0x6
	/*[field] REMAP_VP_7*/
	#define EDMA_VP_REMAP_0_REMAP_VP_7
	#define EDMA_VP_REMAP_0_REMAP_VP_7_OFFSET  28
	#define EDMA_VP_REMAP_0_REMAP_VP_7_LEN     4
	#define EDMA_VP_REMAP_0_REMAP_VP_7_DEFAULT 0x7

struct edma_vp_remap_0 {
	a_uint32_t  remap_vp_0:4;
	a_uint32_t  remap_vp_1:4;
	a_uint32_t  remap_vp_2:4;
	a_uint32_t  remap_vp_3:4;
	a_uint32_t  remap_vp_4:4;
	a_uint32_t  remap_vp_5:4;
	a_uint32_t  remap_vp_6:4;
	a_uint32_t  remap_vp_7:4;
};

union edma_vp_remap_0_u {
	a_uint32_t val;
	struct edma_vp_remap_0 bf;
};

/*[register] EDMA_VP_REMAP_1*/
#define EDMA_VP_REMAP_1
#define EDMA_VP_REMAP_1_ADDRESS 0x44
#define EDMA_VP_REMAP_1_NUM     1
#define EDMA_VP_REMAP_1_INC     0x4
#define EDMA_VP_REMAP_1_TYPE    REG_TYPE_RW
#define EDMA_VP_REMAP_1_DEFAULT 0x0
	/*[field] REMAP_VP_8*/
	#define EDMA_VP_REMAP_1_REMAP_VP_8
	#define EDMA_VP_REMAP_1_REMAP_VP_8_OFFSET  0
	#define EDMA_VP_REMAP_1_REMAP_VP_8_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_8_DEFAULT 0x0
	/*[field] REMAP_VP_9*/
	#define EDMA_VP_REMAP_1_REMAP_VP_9
	#define EDMA_VP_REMAP_1_REMAP_VP_9_OFFSET  4
	#define EDMA_VP_REMAP_1_REMAP_VP_9_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_9_DEFAULT 0x0
	/*[field] REMAP_VP_10*/
	#define EDMA_VP_REMAP_1_REMAP_VP_10
	#define EDMA_VP_REMAP_1_REMAP_VP_10_OFFSET  8
	#define EDMA_VP_REMAP_1_REMAP_VP_10_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_10_DEFAULT 0x0
	/*[field] REMAP_VP_11*/
	#define EDMA_VP_REMAP_1_REMAP_VP_11
	#define EDMA_VP_REMAP_1_REMAP_VP_11_OFFSET  12
	#define EDMA_VP_REMAP_1_REMAP_VP_11_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_11_DEFAULT 0x0
	/*[field] REMAP_VP_12*/
	#define EDMA_VP_REMAP_1_REMAP_VP_12
	#define EDMA_VP_REMAP_1_REMAP_VP_12_OFFSET  16
	#define EDMA_VP_REMAP_1_REMAP_VP_12_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_12_DEFAULT 0x0
	/*[field] REMAP_VP_13*/
	#define EDMA_VP_REMAP_1_REMAP_VP_13
	#define EDMA_VP_REMAP_1_REMAP_VP_13_OFFSET  20
	#define EDMA_VP_REMAP_1_REMAP_VP_13_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_13_DEFAULT 0x0
	/*[field] REMAP_VP_14*/
	#define EDMA_VP_REMAP_1_REMAP_VP_14
	#define EDMA_VP_REMAP_1_REMAP_VP_14_OFFSET  24
	#define EDMA_VP_REMAP_1_REMAP_VP_14_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_14_DEFAULT 0x0
	/*[field] REMAP_VP_15*/
	#define EDMA_VP_REMAP_1_REMAP_VP_15
	#define EDMA_VP_REMAP_1_REMAP_VP_15_OFFSET  28
	#define EDMA_VP_REMAP_1_REMAP_VP_15_LEN     4
	#define EDMA_VP_REMAP_1_REMAP_VP_15_DEFAULT 0x0

struct edma_vp_remap_1 {
	a_uint32_t  remap_vp_8:4;
	a_uint32_t  remap_vp_9:4;
	a_uint32_t  remap_vp_10:4;
	a_uint32_t  remap_vp_11:4;
	a_uint32_t  remap_vp_12:4;
	a_uint32_t  remap_vp_13:4;
	a_uint32_t  remap_vp_14:4;
	a_uint32_t  remap_vp_15:4;
};

union edma_vp_remap_1_u {
	a_uint32_t val;
	struct edma_vp_remap_1 bf;
};

/*[register] PRX_PORT_TO_VP_MAPPING*/
#define PRX_PORT_TO_VP_MAPPING
#define PRX_PORT_TO_VP_MAPPING_ADDRESS 0x90
#define PRX_PORT_TO_VP_MAPPING_NUM     8
#define PRX_PORT_TO_VP_MAPPING_INC     0x4
#define PRX_PORT_TO_VP_MAPPING_TYPE    REG_TYPE_RW
#define PRX_PORT_TO_VP_MAPPING_DEFAULT 0x0
	/*[field] ATHEROS_HDR_TYPE*/
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_TYPE
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_TYPE_OFFSET  0
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_TYPE_LEN     16
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_TYPE_DEFAULT 0x0
	/*[field] PORT_VP*/
	#define PRX_PORT_TO_VP_MAPPING_PORT_VP
	#define PRX_PORT_TO_VP_MAPPING_PORT_VP_OFFSET  16
	#define PRX_PORT_TO_VP_MAPPING_PORT_VP_LEN     8
	#define PRX_PORT_TO_VP_MAPPING_PORT_VP_DEFAULT 0x0
	/*[field] ATHEROS_HDR_EN*/
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_EN
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_EN_OFFSET  24
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_EN_LEN     1
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_EN_DEFAULT 0x0
	/*[field] ATHEROS_HDR_VER*/
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_VER
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_VER_OFFSET  25
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_VER_LEN     1
	#define PRX_PORT_TO_VP_MAPPING_ATHEROS_HDR_VER_DEFAULT 0x0

struct prx_port_to_vp_mapping {
	a_uint32_t  atheros_hdr_type:16;
	a_uint32_t  port_vp:8;
	a_uint32_t  atheros_hdr_en:1;
	a_uint32_t  atheros_hdr_ver:1;
	a_uint32_t  _reserved0:6;
};

union prx_port_to_vp_mapping_u {
	a_uint32_t val;
	struct prx_port_to_vp_mapping bf;
};

/*[register] PRX_HDR_RCV_PRI_MAPPING*/
#define PRX_HDR_RCV_PRI_MAPPING
#define PRX_HDR_RCV_PRI_MAPPING_ADDRESS 0xb0
#define PRX_HDR_RCV_PRI_MAPPING_NUM     16
#define PRX_HDR_RCV_PRI_MAPPING_INC     0x4
#define PRX_HDR_RCV_PRI_MAPPING_TYPE    REG_TYPE_RW
#define PRX_HDR_RCV_PRI_MAPPING_DEFAULT 0x0
	/*[field] INT_PRI*/
	#define PRX_HDR_RCV_PRI_MAPPING_INT_PRI
	#define PRX_HDR_RCV_PRI_MAPPING_INT_PRI_OFFSET  0
	#define PRX_HDR_RCV_PRI_MAPPING_INT_PRI_LEN     4
	#define PRX_HDR_RCV_PRI_MAPPING_INT_PRI_DEFAULT 0x0

struct prx_hdr_rcv_pri_mapping {
	a_uint32_t  int_pri:4;
	a_uint32_t  _reserved0:28;
};

union prx_hdr_rcv_pri_mapping_u {
	a_uint32_t val;
	struct prx_hdr_rcv_pri_mapping bf;
};

#endif
