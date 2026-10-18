/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_PORTCTRL_REG_H_
#define _HPPE_PORTCTRL_REG_H_

/*[register] IPR_PKT_NUM_TBL_REG*/
#define IPR_PKT_NUM_TBL_REG
#define IPR_PKT_NUM_TBL_REG_ADDRESS 0x80
#define IPR_PKT_NUM_TBL_REG_NUM     9
#define IPR_PKT_NUM_TBL_REG_INC     0x4
#define IPR_PKT_NUM_TBL_REG_TYPE    REG_TYPE_RO
#define IPR_PKT_NUM_TBL_REG_DEFAULT 0x0
	/*[field] IPR_PKT_NUM_TBL_REG*/
	#define IPR_PKT_NUM_TBL_REG_IPR_PKT_NUM
	#define IPR_PKT_NUM_TBL_REG_IPR_PKT_NUM_OFFSET  0
	#define IPR_PKT_NUM_TBL_REG_IPR_PKT_NUM_LEN     32
	#define IPR_PKT_NUM_TBL_REG_IPR_PKT_NUM_DEFAULT 0x0

struct ipr_pkt_num_tbl_reg {
	a_uint32_t  ipr_pkt_num_tbl_reg:32;
};

union ipr_pkt_num_tbl_reg_u {
	a_uint32_t val;
	struct ipr_pkt_num_tbl_reg bf;
};

/*[register] IPR_BYTE_LOW_REG_REG*/
#define IPR_BYTE_LOW_REG_REG
#define IPR_BYTE_LOW_REG_REG_ADDRESS 0xc0
#define IPR_BYTE_LOW_REG_REG_NUM     9
#define IPR_BYTE_LOW_REG_REG_INC     0x4
#define IPR_BYTE_LOW_REG_REG_TYPE    REG_TYPE_RO
#define IPR_BYTE_LOW_REG_REG_DEFAULT 0x0
	/*[field] IPR_BYTE_LOW_REG_REG*/
	#define IPR_BYTE_LOW_REG_REG_IPR_BYTE_LOW
	#define IPR_BYTE_LOW_REG_REG_IPR_BYTE_LOW_OFFSET  0
	#define IPR_BYTE_LOW_REG_REG_IPR_BYTE_LOW_LEN     32
	#define IPR_BYTE_LOW_REG_REG_IPR_BYTE_LOW_DEFAULT 0x0

struct ipr_byte_low_reg_reg {
	a_uint32_t  ipr_byte_low_reg_reg:32;
};

union ipr_byte_low_reg_reg_u {
	a_uint32_t val;
	struct ipr_byte_low_reg_reg bf;
};

/*[register] IPR_BYTE_HIGH_REG*/
#define IPR_BYTE_HIGH_REG
#define IPR_BYTE_HIGH_REG_ADDRESS 0x100
#define IPR_BYTE_HIGH_REG_NUM     9
#define IPR_BYTE_HIGH_REG_INC     0x4
#define IPR_BYTE_HIGH_REG_TYPE    REG_TYPE_RW
#define IPR_BYTE_HIGH_REG_DEFAULT 0x0
	/*[field] IPR_BYTE_HIGH_REG*/
	#define IPR_BYTE_HIGH_REG_IPR_BYTE_HIGH
	#define IPR_BYTE_HIGH_REG_IPR_BYTE_HIGH_OFFSET  0
	#define IPR_BYTE_HIGH_REG_IPR_BYTE_HIGH_LEN     8
	#define IPR_BYTE_HIGH_REG_IPR_BYTE_HIGH_DEFAULT 0x0
	/*[field] CLEAR*/
	#define IPR_BYTE_HIGH_REG_CLEAR
	#define IPR_BYTE_HIGH_REG_CLEAR_OFFSET  8
	#define IPR_BYTE_HIGH_REG_CLEAR_LEN     1
	#define IPR_BYTE_HIGH_REG_CLEAR_DEFAULT 0x0

struct ipr_byte_high_reg {
	a_uint32_t  ipr_byte_high_reg:8;
	a_uint32_t  clear:1;
	a_uint32_t  _reserved0:23;
};

union ipr_byte_high_reg_u {
	a_uint32_t val;
	struct ipr_byte_high_reg bf;
};

/*[register] PORT_IN_FORWARD*/
#define PORT_IN_FORWARD
#define PORT_IN_FORWARD_ADDRESS 0x700
#define PORT_IN_FORWARD_NUM     9
#define PORT_IN_FORWARD_INC     0x4
#define PORT_IN_FORWARD_TYPE    REG_TYPE_RW
#define PORT_IN_FORWARD_DEFAULT 0x0
	/*[field] SOURCE_FILTERING_BYPASS*/
	#define PORT_IN_FORWARD_SOURCE_FILTERING_BYPASS
	#define PORT_IN_FORWARD_SOURCE_FILTERING_BYPASS_OFFSET  0
	#define PORT_IN_FORWARD_SOURCE_FILTERING_BYPASS_LEN     1
	#define PORT_IN_FORWARD_SOURCE_FILTERING_BYPASS_DEFAULT 0x0

struct port_in_forward {
	a_uint32_t  source_filtering_bypass:1;
	a_uint32_t  _reserved0:31;
};

union port_in_forward_u {
	a_uint32_t val;
	struct port_in_forward bf;
};

/*[register] MC_MTU_CTRL_TBL*/
#define MC_MTU_CTRL_TBL
#define MC_MTU_CTRL_TBL_ADDRESS 0xa00
#define MC_MTU_CTRL_TBL_NUM     9
#define MC_MTU_CTRL_TBL_INC     0x4
#define MC_MTU_CTRL_TBL_TYPE    REG_TYPE_RW
#define MC_MTU_CTRL_TBL_DEFAULT 0x5ea
	/*[field] MTU*/
	#define MC_MTU_CTRL_TBL_MTU
	#define MC_MTU_CTRL_TBL_MTU_OFFSET  0
	#define MC_MTU_CTRL_TBL_MTU_LEN     14
	#define MC_MTU_CTRL_TBL_MTU_DEFAULT 0x5ea
	/*[field] MTU_CMD*/
	#define MC_MTU_CTRL_TBL_MTU_CMD
	#define MC_MTU_CTRL_TBL_MTU_CMD_OFFSET  14
	#define MC_MTU_CTRL_TBL_MTU_CMD_LEN     2
	#define MC_MTU_CTRL_TBL_MTU_CMD_DEFAULT 0x0
	/*[field] TX_CNT_EN*/
	#define MC_MTU_CTRL_TBL_TX_CNT_EN
	#define MC_MTU_CTRL_TBL_TX_CNT_EN_OFFSET  16
	#define MC_MTU_CTRL_TBL_TX_CNT_EN_LEN     1
	#define MC_MTU_CTRL_TBL_TX_CNT_EN_DEFAULT 0x0

struct mc_mtu_ctrl_tbl {
	a_uint32_t  mtu:14;
	a_uint32_t  mtu_cmd:2;
	a_uint32_t  tx_cnt_en:1;
	a_uint32_t  _reserved0:15;
};

union mc_mtu_ctrl_tbl_u {
	a_uint32_t val;
	struct mc_mtu_ctrl_tbl bf;
};

/*[register] LINK_OAM_CTRL*/
#define LINK_OAM_CTRL
#define LINK_OAM_CTRL_ADDRESS 0x1800
#define LINK_OAM_CTRL_NUM     9
#define LINK_OAM_CTRL_INC     0x4
#define LINK_OAM_CTRL_TYPE    REG_TYPE_RW
#define LINK_OAM_CTRL_DEFAULT 0x0
	/*[field] LOOPBACK_STATE*/
	#define LINK_OAM_CTRL_LOOPBACK_STATE
	#define LINK_OAM_CTRL_LOOPBACK_STATE_OFFSET  0
	#define LINK_OAM_CTRL_LOOPBACK_STATE_LEN     1
	#define LINK_OAM_CTRL_LOOPBACK_STATE_DEFAULT 0x0

struct link_oam_ctrl {
	a_uint32_t  loopback_state:1;
	a_uint32_t  _reserved0:31;
};

union link_oam_ctrl_u {
	a_uint32_t val;
	struct link_oam_ctrl bf;
};

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

/*[register] MAC_ENABLE*/
#define MAC_ENABLE
#define MAC_ENABLE_ADDRESS 0x0
#define MAC_ENABLE_NUM     6
#define MAC_ENABLE_INC     0x200
#define MAC_ENABLE_TYPE    REG_TYPE_RW
#define MAC_ENABLE_DEFAULT 0x10
	/*[field] RXMAC_EN*/
	#define MAC_ENABLE_RXMAC_EN
	#define MAC_ENABLE_RXMAC_EN_OFFSET  0
	#define MAC_ENABLE_RXMAC_EN_LEN     1
	#define MAC_ENABLE_RXMAC_EN_DEFAULT 0x0
	/*[field] TXMAC_EN*/
	#define MAC_ENABLE_TXMAC_EN
	#define MAC_ENABLE_TXMAC_EN_OFFSET  1
	#define MAC_ENABLE_TXMAC_EN_LEN     1
	#define MAC_ENABLE_TXMAC_EN_DEFAULT 0x0
	/*[field] DUPLEX*/
	#define MAC_ENABLE_DUPLEX
	#define MAC_ENABLE_DUPLEX_OFFSET  4
	#define MAC_ENABLE_DUPLEX_LEN     1
	#define MAC_ENABLE_DUPLEX_DEFAULT 0x1
	/*[field] RX_FLOW_EN*/
	#define MAC_ENABLE_RX_FLOW_EN
	#define MAC_ENABLE_RX_FLOW_EN_OFFSET  5
	#define MAC_ENABLE_RX_FLOW_EN_LEN     1
	#define MAC_ENABLE_RX_FLOW_EN_DEFAULT 0x0
	/*[field] TX_FLOW_EN*/
	#define MAC_ENABLE_TX_FLOW_EN
	#define MAC_ENABLE_TX_FLOW_EN_OFFSET  6
	#define MAC_ENABLE_TX_FLOW_EN_LEN     1
	#define MAC_ENABLE_TX_FLOW_EN_DEFAULT 0x0

struct mac_enable {
	a_uint32_t  rxmac_en:1;
	a_uint32_t  txmac_en:1;
	a_uint32_t  _reserved0:2;
	a_uint32_t  duplex:1;
	a_uint32_t  rx_flow_en:1;
	a_uint32_t  tx_flow_en:1;
	a_uint32_t  _reserved1:25;
};

union mac_enable_u {
	a_uint32_t val;
	struct mac_enable bf;
};

/*[register] MAC_SPEED*/
#define MAC_SPEED
#define MAC_SPEED_ADDRESS 0x4
#define MAC_SPEED_NUM     6
#define MAC_SPEED_INC     0x200
#define MAC_SPEED_TYPE    REG_TYPE_RW
#define MAC_SPEED_DEFAULT 0x0
	/*[field] MAC_SPEED*/
	#define MAC_SPEED_MAC_SPEED
	#define MAC_SPEED_MAC_SPEED_OFFSET  0
	#define MAC_SPEED_MAC_SPEED_LEN     2
	#define MAC_SPEED_MAC_SPEED_DEFAULT 0x0

struct mac_speed {
	a_uint32_t  mac_speed:2;
	a_uint32_t  _reserved0:30;
};

union mac_speed_u {
	a_uint32_t val;
	struct mac_speed bf;
};

/*[register] MAC_CTRL2*/
#define MAC_CTRL2
#define MAC_CTRL2_ADDRESS 0x18
#define MAC_CTRL2_NUM     6
#define MAC_CTRL2_INC     0x200
#define MAC_CTRL2_TYPE    REG_TYPE_RW
#define MAC_CTRL2_DEFAULT 0x271c40
	/*[field] IPG_DEC_LEN*/
	#define MAC_CTRL2_IPG_DEC_LEN
	#define MAC_CTRL2_IPG_DEC_LEN_OFFSET  1
	#define MAC_CTRL2_IPG_DEC_LEN_LEN     1
	#define MAC_CTRL2_IPG_DEC_LEN_DEFAULT 0x0
	/*[field] TEST_PAUSE*/
	#define MAC_CTRL2_TEST_PAUSE
	#define MAC_CTRL2_TEST_PAUSE_OFFSET  2
	#define MAC_CTRL2_TEST_PAUSE_LEN     1
	#define MAC_CTRL2_TEST_PAUSE_DEFAULT 0x0
	/*[field] MAC_LPI_TX_IDLE*/
	#define MAC_CTRL2_MAC_LPI_TX_IDLE
	#define MAC_CTRL2_MAC_LPI_TX_IDLE_OFFSET  3
	#define MAC_CTRL2_MAC_LPI_TX_IDLE_LEN     1
	#define MAC_CTRL2_MAC_LPI_TX_IDLE_DEFAULT 0x0
	/*[field] MAC_LOOP_BACK*/
	#define MAC_CTRL2_MAC_LOOP_BACK
	#define MAC_CTRL2_MAC_LOOP_BACK_OFFSET  4
	#define MAC_CTRL2_MAC_LOOP_BACK_LEN     1
	#define MAC_CTRL2_MAC_LOOP_BACK_DEFAULT 0x0
	/*[field] IPG_DEC_EN*/
	#define MAC_CTRL2_IPG_DEC_EN
	#define MAC_CTRL2_IPG_DEC_EN_OFFSET  5
	#define MAC_CTRL2_IPG_DEC_EN_LEN     1
	#define MAC_CTRL2_IPG_DEC_EN_DEFAULT 0x0
	/*[field] CRS_SEL*/
	#define MAC_CTRL2_CRS_SEL
	#define MAC_CTRL2_CRS_SEL_OFFSET  6
	#define MAC_CTRL2_CRS_SEL_LEN     1
	#define MAC_CTRL2_CRS_SEL_DEFAULT 0x1
	/*[field] CRC_RSV_EN*/
	#define MAC_CTRL2_CRC_RSV_EN
	#define MAC_CTRL2_CRC_RSV_EN_OFFSET  7
	#define MAC_CTRL2_CRC_RSV_EN_LEN     1
	#define MAC_CTRL2_CRC_RSV_EN_DEFAULT 0x0
	/*[field] MAXFR*/
	#define MAC_CTRL2_MAXFR
	#define MAC_CTRL2_MAXFR_OFFSET  8
	#define MAC_CTRL2_MAXFR_LEN     14
	#define MAC_CTRL2_MAXFR_DEFAULT 0x271c

struct mac_ctrl2 {
	a_uint32_t  ipg_dec_len:1;
	a_uint32_t  test_pause:1;
	a_uint32_t  mac_lpi_tx_idle:1;
	a_uint32_t  mac_loop_back:1;
	a_uint32_t  ipg_dec_en:1;
	a_uint32_t  crs_sel:1;
	a_uint32_t  crc_rsv_en:1;
	a_uint32_t  maxfr:14;
	a_uint32_t  _reserved0:10;
};

union mac_ctrl2_u {
	a_uint32_t val;
	struct mac_ctrl2 bf;
};

/*[register] MAC_DBG_CTRL*/
#define MAC_DBG_CTRL
#define MAC_DBG_CTRL_ADDRESS 0x1c
#define MAC_DBG_CTRL_NUM     6
#define MAC_DBG_CTRL_INC     0x200
#define MAC_DBG_CTRL_TYPE    REG_TYPE_RW
#define MAC_DBG_CTRL_DEFAULT 0x800701040
	/*[field] IPGR1*/
	#define MAC_DBG_CTRL_IPGR1
	#define MAC_DBG_CTRL_IPGR1_OFFSET  0
	#define MAC_DBG_CTRL_IPGR1_LEN     7
	#define MAC_DBG_CTRL_IPGR1_DEFAULT 0x40
	/*[field] HIGH_IPG*/
	#define MAC_DBG_CTRL_HIGH_IPG
	#define MAC_DBG_CTRL_HIGH_IPG_OFFSET  8
	#define MAC_DBG_CTRL_HIGH_IPG_LEN     8
	#define MAC_DBG_CTRL_HIGH_IPG_DEFAULT 0x10
	/*[field] MAC_IPG_CTRL*/
	#define MAC_DBG_CTRL_MAC_IPG_CTRL
	#define MAC_DBG_CTRL_MAC_IPG_CTRL_OFFSET  20
	#define MAC_DBG_CTRL_MAC_IPG_CTRL_LEN     4
	#define MAC_DBG_CTRL_MAC_IPG_CTRL_DEFAULT 0x7
	/*[field] MAC_LEN_CTRL*/
	#define MAC_DBG_CTRL_MAC_LEN_CTRL
	#define MAC_DBG_CTRL_MAC_LEN_CTRL_OFFSET  30
	#define MAC_DBG_CTRL_MAC_LEN_CTRL_LEN     1
	#define MAC_DBG_CTRL_MAC_LEN_CTRL_DEFAULT 0x0
	/*[field] EDXSDFR_TRANSMIT_EN*/
	#define MAC_DBG_CTRL_EDXSDFR_TRANSMIT_EN
	#define MAC_DBG_CTRL_EDXSDFR_TRANSMIT_EN_OFFSET  31
	#define MAC_DBG_CTRL_EDXSDFR_TRANSMIT_EN_LEN     1
	#define MAC_DBG_CTRL_EDXSDFR_TRANSMIT_EN_DEFAULT 0x10

struct mac_dbg_ctrl {
	a_uint32_t  ipgr1:7;
	a_uint32_t  _reserved0:1;
	a_uint32_t  high_ipg:8;
	a_uint32_t  _reserved1:4;
	a_uint32_t  mac_ipg_ctrl:4;
	a_uint32_t  _reserved2:6;
	a_uint32_t  mac_len_ctrl:1;
	a_uint32_t  edxsdfr_transmit_en:1;
};

union mac_dbg_ctrl_u {
	a_uint32_t val;
	struct mac_dbg_ctrl bf;
};

/*[register] MAC_DBG_ADDR*/
#define MAC_DBG_ADDR
#define MAC_DBG_ADDR_ADDRESS 0x20
#define MAC_DBG_ADDR_NUM     6
#define MAC_DBG_ADDR_INC     0x200
#define MAC_DBG_ADDR_TYPE    REG_TYPE_RW
#define MAC_DBG_ADDR_DEFAULT 0x0
	/*[field] MAC_DEBUG_ADDR*/
	#define MAC_DBG_ADDR_MAC_DEBUG_ADDR
	#define MAC_DBG_ADDR_MAC_DEBUG_ADDR_OFFSET  0
	#define MAC_DBG_ADDR_MAC_DEBUG_ADDR_LEN     8
	#define MAC_DBG_ADDR_MAC_DEBUG_ADDR_DEFAULT 0x0

struct mac_dbg_addr {
	a_uint32_t  mac_debug_addr:8;
	a_uint32_t  _reserved0:24;
};

union mac_dbg_addr_u {
	a_uint32_t val;
	struct mac_dbg_addr bf;
};

/*[register] MAC_DBG_DATA*/
#define MAC_DBG_DATA
#define MAC_DBG_DATA_ADDRESS 0x24
#define MAC_DBG_DATA_NUM     6
#define MAC_DBG_DATA_INC     0x200
#define MAC_DBG_DATA_TYPE    REG_TYPE_RO
#define MAC_DBG_DATA_DEFAULT 0x0
	/*[field] MAC_DEBUG_DATA*/
	#define MAC_DBG_DATA_MAC_DEBUG_DATA
	#define MAC_DBG_DATA_MAC_DEBUG_DATA_OFFSET  0
	#define MAC_DBG_DATA_MAC_DEBUG_DATA_LEN     32
	#define MAC_DBG_DATA_MAC_DEBUG_DATA_DEFAULT 0x0

struct mac_dbg_data {
	a_uint32_t  mac_debug_data:32;
};

union mac_dbg_data_u {
	a_uint32_t val;
	struct mac_dbg_data bf;
};

/*[register] MAC_JUMBO_SIZE*/
#define MAC_JUMBO_SIZE
#define MAC_JUMBO_SIZE_ADDRESS 0x30
#define MAC_JUMBO_SIZE_NUM     6
#define MAC_JUMBO_SIZE_INC     0x200
#define MAC_JUMBO_SIZE_TYPE    REG_TYPE_RW
#define MAC_JUMBO_SIZE_DEFAULT 0x271c
	/*[field] MAC_JUMBO_SIZE*/
	#define MAC_JUMBO_SIZE_MAC_JUMBO_SIZE
	#define MAC_JUMBO_SIZE_MAC_JUMBO_SIZE_OFFSET  0
	#define MAC_JUMBO_SIZE_MAC_JUMBO_SIZE_LEN     14
	#define MAC_JUMBO_SIZE_MAC_JUMBO_SIZE_DEFAULT 0x271c

struct mac_jumbo_size {
	a_uint32_t  mac_jumbo_size:14;
	a_uint32_t  _reserved0:18;
};

union mac_jumbo_size_u {
	a_uint32_t val;
	struct mac_jumbo_size bf;
};

#endif
