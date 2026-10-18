/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_TUNNEL_REG_H_
#define _JHPPE_TUNNEL_REG_H_

/*[register] TPR_TUPLE_IP_ADDR_0*/
#define TPR_TUPLE_IP_ADDR_0
#define TPR_TUPLE_IP_ADDR_0_ADDRESS 0xa00
#define TPR_TUPLE_IP_ADDR_0_NUM     16
#define TPR_TUPLE_IP_ADDR_0_INC     0x4
#define TPR_TUPLE_IP_ADDR_0_TYPE    REG_TYPE_RW
#define TPR_TUPLE_IP_ADDR_0_DEFAULT 0x0
	/*[field] VALUE*/
	#define TPR_TUPLE_IP_ADDR_0_VALUE
	#define TPR_TUPLE_IP_ADDR_0_VALUE_OFFSET  0
	#define TPR_TUPLE_IP_ADDR_0_VALUE_LEN     32
	#define TPR_TUPLE_IP_ADDR_0_VALUE_DEFAULT 0x0

struct tpr_tuple_ip_addr_0 {
	a_uint32_t  value:32;
};

union tpr_tuple_ip_addr_0_u {
	a_uint32_t val;
	struct tpr_tuple_ip_addr_0 bf;
};

/*[register] TPR_TUPLE_IP_ADDR_1*/
#define TPR_TUPLE_IP_ADDR_1
#define TPR_TUPLE_IP_ADDR_1_ADDRESS 0xa40
#define TPR_TUPLE_IP_ADDR_1_NUM     16
#define TPR_TUPLE_IP_ADDR_1_INC     0x4
#define TPR_TUPLE_IP_ADDR_1_TYPE    REG_TYPE_RW
#define TPR_TUPLE_IP_ADDR_1_DEFAULT 0x0
	/*[field] VALUE*/
	#define TPR_TUPLE_IP_ADDR_1_VALUE
	#define TPR_TUPLE_IP_ADDR_1_VALUE_OFFSET  0
	#define TPR_TUPLE_IP_ADDR_1_VALUE_LEN     32
	#define TPR_TUPLE_IP_ADDR_1_VALUE_DEFAULT 0x0

struct tpr_tuple_ip_addr_1 {
	a_uint32_t  value:32;
};

union tpr_tuple_ip_addr_1_u {
	a_uint32_t val;
	struct tpr_tuple_ip_addr_1 bf;
};

/*[register] TPR_TUPLE_IP_ADDR_2*/
#define TPR_TUPLE_IP_ADDR_2
#define TPR_TUPLE_IP_ADDR_2_ADDRESS 0xa80
#define TPR_TUPLE_IP_ADDR_2_NUM     16
#define TPR_TUPLE_IP_ADDR_2_INC     0x4
#define TPR_TUPLE_IP_ADDR_2_TYPE    REG_TYPE_RW
#define TPR_TUPLE_IP_ADDR_2_DEFAULT 0x0
	/*[field] VALUE*/
	#define TPR_TUPLE_IP_ADDR_2_VALUE
	#define TPR_TUPLE_IP_ADDR_2_VALUE_OFFSET  0
	#define TPR_TUPLE_IP_ADDR_2_VALUE_LEN     32
	#define TPR_TUPLE_IP_ADDR_2_VALUE_DEFAULT 0x0

struct tpr_tuple_ip_addr_2 {
	a_uint32_t  value:32;
};

union tpr_tuple_ip_addr_2_u {
	a_uint32_t val;
	struct tpr_tuple_ip_addr_2 bf;
};

/*[register] TPR_TUPLE_IP_ADDR_3*/
#define TPR_TUPLE_IP_ADDR_3
#define TPR_TUPLE_IP_ADDR_3_ADDRESS 0xac0
#define TPR_TUPLE_IP_ADDR_3_NUM     16
#define TPR_TUPLE_IP_ADDR_3_INC     0x4
#define TPR_TUPLE_IP_ADDR_3_TYPE    REG_TYPE_RW
#define TPR_TUPLE_IP_ADDR_3_DEFAULT 0x0
	/*[field] VALUE*/
	#define TPR_TUPLE_IP_ADDR_3_VALUE
	#define TPR_TUPLE_IP_ADDR_3_VALUE_OFFSET  0
	#define TPR_TUPLE_IP_ADDR_3_VALUE_LEN     32
	#define TPR_TUPLE_IP_ADDR_3_VALUE_DEFAULT 0x0

struct tpr_tuple_ip_addr_3 {
	a_uint32_t  value:32;
};

union tpr_tuple_ip_addr_3_u {
	a_uint32_t val;
	struct tpr_tuple_ip_addr_3 bf;
};

/*[register] TPR_TUPLE_IP_ADDR_CTRL*/
#define TPR_TUPLE_IP_ADDR_CTRL
#define TPR_TUPLE_IP_ADDR_CTRL_ADDRESS 0xb00
#define TPR_TUPLE_IP_ADDR_CTRL_NUM     16
#define TPR_TUPLE_IP_ADDR_CTRL_INC     0x4
#define TPR_TUPLE_IP_ADDR_CTRL_TYPE    REG_TYPE_RW
#define TPR_TUPLE_IP_ADDR_CTRL_DEFAULT 0x0
	/*[field] TYPE*/
	#define TPR_TUPLE_IP_ADDR_CTRL_TYPE_F
	#define TPR_TUPLE_IP_ADDR_CTRL_TYPE_F_OFFSET  0
	#define TPR_TUPLE_IP_ADDR_CTRL_TYPE_F_LEN     1
	#define TPR_TUPLE_IP_ADDR_CTRL_TYPE_F_DEFAULT 0x0
	/*[field] VER*/
	#define TPR_TUPLE_IP_ADDR_CTRL_VER
	#define TPR_TUPLE_IP_ADDR_CTRL_VER_OFFSET  1
	#define TPR_TUPLE_IP_ADDR_CTRL_VER_LEN     1
	#define TPR_TUPLE_IP_ADDR_CTRL_VER_DEFAULT 0x0

struct tpr_tuple_ip_addr_ctrl {
	a_uint32_t  type:1;
	a_uint32_t  ver:1;
	a_uint32_t  _reserved0:30;
};

union tpr_tuple_ip_addr_ctrl_u {
	a_uint32_t val;
	struct tpr_tuple_ip_addr_ctrl bf;
};

/*[register] TPR_TUPLE_L4_PORT*/
#define TPR_TUPLE_L4_PORT
#define TPR_TUPLE_L4_PORT_ADDRESS 0xb80
#define TPR_TUPLE_L4_PORT_NUM     32
#define TPR_TUPLE_L4_PORT_INC     0x4
#define TPR_TUPLE_L4_PORT_TYPE    REG_TYPE_RW
#define TPR_TUPLE_L4_PORT_DEFAULT 0x0
	/*[field] PORT_VALUE*/
	#define TPR_TUPLE_L4_PORT_PORT_VALUE
	#define TPR_TUPLE_L4_PORT_PORT_VALUE_OFFSET  0
	#define TPR_TUPLE_L4_PORT_PORT_VALUE_LEN     16
	#define TPR_TUPLE_L4_PORT_PORT_VALUE_DEFAULT 0x0
	/*[field] PORT_TYPE*/
	#define TPR_TUPLE_L4_PORT_PORT_TYPE
	#define TPR_TUPLE_L4_PORT_PORT_TYPE_OFFSET  16
	#define TPR_TUPLE_L4_PORT_PORT_TYPE_LEN     2
	#define TPR_TUPLE_L4_PORT_PORT_TYPE_DEFAULT 0x0

struct tpr_tuple_l4_port {
	a_uint32_t  port_value:16;
	a_uint32_t  port_type:2;
	a_uint32_t  _reserved0:14;
};

union tpr_tuple_l4_port_u {
	a_uint32_t val;
	struct tpr_tuple_l4_port bf;
};

/*[register] TPR_TUPLE_ADDR_MATCH*/
#define TPR_TUPLE_ADDR_MATCH
#define TPR_TUPLE_ADDR_MATCH_ADDRESS 0xc00
#define TPR_TUPLE_ADDR_MATCH_NUM     16
#define TPR_TUPLE_ADDR_MATCH_INC     0x4
#define TPR_TUPLE_ADDR_MATCH_TYPE    REG_TYPE_RW
#define TPR_TUPLE_ADDR_MATCH_DEFAULT 0x0
	/*[field] ADDR_MAP*/
	#define TPR_TUPLE_ADDR_MATCH_ADDR_MAP
	#define TPR_TUPLE_ADDR_MATCH_ADDR_MAP_OFFSET  0
	#define TPR_TUPLE_ADDR_MATCH_ADDR_MAP_LEN     16
	#define TPR_TUPLE_ADDR_MATCH_ADDR_MAP_DEFAULT 0x0

struct tpr_tuple_addr_match {
	a_uint32_t  addr_map:16;
	a_uint32_t  _reserved0:16;
};

union tpr_tuple_addr_match_u {
	a_uint32_t val;
	struct tpr_tuple_addr_match bf;
};

/*[register] TPR_TUPLE_PORT_MATCH*/
#define TPR_TUPLE_PORT_MATCH
#define TPR_TUPLE_PORT_MATCH_ADDRESS 0xc40
#define TPR_TUPLE_PORT_MATCH_NUM     16
#define TPR_TUPLE_PORT_MATCH_INC     0x4
#define TPR_TUPLE_PORT_MATCH_TYPE    REG_TYPE_RW
#define TPR_TUPLE_PORT_MATCH_DEFAULT 0x0
	/*[field] PORT_MAP*/
	#define TPR_TUPLE_PORT_MATCH_PORT_MAP
	#define TPR_TUPLE_PORT_MATCH_PORT_MAP_OFFSET  0
	#define TPR_TUPLE_PORT_MATCH_PORT_MAP_LEN     32
	#define TPR_TUPLE_PORT_MATCH_PORT_MAP_DEFAULT 0x0

struct tpr_tuple_port_match {
	a_uint32_t  port_map:32;
};

union tpr_tuple_port_match_u {
	a_uint32_t val;
	struct tpr_tuple_port_match bf;
};

/*[register] TPR_TUPLE_MATCH_CTRL*/
#define TPR_TUPLE_MATCH_CTRL
#define TPR_TUPLE_MATCH_CTRL_ADDRESS 0xc80
#define TPR_TUPLE_MATCH_CTRL_NUM     16
#define TPR_TUPLE_MATCH_CTRL_INC     0x4
#define TPR_TUPLE_MATCH_CTRL_TYPE    REG_TYPE_RW
#define TPR_TUPLE_MATCH_CTRL_DEFAULT 0x0
	/*[field] VALID*/
	#define TPR_TUPLE_MATCH_CTRL_VALID
	#define TPR_TUPLE_MATCH_CTRL_VALID_OFFSET  0
	#define TPR_TUPLE_MATCH_CTRL_VALID_LEN     1
	#define TPR_TUPLE_MATCH_CTRL_VALID_DEFAULT 0x0
	/*[field] TUNNEL_TYPE_EN*/
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_EN
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_EN_OFFSET  1
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_EN_LEN     1
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_EN_DEFAULT 0x0
	/*[field] TUPLE_ID_EN*/
	#define TPR_TUPLE_MATCH_CTRL_TUPLE_ID_EN
	#define TPR_TUPLE_MATCH_CTRL_TUPLE_ID_EN_OFFSET  2
	#define TPR_TUPLE_MATCH_CTRL_TUPLE_ID_EN_LEN     1
	#define TPR_TUPLE_MATCH_CTRL_TUPLE_ID_EN_DEFAULT 0x0
	/*[field] TUNNEL_TYPE*/
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_OFFSET  3
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_LEN     5
	#define TPR_TUPLE_MATCH_CTRL_TUNNEL_TYPE_DEFAULT 0x0

struct tpr_tuple_match_ctrl {
	a_uint32_t  valid:1;
	a_uint32_t  tunnel_type_en:1;
	a_uint32_t  tuple_id_en:1;
	a_uint32_t  tunnel_type:5;
	a_uint32_t  _reserved0:24;
};

union tpr_tuple_match_ctrl_u {
	a_uint32_t val;
	struct tpr_tuple_match_ctrl bf;
};

/*[register] TUNNEL_TYPE_MAPPING*/
#define TUNNEL_TYPE_MAPPING
#define TUNNEL_TYPE_MAPPING_ADDRESS 0x43e8
#define TUNNEL_TYPE_MAPPING_NUM     32
#define TUNNEL_TYPE_MAPPING_INC     0x4
#define TUNNEL_TYPE_MAPPING_TYPE    REG_TYPE_RW
#define TUNNEL_TYPE_MAPPING_DEFAULT 0x0
	/*[field] APP_TYPE*/
	#define TUNNEL_TYPE_MAPPING_APP_TYPE
	#define TUNNEL_TYPE_MAPPING_APP_TYPE_OFFSET  0
	#define TUNNEL_TYPE_MAPPING_APP_TYPE_LEN     2
	#define TUNNEL_TYPE_MAPPING_APP_TYPE_DEFAULT 0x0
	/*[field] OUTER_EN*/
	#define TUNNEL_TYPE_MAPPING_OUTER_EN
	#define TUNNEL_TYPE_MAPPING_OUTER_EN_OFFSET  2
	#define TUNNEL_TYPE_MAPPING_OUTER_EN_LEN     1
	#define TUNNEL_TYPE_MAPPING_OUTER_EN_DEFAULT 0x0

struct tunnel_type_mapping {
	a_uint32_t  app_type:2;
	a_uint32_t  outer_en:1;
	a_uint32_t  _reserved0:29;
};

union tunnel_type_mapping_u {
	a_uint32_t val;
	struct tunnel_type_mapping bf;
};

#endif
