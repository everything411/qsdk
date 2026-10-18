/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_PORTCTRL_REG_H_
#define _HTTPPE_PORTCTRL_REG_H_

/*[table] MRU_MTU_CTRL_TBL*/
#define MRU_MTU_CTRL_TBL
#define MRU_MTU_CTRL_TBL_ADDRESS 0x5000
#define MRU_MTU_CTRL_TBL_NUM     256
#define MRU_MTU_CTRL_TBL_INC     0x10
#define MRU_MTU_CTRL_TBL_TYPE    REG_TYPE_RW
#define MRU_MTU_CTRL_TBL_DEFAULT 0x0
	/*[field] MRU*/
	#define MRU_MTU_CTRL_TBL_MRU
	#define MRU_MTU_CTRL_TBL_MRU_OFFSET  0
	#define MRU_MTU_CTRL_TBL_MRU_LEN     14
	#define MRU_MTU_CTRL_TBL_MRU_DEFAULT 0x0
	/*[field] MRU_CMD*/
	#define MRU_MTU_CTRL_TBL_MRU_CMD
	#define MRU_MTU_CTRL_TBL_MRU_CMD_OFFSET  14
	#define MRU_MTU_CTRL_TBL_MRU_CMD_LEN     2
	#define MRU_MTU_CTRL_TBL_MRU_CMD_DEFAULT 0x0
	/*[field] MTU*/
	#define MRU_MTU_CTRL_TBL_MTU
	#define MRU_MTU_CTRL_TBL_MTU_OFFSET  16
	#define MRU_MTU_CTRL_TBL_MTU_LEN     14
	#define MRU_MTU_CTRL_TBL_MTU_DEFAULT 0x0
	/*[field] MTU_CMD*/
	#define MRU_MTU_CTRL_TBL_MTU_CMD
	#define MRU_MTU_CTRL_TBL_MTU_CMD_OFFSET  30
	#define MRU_MTU_CTRL_TBL_MTU_CMD_LEN     2
	#define MRU_MTU_CTRL_TBL_MTU_CMD_DEFAULT 0x0
	/*[field] RX_CNT_EN*/
	#define MRU_MTU_CTRL_TBL_RX_CNT_EN
	#define MRU_MTU_CTRL_TBL_RX_CNT_EN_OFFSET  32
	#define MRU_MTU_CTRL_TBL_RX_CNT_EN_LEN     1
	#define MRU_MTU_CTRL_TBL_RX_CNT_EN_DEFAULT 0x0
	/*[field] TX_CNT_EN*/
	#define MRU_MTU_CTRL_TBL_TX_CNT_EN
	#define MRU_MTU_CTRL_TBL_TX_CNT_EN_OFFSET  33
	#define MRU_MTU_CTRL_TBL_TX_CNT_EN_LEN     1
	#define MRU_MTU_CTRL_TBL_TX_CNT_EN_DEFAULT 0x0
	/*[field] SRC_PROFILE*/
	#define MRU_MTU_CTRL_TBL_SRC_PROFILE
	#define MRU_MTU_CTRL_TBL_SRC_PROFILE_OFFSET  34
	#define MRU_MTU_CTRL_TBL_SRC_PROFILE_LEN     2
	#define MRU_MTU_CTRL_TBL_SRC_PROFILE_DEFAULT 0x0
	/*[field] PCP_QOS_GROUP_ID*/
	#define MRU_MTU_CTRL_TBL_PCP_QOS_GROUP_ID
	#define MRU_MTU_CTRL_TBL_PCP_QOS_GROUP_ID_OFFSET  36
	#define MRU_MTU_CTRL_TBL_PCP_QOS_GROUP_ID_LEN     1
	#define MRU_MTU_CTRL_TBL_PCP_QOS_GROUP_ID_DEFAULT 0x0
	/*[field] DSCP_QOS_GROUP_ID*/
	#define MRU_MTU_CTRL_TBL_DSCP_QOS_GROUP_ID
	#define MRU_MTU_CTRL_TBL_DSCP_QOS_GROUP_ID_OFFSET  37
	#define MRU_MTU_CTRL_TBL_DSCP_QOS_GROUP_ID_LEN     1
	#define MRU_MTU_CTRL_TBL_DSCP_QOS_GROUP_ID_DEFAULT 0x0
	/*[field] PCP_RES_PREC_FORCE*/
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_FORCE
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_FORCE_OFFSET  38
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_FORCE_LEN     1
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_FORCE_DEFAULT 0x0
	/*[field] DSCP_RES_PREC_FORCE*/
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_FORCE
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_FORCE_OFFSET  39
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_FORCE_LEN     1
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_FORCE_DEFAULT 0x0
	/*[field] PREHEADER_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_PREHEADER_RES_PREC
	#define MRU_MTU_CTRL_TBL_PREHEADER_RES_PREC_OFFSET  40
	#define MRU_MTU_CTRL_TBL_PREHEADER_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_PREHEADER_RES_PREC_DEFAULT 0x0
	/*[field] PCP_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_OFFSET  43
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_PCP_RES_PREC_DEFAULT 0x0
	/*[field] DSCP_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_OFFSET  46
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_DSCP_RES_PREC_DEFAULT 0x0
	/*[field] FLOW_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_FLOW_RES_PREC
	#define MRU_MTU_CTRL_TBL_FLOW_RES_PREC_OFFSET  49
	#define MRU_MTU_CTRL_TBL_FLOW_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_FLOW_RES_PREC_DEFAULT 0x0
	/*[field] PRE_ACL_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_PRE_ACL_RES_PREC
	#define MRU_MTU_CTRL_TBL_PRE_ACL_RES_PREC_OFFSET  52
	#define MRU_MTU_CTRL_TBL_PRE_ACL_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_PRE_ACL_RES_PREC_DEFAULT 0x0
	/*[field] POST_ACL_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_POST_ACL_RES_PREC
	#define MRU_MTU_CTRL_TBL_POST_ACL_RES_PREC_OFFSET  55
	#define MRU_MTU_CTRL_TBL_POST_ACL_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_POST_ACL_RES_PREC_DEFAULT 0x0
	/*[field] SOURCE_FILTERING_BYPASS*/
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_BYPASS
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_BYPASS_OFFSET  58
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_BYPASS_LEN     1
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_BYPASS_DEFAULT 0x0
	/*[field] SOURCE_FILTERING_MODE*/
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_MODE
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_MODE_OFFSET  59
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_MODE_LEN     1
	#define MRU_MTU_CTRL_TBL_SOURCE_FILTERING_MODE_DEFAULT 0x0
	/*[field] PRE_IPO_OUTER_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_PRE_IPO_OUTER_RES_PREC
	#define MRU_MTU_CTRL_TBL_PRE_IPO_OUTER_RES_PREC_OFFSET  60
	#define MRU_MTU_CTRL_TBL_PRE_IPO_OUTER_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_PRE_IPO_OUTER_RES_PREC_DEFAULT 0x0
	/*[field] PRE_IPO_INNER_RES_PREC*/
	#define MRU_MTU_CTRL_TBL_PRE_IPO_INNER_RES_PREC
	#define MRU_MTU_CTRL_TBL_PRE_IPO_INNER_RES_PREC_OFFSET  63
	#define MRU_MTU_CTRL_TBL_PRE_IPO_INNER_RES_PREC_LEN     3
	#define MRU_MTU_CTRL_TBL_PRE_IPO_INNER_RES_PREC_DEFAULT 0x0
	/*[field] PCP_QOS_MODE*/
	#define MRU_MTU_CTRL_TBL_PCP_QOS_MODE
	#define MRU_MTU_CTRL_TBL_PCP_QOS_MODE_OFFSET  66
	#define MRU_MTU_CTRL_TBL_PCP_QOS_MODE_LEN     1
	#define MRU_MTU_CTRL_TBL_PCP_QOS_MODE_DEFAULT 0x0
	/*[field] DEFAULT_PCP_DEI*/
	#define MRU_MTU_CTRL_TBL_DEFAULT_PCP_DEI
	#define MRU_MTU_CTRL_TBL_DEFAULT_PCP_DEI_OFFSET  67
	#define MRU_MTU_CTRL_TBL_DEFAULT_PCP_DEI_LEN     4
	#define MRU_MTU_CTRL_TBL_DEFAULT_PCP_DEI_DEFAULT 0x0

struct mru_mtu_ctrl_tbl {
	a_uint32_t  mru:14;
	a_uint32_t  mru_cmd:2;
	a_uint32_t  mtu:14;
	a_uint32_t  mtu_cmd:2;
	a_uint32_t  rx_cnt_en:1;
	a_uint32_t  tx_cnt_en:1;
	a_uint32_t  src_profile:2;
	a_uint32_t  pcp_qos_group_id:1;
	a_uint32_t  dscp_qos_group_id:1;
	a_uint32_t  pcp_res_prec_force:1;
	a_uint32_t  dscp_res_prec_force:1;
	a_uint32_t  preheader_res_prec:3;
	a_uint32_t  pcp_res_prec:3;
	a_uint32_t  dscp_res_prec:3;
	a_uint32_t  flow_res_prec:3;
	a_uint32_t  pre_acl_res_prec:3;
	a_uint32_t  post_acl_res_prec:3;
	a_uint32_t  source_filtering_bypass:1;
	a_uint32_t  source_filtering_mode:1;
	a_uint32_t  pre_ipo_outer_res_prec:3;
	a_uint32_t  pre_ipo_inner_res_prec_0:1;
	a_uint32_t  pre_ipo_inner_res_prec_1:2;
	a_uint32_t  pcp_qos_mode:1;
	a_uint32_t  default_pcp_dei:4;
	a_uint32_t  _reserved0:25;
};

union mru_mtu_ctrl_tbl_u {
	a_uint32_t val[3];
	struct mru_mtu_ctrl_tbl bf;
};

/*[table] PORT_TX_COUNTER_TBL_REG*/
#define PORT_TX_COUNTER_TBL_REG
#define PORT_TX_COUNTER_TBL_REG_ADDRESS 0x14000
#define PORT_TX_COUNTER_TBL_REG_NUM     9
#define PORT_TX_COUNTER_TBL_REG_INC     0x10
#define PORT_TX_COUNTER_TBL_REG_TYPE    REG_TYPE_RW
#define PORT_TX_COUNTER_TBL_REG_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define PORT_TX_COUNTER_TBL_REG_TX_PACKETS
	#define PORT_TX_COUNTER_TBL_REG_TX_PACKETS_OFFSET  0
	#define PORT_TX_COUNTER_TBL_REG_TX_PACKETS_LEN     32
	#define PORT_TX_COUNTER_TBL_REG_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define PORT_TX_COUNTER_TBL_REG_TX_BYTES
	#define PORT_TX_COUNTER_TBL_REG_TX_BYTES_OFFSET  32
	#define PORT_TX_COUNTER_TBL_REG_TX_BYTES_LEN     40
	#define PORT_TX_COUNTER_TBL_REG_TX_BYTES_DEFAULT 0x0

struct port_tx_counter_tbl_reg {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union port_tx_counter_tbl_reg_u {
	a_uint32_t val[3];
	struct port_tx_counter_tbl_reg bf;
};

/*[table] VP_TX_COUNTER_TBL_REG*/
#define VP_TX_COUNTER_TBL_REG
#define VP_TX_COUNTER_TBL_REG_ADDRESS 0x16000
#define VP_TX_COUNTER_TBL_REG_NUM     256
#define VP_TX_COUNTER_TBL_REG_INC     0x10
#define VP_TX_COUNTER_TBL_REG_TYPE    REG_TYPE_RW
#define VP_TX_COUNTER_TBL_REG_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define VP_TX_COUNTER_TBL_REG_TX_PACKETS
	#define VP_TX_COUNTER_TBL_REG_TX_PACKETS_OFFSET  0
	#define VP_TX_COUNTER_TBL_REG_TX_PACKETS_LEN     32
	#define VP_TX_COUNTER_TBL_REG_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define VP_TX_COUNTER_TBL_REG_TX_BYTES
	#define VP_TX_COUNTER_TBL_REG_TX_BYTES_OFFSET  32
	#define VP_TX_COUNTER_TBL_REG_TX_BYTES_LEN     40
	#define VP_TX_COUNTER_TBL_REG_TX_BYTES_DEFAULT 0x0

struct vp_tx_counter_tbl_reg {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union vp_tx_counter_tbl_reg_u {
	a_uint32_t val[3];
	struct vp_tx_counter_tbl_reg bf;
};

#endif
