/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_L3_FLOW_REG_H_
#define _JHPPE_L3_FLOW_REG_H_

/*[register] APP_UDP_PORT_CFG*/
#define APP_UDP_PORT_CFG
#define APP_UDP_PORT_CFG_ADDRESS 0x500
#define APP_UDP_PORT_CFG_NUM     1
#define APP_UDP_PORT_CFG_INC     0x4
#define APP_UDP_PORT_CFG_TYPE    REG_TYPE_RW
#define APP_UDP_PORT_CFG_DEFAULT 0x1f0000
	/*[field] PORT_VALUE*/
	#define APP_UDP_PORT_CFG_PORT_VALUE
	#define APP_UDP_PORT_CFG_PORT_VALUE_OFFSET  0
	#define APP_UDP_PORT_CFG_PORT_VALUE_LEN     16
	#define APP_UDP_PORT_CFG_PORT_VALUE_DEFAULT 0x0
	/*[field] IP_VER*/
	#define APP_UDP_PORT_CFG_IP_VER
	#define APP_UDP_PORT_CFG_IP_VER_OFFSET  16
	#define APP_UDP_PORT_CFG_IP_VER_LEN     2
	#define APP_UDP_PORT_CFG_IP_VER_DEFAULT 0x3
	/*[field] UDP_TYPE*/
	#define APP_UDP_PORT_CFG_UDP_TYPE
	#define APP_UDP_PORT_CFG_UDP_TYPE_OFFSET  18
	#define APP_UDP_PORT_CFG_UDP_TYPE_LEN     2
	#define APP_UDP_PORT_CFG_UDP_TYPE_DEFAULT 0x3
	/*[field] PORT_TYPE*/
	#define APP_UDP_PORT_CFG_PORT_TYPE
	#define APP_UDP_PORT_CFG_PORT_TYPE_OFFSET  20
	#define APP_UDP_PORT_CFG_PORT_TYPE_LEN     2
	#define APP_UDP_PORT_CFG_PORT_TYPE_DEFAULT 0x1

struct app_udp_port_cfg {
	a_uint32_t  port_value:16;
	a_uint32_t  ip_ver:2;
	a_uint32_t  udp_type:2;
	a_uint32_t  port_type:2;
	a_uint32_t  _reserved0:10;
};

union app_udp_port_cfg_u {
	a_uint32_t val;
	struct app_udp_port_cfg bf;
};

/*[register] NAT_T_CFG*/
#define NAT_T_CFG
#define NAT_T_CFG_ADDRESS 0x504
#define NAT_T_CFG_NUM     1
#define NAT_T_CFG_INC     0x4
#define NAT_T_CFG_TYPE    REG_TYPE_RW
#define NAT_T_CFG_DEFAULT 0x0
	/*[field] APP_UDP_PORT_MAP*/
	#define NAT_T_CFG_APP_UDP_PORT_MAP
	#define NAT_T_CFG_APP_UDP_PORT_MAP_OFFSET  0
	#define NAT_T_CFG_APP_UDP_PORT_MAP_LEN     1
	#define NAT_T_CFG_APP_UDP_PORT_MAP_DEFAULT 0x0

struct nat_t_cfg {
	a_uint32_t  app_udp_port_map:1;
	a_uint32_t  _reserved0:31;
};

union nat_t_cfg_u {
	a_uint32_t val;
	struct nat_t_cfg bf;
};

/*[register] L3_FLOW_KEY_GEN*/
#define L3_FLOW_KEY_GEN
#define L3_FLOW_KEY_GEN_ADDRESS 0x1a14
#define L3_FLOW_KEY_GEN_NUM     2
#define L3_FLOW_KEY_GEN_INC     0x10
#define L3_FLOW_KEY_GEN_TYPE    REG_TYPE_RW
#define L3_FLOW_KEY_GEN_DEFAULT 0x0
	/*[field] VALID*/
	#define L3_FLOW_KEY_GEN_VALID
	#define L3_FLOW_KEY_GEN_VALID_OFFSET  0
	#define L3_FLOW_KEY_GEN_VALID_LEN     1
	#define L3_FLOW_KEY_GEN_VALID_DEFAULT 0x0
	/*[field] L3_TYPE_VLD*/
	#define L3_FLOW_KEY_GEN_L3_TYPE_VLD
	#define L3_FLOW_KEY_GEN_L3_TYPE_VLD_OFFSET  1
	#define L3_FLOW_KEY_GEN_L3_TYPE_VLD_LEN     1
	#define L3_FLOW_KEY_GEN_L3_TYPE_VLD_DEFAULT 0x0
	/*[field] L3_TYPE*/
	#define L3_FLOW_KEY_GEN_L3_TYPE
	#define L3_FLOW_KEY_GEN_L3_TYPE_OFFSET  2
	#define L3_FLOW_KEY_GEN_L3_TYPE_LEN     2
	#define L3_FLOW_KEY_GEN_L3_TYPE_DEFAULT 0x0
	/*[field] L4_TYPE_VLD*/
	#define L3_FLOW_KEY_GEN_L4_TYPE_VLD
	#define L3_FLOW_KEY_GEN_L4_TYPE_VLD_OFFSET  4
	#define L3_FLOW_KEY_GEN_L4_TYPE_VLD_LEN     1
	#define L3_FLOW_KEY_GEN_L4_TYPE_VLD_DEFAULT 0x0
	/*[field] L4_TYPE*/
	#define L3_FLOW_KEY_GEN_L4_TYPE
	#define L3_FLOW_KEY_GEN_L4_TYPE_OFFSET  5
	#define L3_FLOW_KEY_GEN_L4_TYPE_LEN     3
	#define L3_FLOW_KEY_GEN_L4_TYPE_DEFAULT 0x0
	/*[field] APP_TYPE_VLD*/
	#define L3_FLOW_KEY_GEN_APP_TYPE_VLD
	#define L3_FLOW_KEY_GEN_APP_TYPE_VLD_OFFSET  8
	#define L3_FLOW_KEY_GEN_APP_TYPE_VLD_LEN     1
	#define L3_FLOW_KEY_GEN_APP_TYPE_VLD_DEFAULT 0x0
	/*[field] APP_TYPE*/
	#define L3_FLOW_KEY_GEN_APP_TYPE
	#define L3_FLOW_KEY_GEN_APP_TYPE_OFFSET  9
	#define L3_FLOW_KEY_GEN_APP_TYPE_LEN     2
	#define L3_FLOW_KEY_GEN_APP_TYPE_DEFAULT 0x0

struct l3_flow_key_gen {
	a_uint32_t  valid:1;
	a_uint32_t  l3_type_vld:1;
	a_uint32_t  l3_type:2;
	a_uint32_t  l4_type_vld:1;
	a_uint32_t  l4_type:3;
	a_uint32_t  app_type_vld:1;
	a_uint32_t  app_type:2;
	a_uint32_t  _reserved0:21;
};

union l3_flow_key_gen_u {
	a_uint32_t val;
	struct l3_flow_key_gen bf;
};

/*[register] L3_FLOW_KEY_SEL*/
#define L3_FLOW_KEY_SEL
#define L3_FLOW_KEY_SEL_ADDRESS 0x1a44
#define L3_FLOW_KEY_SEL_NUM     2
#define L3_FLOW_KEY_SEL_INC     0x14
#define L3_FLOW_KEY_SEL_TYPE    REG_TYPE_RW
#define L3_FLOW_KEY_SEL_DEFAULT 0x0
	/*[field] UDF0_MASK*/
	#define L3_FLOW_KEY_SEL_UDF0_MASK
	#define L3_FLOW_KEY_SEL_UDF0_MASK_OFFSET  0
	#define L3_FLOW_KEY_SEL_UDF0_MASK_LEN     16
	#define L3_FLOW_KEY_SEL_UDF0_MASK_DEFAULT 0x0
	/*[field] UDF1_MASK*/
	#define L3_FLOW_KEY_SEL_UDF1_MASK
	#define L3_FLOW_KEY_SEL_UDF1_MASK_OFFSET  16
	#define L3_FLOW_KEY_SEL_UDF1_MASK_LEN     16
	#define L3_FLOW_KEY_SEL_UDF1_MASK_DEFAULT 0x0
	/*[field] SIP_INC*/
	#define L3_FLOW_KEY_SEL_SIP_INC
	#define L3_FLOW_KEY_SEL_SIP_INC_OFFSET  32
	#define L3_FLOW_KEY_SEL_SIP_INC_LEN     1
	#define L3_FLOW_KEY_SEL_SIP_INC_DEFAULT 0x0
	/*[field] DIP_INC*/
	#define L3_FLOW_KEY_SEL_DIP_INC
	#define L3_FLOW_KEY_SEL_DIP_INC_OFFSET  33
	#define L3_FLOW_KEY_SEL_DIP_INC_LEN     1
	#define L3_FLOW_KEY_SEL_DIP_INC_DEFAULT 0x0
	/*[field] IP_PROT_INC*/
	#define L3_FLOW_KEY_SEL_IP_PROT_INC
	#define L3_FLOW_KEY_SEL_IP_PROT_INC_OFFSET  34
	#define L3_FLOW_KEY_SEL_IP_PROT_INC_LEN     1
	#define L3_FLOW_KEY_SEL_IP_PROT_INC_DEFAULT 0x0
	/*[field] SPORT_INC*/
	#define L3_FLOW_KEY_SEL_SPORT_INC
	#define L3_FLOW_KEY_SEL_SPORT_INC_OFFSET  35
	#define L3_FLOW_KEY_SEL_SPORT_INC_LEN     1
	#define L3_FLOW_KEY_SEL_SPORT_INC_DEFAULT 0x0
	/*[field] DPORT_INC*/
	#define L3_FLOW_KEY_SEL_DPORT_INC
	#define L3_FLOW_KEY_SEL_DPORT_INC_OFFSET  36
	#define L3_FLOW_KEY_SEL_DPORT_INC_LEN     1
	#define L3_FLOW_KEY_SEL_DPORT_INC_DEFAULT 0x0
	/*[field] UDF0_INC*/
	#define L3_FLOW_KEY_SEL_UDF0_INC
	#define L3_FLOW_KEY_SEL_UDF0_INC_OFFSET  37
	#define L3_FLOW_KEY_SEL_UDF0_INC_LEN     1
	#define L3_FLOW_KEY_SEL_UDF0_INC_DEFAULT 0x0
	/*[field] UDF0_ID*/
	#define L3_FLOW_KEY_SEL_UDF0_ID
	#define L3_FLOW_KEY_SEL_UDF0_ID_OFFSET  38
	#define L3_FLOW_KEY_SEL_UDF0_ID_LEN     2
	#define L3_FLOW_KEY_SEL_UDF0_ID_DEFAULT 0x0
	/*[field] UDF1_INC*/
	#define L3_FLOW_KEY_SEL_UDF1_INC
	#define L3_FLOW_KEY_SEL_UDF1_INC_OFFSET  40
	#define L3_FLOW_KEY_SEL_UDF1_INC_LEN     1
	#define L3_FLOW_KEY_SEL_UDF1_INC_DEFAULT 0x0
	/*[field] UDF1_ID*/
	#define L3_FLOW_KEY_SEL_UDF1_ID
	#define L3_FLOW_KEY_SEL_UDF1_ID_OFFSET  41
	#define L3_FLOW_KEY_SEL_UDF1_ID_LEN     2
	#define L3_FLOW_KEY_SEL_UDF1_ID_DEFAULT 0x0

struct l3_flow_key_sel {
	a_uint32_t  udf0_mask:16;
	a_uint32_t  udf1_mask:16;
	a_uint32_t  sip_inc:1;
	a_uint32_t  dip_inc:1;
	a_uint32_t  ip_prot_inc:1;
	a_uint32_t  sport_inc:1;
	a_uint32_t  dport_inc:1;
	a_uint32_t  udf0_inc:1;
	a_uint32_t  udf0_id:2;
	a_uint32_t  udf1_inc:1;
	a_uint32_t  udf1_id:2;
	a_uint32_t  _reserved0:21;
};

union l3_flow_key_sel_u {
	a_uint32_t val[2];
	struct l3_flow_key_sel bf;
};

/*[table] L3_MY_MAC_TBL*/
#define L3_MY_MAC_TBL
#define L3_MY_MAC_TBL_ADDRESS 0x2000
#define L3_MY_MAC_TBL_NUM     32
#define L3_MY_MAC_TBL_INC     0x8
#define L3_MY_MAC_TBL_TYPE    REG_TYPE_RW
#define L3_MY_MAC_TBL_DEFAULT 0x0
	/*[field] MAC*/
	#define L3_MY_MAC_TBL_MAC
	#define L3_MY_MAC_TBL_MAC_OFFSET  0
	#define L3_MY_MAC_TBL_MAC_LEN     48
	#define L3_MY_MAC_TBL_MAC_DEFAULT 0x0
	/*[field] VALID*/
	#define L3_MY_MAC_TBL_VALID
	#define L3_MY_MAC_TBL_VALID_OFFSET  48
	#define L3_MY_MAC_TBL_VALID_LEN     1
	#define L3_MY_MAC_TBL_VALID_DEFAULT 0x0
	/*[field] VSI*/
	#define L3_MY_MAC_TBL_VSI
	#define L3_MY_MAC_TBL_VSI_OFFSET  49
	#define L3_MY_MAC_TBL_VSI_LEN     6
	#define L3_MY_MAC_TBL_VSI_DEFAULT 0x0
	/*[field] VSI_VALID*/
	#define L3_MY_MAC_TBL_VSI_VALID
	#define L3_MY_MAC_TBL_VSI_VALID_OFFSET  55
	#define L3_MY_MAC_TBL_VSI_VALID_LEN     1
	#define L3_MY_MAC_TBL_VSI_VALID_DEFAULT 0x0
	/*[field] L3_IF_INDEX*/
	#define L3_MY_MAC_TBL_L3_IF_INDEX
	#define L3_MY_MAC_TBL_L3_IF_INDEX_OFFSET  56
	#define L3_MY_MAC_TBL_L3_IF_INDEX_LEN     8
	#define L3_MY_MAC_TBL_L3_IF_INDEX_DEFAULT 0x0

struct l3_my_mac_tbl {
	a_uint32_t  mac_0:32;
	a_uint32_t  mac_1:16;
	a_uint32_t  valid:1;
	a_uint32_t  vsi:6;
	a_uint32_t  vsi_valid:1;
	a_uint32_t  l3_if_index:8;
};

union l3_my_mac_tbl_u {
	a_uint32_t val[2];
	struct l3_my_mac_tbl bf;
};

/*[table] IN_FLOW_6TUPLE_TBL*/
#define IN_FLOW_6TUPLE_TBL
#define IN_FLOW_6TUPLE_TBL_ADDRESS 0x80000
#define IN_FLOW_6TUPLE_TBL_NUM     2048
#define IN_FLOW_6TUPLE_TBL_INC     0x40
#define IN_FLOW_6TUPLE_TBL_TYPE    REG_TYPE_RW
#define IN_FLOW_6TUPLE_TBL_DEFAULT 0x0
	/*[field] VALID*/
	#define IN_FLOW_6TUPLE_TBL_VALID
	#define IN_FLOW_6TUPLE_TBL_VALID_OFFSET  0
	#define IN_FLOW_6TUPLE_TBL_VALID_LEN     1
	#define IN_FLOW_6TUPLE_TBL_VALID_DEFAULT 0x0
	/*[field] ENTRY_TYPE*/
	#define IN_FLOW_6TUPLE_TBL_ENTRY_TYPE
	#define IN_FLOW_6TUPLE_TBL_ENTRY_TYPE_OFFSET  1
	#define IN_FLOW_6TUPLE_TBL_ENTRY_TYPE_LEN     1
	#define IN_FLOW_6TUPLE_TBL_ENTRY_TYPE_DEFAULT 0x0
	/*[field] HOST_ADDR_INDEX_TYPE*/
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_TYPE
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_TYPE_OFFSET  2
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_TYPE_LEN     1
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_TYPE_DEFAULT 0x0
	/*[field] HOST_ADDR_INDEX*/
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_OFFSET  3
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_LEN     13
	#define IN_FLOW_6TUPLE_TBL_HOST_ADDR_INDEX_DEFAULT 0x0
	/*[field] PROTOCOL_TYPE*/
	#define IN_FLOW_6TUPLE_TBL_PROTOCOL_TYPE
	#define IN_FLOW_6TUPLE_TBL_PROTOCOL_TYPE_OFFSET  16
	#define IN_FLOW_6TUPLE_TBL_PROTOCOL_TYPE_LEN     3
	#define IN_FLOW_6TUPLE_TBL_PROTOCOL_TYPE_DEFAULT 0x0
	/*[field] AGE*/
	#define IN_FLOW_6TUPLE_TBL_AGE
	#define IN_FLOW_6TUPLE_TBL_AGE_OFFSET  19
	#define IN_FLOW_6TUPLE_TBL_AGE_LEN     2
	#define IN_FLOW_6TUPLE_TBL_AGE_DEFAULT 0x0
	/*[field] SRC_L3_IF_VALID*/
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_VALID
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_VALID_OFFSET  21
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_VALID_LEN     1
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_VALID_DEFAULT 0x0
	/*[field] SRC_L3_IF*/
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_OFFSET  22
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_LEN     8
	#define IN_FLOW_6TUPLE_TBL_SRC_L3_IF_DEFAULT 0x0
	/*[field] FWD_TYPE*/
	#define IN_FLOW_6TUPLE_TBL_FWD_TYPE
	#define IN_FLOW_6TUPLE_TBL_FWD_TYPE_OFFSET  30
	#define IN_FLOW_6TUPLE_TBL_FWD_TYPE_LEN     3
	#define IN_FLOW_6TUPLE_TBL_FWD_TYPE_DEFAULT 0x0
	/*[field] NEXT_HOP1 reuse FWD_TYPE[1]*/
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP1
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP1_OFFSET  33
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP1_LEN     12
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP1_DEFAULT 0x0
	/*[field] NEXT_HOP2 reuse FWD_TYPE[2]*/
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP2
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP2_OFFSET  33
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP2_LEN     12
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP2_DEFAULT 0x0
	/*[field] NEXT_HOP3 reuse FWD_TYPE[3]*/
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP3
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP3_OFFSET  33
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP3_LEN     12
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP3_DEFAULT 0x0
	/*[field] PORT_VP2 reuse FWD_TYPE[4]*/
	#define IN_FLOW_6TUPLE_TBL_PORT_VP2
	#define IN_FLOW_6TUPLE_TBL_PORT_VP2_OFFSET  33
	#define IN_FLOW_6TUPLE_TBL_PORT_VP2_LEN     8
	#define IN_FLOW_6TUPLE_TBL_PORT_VP2_DEFAULT 0x0
	/*[field] VLAN_FMT_VALID reuse FWD_TYPE[4]*/
	#define IN_FLOW_6TUPLE_TBL_VLAN_FMT_VALID
	#define IN_FLOW_6TUPLE_TBL_VLAN_FMT_VALID_OFFSET  41
	#define IN_FLOW_6TUPLE_TBL_VLAN_FMT_VALID_LEN     1
	#define IN_FLOW_6TUPLE_TBL_VLAN_FMT_VALID_DEFAULT 0x0
	/*[field] SVLAN_FMT reuse FWD_TYPE[4]*/
	#define IN_FLOW_6TUPLE_TBL_SVLAN_FMT
	#define IN_FLOW_6TUPLE_TBL_SVLAN_FMT_OFFSET  42
	#define IN_FLOW_6TUPLE_TBL_SVLAN_FMT_LEN     1
	#define IN_FLOW_6TUPLE_TBL_SVLAN_FMT_DEFAULT 0x0
	/*[field] CVLAN_FMT reuse FWD_TYPE[4]*/
	#define IN_FLOW_6TUPLE_TBL_CVLAN_FMT
	#define IN_FLOW_6TUPLE_TBL_CVLAN_FMT_OFFSET  43
	#define IN_FLOW_6TUPLE_TBL_CVLAN_FMT_LEN     1
	#define IN_FLOW_6TUPLE_TBL_CVLAN_FMT_DEFAULT 0x0
	/*[field] NEXT_HOP4_VALID reuse FWD_TYPE[4]*/
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_VALID
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_VALID_OFFSET  44
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_VALID_LEN     1
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_VALID_DEFAULT 0x0
	/*[field] L4_PORT1 reuse FWD_TYPE[1]*/
	#define IN_FLOW_6TUPLE_TBL_L4_PORT1
	#define IN_FLOW_6TUPLE_TBL_L4_PORT1_OFFSET  45
	#define IN_FLOW_6TUPLE_TBL_L4_PORT1_LEN     16
	#define IN_FLOW_6TUPLE_TBL_L4_PORT1_DEFAULT 0x0
	/*[field] L4_PORT2 reuse FWD_TYPE[2]*/
	#define IN_FLOW_6TUPLE_TBL_L4_PORT2
	#define IN_FLOW_6TUPLE_TBL_L4_PORT2_OFFSET  45
	#define IN_FLOW_6TUPLE_TBL_L4_PORT2_LEN     16
	#define IN_FLOW_6TUPLE_TBL_L4_PORT2_DEFAULT 0x0
	/*[field] PORT_VP_VALID1 reuse FWD_TYPE[3]*/
	#define IN_FLOW_6TUPLE_TBL_PORT_VP_VALID1
	#define IN_FLOW_6TUPLE_TBL_PORT_VP_VALID1_OFFSET  45
	#define IN_FLOW_6TUPLE_TBL_PORT_VP_VALID1_LEN     1
	#define IN_FLOW_6TUPLE_TBL_PORT_VP_VALID1_DEFAULT 0x0
	/*[field] NEXT_HOP4 reuse FWD_TYPE[4]*/
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_OFFSET  45
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_LEN     12
	#define IN_FLOW_6TUPLE_TBL_NEXT_HOP4_DEFAULT 0x0
	/*[field] PORT_VP1 reuse FWD_TYPE[3]*/
	#define IN_FLOW_6TUPLE_TBL_PORT_VP1
	#define IN_FLOW_6TUPLE_TBL_PORT_VP1_OFFSET  46
	#define IN_FLOW_6TUPLE_TBL_PORT_VP1_LEN     8
	#define IN_FLOW_6TUPLE_TBL_PORT_VP1_DEFAULT 0x0
	/*[field] NAT_ACTION1 reuse FWD_TYPE[3]*/
	#define IN_FLOW_6TUPLE_TBL_NAT_ACTION1
	#define IN_FLOW_6TUPLE_TBL_NAT_ACTION1_OFFSET  54
	#define IN_FLOW_6TUPLE_TBL_NAT_ACTION1_LEN     2
	#define IN_FLOW_6TUPLE_TBL_NAT_ACTION1_DEFAULT 0x0
	/*[field] DE_ACCE*/
	#define IN_FLOW_6TUPLE_TBL_DE_ACCE
	#define IN_FLOW_6TUPLE_TBL_DE_ACCE_OFFSET  61
	#define IN_FLOW_6TUPLE_TBL_DE_ACCE_LEN     1
	#define IN_FLOW_6TUPLE_TBL_DE_ACCE_DEFAULT 0x0
	/*[field] COPY_TO_CPU_EN*/
	#define IN_FLOW_6TUPLE_TBL_COPY_TO_CPU_EN
	#define IN_FLOW_6TUPLE_TBL_COPY_TO_CPU_EN_OFFSET  62
	#define IN_FLOW_6TUPLE_TBL_COPY_TO_CPU_EN_LEN     1
	#define IN_FLOW_6TUPLE_TBL_COPY_TO_CPU_EN_DEFAULT 0x0
	/*[field] SYN_TOGGLE*/
	#define IN_FLOW_6TUPLE_TBL_SYN_TOGGLE
	#define IN_FLOW_6TUPLE_TBL_SYN_TOGGLE_OFFSET  63
	#define IN_FLOW_6TUPLE_TBL_SYN_TOGGLE_LEN     1
	#define IN_FLOW_6TUPLE_TBL_SYN_TOGGLE_DEFAULT 0x0
	/*[field] PRI_PROFILE*/
	#define IN_FLOW_6TUPLE_TBL_PRI_PROFILE
	#define IN_FLOW_6TUPLE_TBL_PRI_PROFILE_OFFSET  64
	#define IN_FLOW_6TUPLE_TBL_PRI_PROFILE_LEN     5
	#define IN_FLOW_6TUPLE_TBL_PRI_PROFILE_DEFAULT 0x0
	/*[field] SERVICE_CODE*/
	#define IN_FLOW_6TUPLE_TBL_SERVICE_CODE
	#define IN_FLOW_6TUPLE_TBL_SERVICE_CODE_OFFSET  69
	#define IN_FLOW_6TUPLE_TBL_SERVICE_CODE_LEN     8
	#define IN_FLOW_6TUPLE_TBL_SERVICE_CODE_DEFAULT 0x0
	/*[field] IP_ADDR*/
	#define IN_FLOW_6TUPLE_TBL_IP_ADDR
	#define IN_FLOW_6TUPLE_TBL_IP_ADDR_OFFSET  77
	#define IN_FLOW_6TUPLE_TBL_IP_ADDR_LEN     128
	#define IN_FLOW_6TUPLE_TBL_IP_ADDR_DEFAULT 0x0
	/*[field] L4_SPORT*/
	#define IN_FLOW_6TUPLE_TBL_L4_SPORT
	#define IN_FLOW_6TUPLE_TBL_L4_SPORT_OFFSET  205
	#define IN_FLOW_6TUPLE_TBL_L4_SPORT_LEN     16
	#define IN_FLOW_6TUPLE_TBL_L4_SPORT_DEFAULT 0x0
	/*[field] L4_DPORT*/
	#define IN_FLOW_6TUPLE_TBL_L4_DPORT
	#define IN_FLOW_6TUPLE_TBL_L4_DPORT_OFFSET  221
	#define IN_FLOW_6TUPLE_TBL_L4_DPORT_LEN     16
	#define IN_FLOW_6TUPLE_TBL_L4_DPORT_DEFAULT 0x0
	/*[field] IP_PROTOCOL*/
	#define IN_FLOW_6TUPLE_TBL_IP_PROTOCOL
	#define IN_FLOW_6TUPLE_TBL_IP_PROTOCOL_OFFSET  237
	#define IN_FLOW_6TUPLE_TBL_IP_PROTOCOL_LEN     8
	#define IN_FLOW_6TUPLE_TBL_IP_PROTOCOL_DEFAULT 0x0
	/*[field] VPN_ID*/
	#define IN_FLOW_6TUPLE_TBL_VPN_ID
	#define IN_FLOW_6TUPLE_TBL_VPN_ID_OFFSET  245
	#define IN_FLOW_6TUPLE_TBL_VPN_ID_LEN     13
	#define IN_FLOW_6TUPLE_TBL_VPN_ID_DEFAULT 0x0
	/*[field] PMTU_CHECK_TYPE*/
	#define IN_FLOW_6TUPLE_TBL_PMTU_CHECK_TYPE
	#define IN_FLOW_6TUPLE_TBL_PMTU_CHECK_TYPE_OFFSET  258
	#define IN_FLOW_6TUPLE_TBL_PMTU_CHECK_TYPE_LEN     1
	#define IN_FLOW_6TUPLE_TBL_PMTU_CHECK_TYPE_DEFAULT 0x0
	/*[field] PMTU*/
	#define IN_FLOW_6TUPLE_TBL_PMTU
	#define IN_FLOW_6TUPLE_TBL_PMTU_OFFSET  259
	#define IN_FLOW_6TUPLE_TBL_PMTU_LEN     14
	#define IN_FLOW_6TUPLE_TBL_PMTU_DEFAULT 0x0
	/*[field] ROUTING*/
	#define IN_FLOW_6TUPLE_TBL_ROUTING
	#define IN_FLOW_6TUPLE_TBL_ROUTING_OFFSET  273
	#define IN_FLOW_6TUPLE_TBL_ROUTING_LEN     1
	#define IN_FLOW_6TUPLE_TBL_ROUTING_DEFAULT 0x0
	/*[field] MATCH_MORE*/
	#define IN_FLOW_6TUPLE_TBL_MATCH_MORE
	#define IN_FLOW_6TUPLE_TBL_MATCH_MORE_OFFSET  274
	#define IN_FLOW_6TUPLE_TBL_MATCH_MORE_LEN     32
	#define IN_FLOW_6TUPLE_TBL_MATCH_MORE_DEFAULT 0x0
	/*[field] UDF0_VALID*/
	#define IN_FLOW_6TUPLE_TBL_UDF0_VALID
	#define IN_FLOW_6TUPLE_TBL_UDF0_VALID_OFFSET  306
	#define IN_FLOW_6TUPLE_TBL_UDF0_VALID_LEN     1
	#define IN_FLOW_6TUPLE_TBL_UDF0_VALID_DEFAULT 0x0
	/*[field] UDF1_VALID*/
	#define IN_FLOW_6TUPLE_TBL_UDF1_VALID
	#define IN_FLOW_6TUPLE_TBL_UDF1_VALID_OFFSET  307
	#define IN_FLOW_6TUPLE_TBL_UDF1_VALID_LEN     1
	#define IN_FLOW_6TUPLE_TBL_UDF1_VALID_DEFAULT 0x0
	/*[field] COUNTER_ID*/
	#define IN_FLOW_6TUPLE_TBL_COUNTER_ID
	#define IN_FLOW_6TUPLE_TBL_COUNTER_ID_OFFSET  308
	#define IN_FLOW_6TUPLE_TBL_COUNTER_ID_LEN     14
	#define IN_FLOW_6TUPLE_TBL_COUNTER_ID_DEFAULT 0x0

struct in_flow_6tuple_tbl_0 {
	a_uint32_t  valid:1;
	a_uint32_t  entry_type:1;
	a_uint32_t  host_addr_index_type:1;
	a_uint32_t  host_addr_index:13;
	a_uint32_t  protocol_type:3;
	a_uint32_t  age:2;
	a_uint32_t  src_l3_if_valid:1;
	a_uint32_t  src_l3_if:8;
	a_uint32_t  fwd_type_0:2;
	a_uint32_t  fwd_type_1:1;
	a_uint32_t  next_hop1:12;
	a_uint32_t  l4_port1:16;
	a_uint32_t  de_acce:1;
	a_uint32_t  copy_to_cpu_en:1;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  pri_profile:5;
	a_uint32_t  service_code:8;
	a_uint32_t  ip_addr_0:19;
	a_uint32_t  ip_addr_1:32;
	a_uint32_t  ip_addr_2:32;
	a_uint32_t  ip_addr_3:32;
	a_uint32_t  ip_addr_4:13;
	a_uint32_t  l4_sport:16;
	a_uint32_t  l4_dport_0:3;
	a_uint32_t  l4_dport_1:13;
	a_uint32_t  ip_protocol:8;
	a_uint32_t  vpn_id_0:11;
	a_uint32_t  vpn_id_1:2;
	a_uint32_t  pmtu_check_type:1;
	a_uint32_t  pmtu:14;
	a_uint32_t  routing:1;
	a_uint32_t  match_more_0:14;
	a_uint32_t  match_more_1:18;
	a_uint32_t  udf0_valid:1;
	a_uint32_t  udf1_valid:1;
	a_uint32_t  counter_id_0:12;
	a_uint32_t  counter_id_1:2;
	a_uint32_t  _reserved0:30;
};

struct in_flow_6tuple_tbl_1 {
	a_uint32_t  valid:1;
	a_uint32_t  entry_type:1;
	a_uint32_t  host_addr_index_type:1;
	a_uint32_t  host_addr_index:13;
	a_uint32_t  protocol_type:3;
	a_uint32_t  age:2;
	a_uint32_t  src_l3_if_valid:1;
	a_uint32_t  src_l3_if:8;
	a_uint32_t  fwd_type_0:2;
	a_uint32_t  fwd_type_1:1;
	a_uint32_t  next_hop2:12;
	a_uint32_t  l4_port2:16;
	a_uint32_t  de_acce:1;
	a_uint32_t  copy_to_cpu_en:1;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  pri_profile:5;
	a_uint32_t  service_code:8;
	a_uint32_t  ip_addr_0:19;
	a_uint32_t  ip_addr_1:32;
	a_uint32_t  ip_addr_2:32;
	a_uint32_t  ip_addr_3:32;
	a_uint32_t  ip_addr_4:13;
	a_uint32_t  l4_sport:16;
	a_uint32_t  l4_dport_0:3;
	a_uint32_t  l4_dport_1:13;
	a_uint32_t  ip_protocol:8;
	a_uint32_t  vpn_id_0:11;
	a_uint32_t  vpn_id_1:2;
	a_uint32_t  pmtu_check_type:1;
	a_uint32_t  pmtu:14;
	a_uint32_t  routing:1;
	a_uint32_t  match_more_0:14;
	a_uint32_t  match_more_1:18;
	a_uint32_t  udf0_valid:1;
	a_uint32_t  udf1_valid:1;
	a_uint32_t  counter_id_0:12;
	a_uint32_t  counter_id_1:2;
	a_uint32_t  _reserved0:30;
};

struct in_flow_6tuple_tbl_2 {
	a_uint32_t  valid:1;
	a_uint32_t  entry_type:1;
	a_uint32_t  host_addr_index_type:1;
	a_uint32_t  host_addr_index:13;
	a_uint32_t  protocol_type:3;
	a_uint32_t  age:2;
	a_uint32_t  src_l3_if_valid:1;
	a_uint32_t  src_l3_if:8;
	a_uint32_t  fwd_type_0:2;
	a_uint32_t  fwd_type_1:1;
	a_uint32_t  next_hop3:12;
	a_uint32_t  port_vp_valid1:1;
	a_uint32_t  port_vp1:8;
	a_uint32_t  nat_action1:2;
	a_uint32_t  _reserved0:5;
	a_uint32_t  de_acce:1;
	a_uint32_t  copy_to_cpu_en:1;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  pri_profile:5;
	a_uint32_t  service_code:8;
	a_uint32_t  ip_addr_0:19;
	a_uint32_t  ip_addr_1:32;
	a_uint32_t  ip_addr_2:32;
	a_uint32_t  ip_addr_3:32;
	a_uint32_t  ip_addr_4:13;
	a_uint32_t  l4_sport:16;
	a_uint32_t  l4_dport_0:3;
	a_uint32_t  l4_dport_1:13;
	a_uint32_t  ip_protocol:8;
	a_uint32_t  vpn_id_0:11;
	a_uint32_t  vpn_id_1:2;
	a_uint32_t  pmtu_check_type:1;
	a_uint32_t  pmtu:14;
	a_uint32_t  routing:1;
	a_uint32_t  match_more_0:14;
	a_uint32_t  match_more_1:18;
	a_uint32_t  udf0_valid:1;
	a_uint32_t  udf1_valid:1;
	a_uint32_t  counter_id_0:12;
	a_uint32_t  counter_id_1:2;
	a_uint32_t  _reserved1:30;
};

struct in_flow_6tuple_tbl_3 {
	a_uint32_t  valid:1;
	a_uint32_t  entry_type:1;
	a_uint32_t  host_addr_index_type:1;
	a_uint32_t  host_addr_index:13;
	a_uint32_t  protocol_type:3;
	a_uint32_t  age:2;
	a_uint32_t  src_l3_if_valid:1;
	a_uint32_t  src_l3_if:8;
	a_uint32_t  fwd_type_0:2;
	a_uint32_t  fwd_type_1:1;
	a_uint32_t  port_vp2:8;
	a_uint32_t  vlan_fmt_valid:1;
	a_uint32_t  svlan_fmt:1;
	a_uint32_t  cvlan_fmt:1;
	a_uint32_t  next_hop4_valid:1;
	a_uint32_t  next_hop4:12;
	a_uint32_t  _reserved0:4;
	a_uint32_t  de_acce:1;
	a_uint32_t  copy_to_cpu_en:1;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  pri_profile:5;
	a_uint32_t  service_code:8;
	a_uint32_t  ip_addr_0:19;
	a_uint32_t  ip_addr_1:32;
	a_uint32_t  ip_addr_2:32;
	a_uint32_t  ip_addr_3:32;
	a_uint32_t  ip_addr_4:13;
	a_uint32_t  l4_sport:16;
	a_uint32_t  l4_dport_0:3;
	a_uint32_t  l4_dport_1:13;
	a_uint32_t  ip_protocol:8;
	a_uint32_t  vpn_id_0:11;
	a_uint32_t  vpn_id_1:2;
	a_uint32_t  pmtu_check_type:1;
	a_uint32_t  pmtu:14;
	a_uint32_t  routing:1;
	a_uint32_t  match_more_0:14;
	a_uint32_t  match_more_1:18;
	a_uint32_t  udf0_valid:1;
	a_uint32_t  udf1_valid:1;
	a_uint32_t  counter_id_0:12;
	a_uint32_t  counter_id_1:2;
	a_uint32_t  _reserved1:30;
};

union in_flow_6tuple_tbl_u {
	a_uint32_t val[11];
	struct in_flow_6tuple_tbl_0 bf;
	struct in_flow_6tuple_tbl_1 bf1;
	struct in_flow_6tuple_tbl_2 bf2;
	struct in_flow_6tuple_tbl_3 bf3;
};

#endif
