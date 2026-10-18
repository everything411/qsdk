/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_CTRLPKT_REG_H_
#define _HPPE_CTRLPKT_REG_H_

/*[register] ETHERTYPE_CTRL*/
#define ETHERTYPE_CTRL
#define ETHERTYPE_CTRL_ADDRESS 0x80
#define ETHERTYPE_CTRL_NUM     4
#define ETHERTYPE_CTRL_INC     0x4
#define ETHERTYPE_CTRL_TYPE    REG_TYPE_RW
#define ETHERTYPE_CTRL_DEFAULT 0x0
	/*[field] ETHERTYPE_EN*/
	#define ETHERTYPE_CTRL_ETHERTYPE_EN
	#define ETHERTYPE_CTRL_ETHERTYPE_EN_OFFSET  0
	#define ETHERTYPE_CTRL_ETHERTYPE_EN_LEN     1
	#define ETHERTYPE_CTRL_ETHERTYPE_EN_DEFAULT 0x0
	/*[field] ETHERTYPE*/
	#define ETHERTYPE_CTRL_ETHERTYPE
	#define ETHERTYPE_CTRL_ETHERTYPE_OFFSET  16
	#define ETHERTYPE_CTRL_ETHERTYPE_LEN     16
	#define ETHERTYPE_CTRL_ETHERTYPE_DEFAULT 0x0

struct ethertype_ctrl {
	a_uint32_t  ethertype_en:1;
	a_uint32_t  _reserved0:15;
	a_uint32_t  ethertype:16;
};

union ethertype_ctrl_u {
	a_uint32_t val;
	struct ethertype_ctrl bf;
};

/*[register] L2_CPU_CODE_CTRL*/
#define L2_CPU_CODE_CTRL
#define L2_CPU_CODE_CTRL_ADDRESS 0x1000
#define L2_CPU_CODE_CTRL_NUM     256
#define L2_CPU_CODE_CTRL_INC     0x4
#define L2_CPU_CODE_CTRL_TYPE    REG_TYPE_RW
#define L2_CPU_CODE_CTRL_DEFAULT 0x0
	/*[field] EXCEPTION_FMT_CTRL_EN*/
	#define L2_CPU_CODE_CTRL_EXCEPTION_FMT_CTRL_EN
	#define L2_CPU_CODE_CTRL_EXCEPTION_FMT_CTRL_EN_OFFSET  0
	#define L2_CPU_CODE_CTRL_EXCEPTION_FMT_CTRL_EN_LEN     1
	#define L2_CPU_CODE_CTRL_EXCEPTION_FMT_CTRL_EN_DEFAULT 0x0
	/*[field] ENQUEUE_DISABLE*/
	#define L2_CPU_CODE_CTRL_ENQUEUE_DISABLE
	#define L2_CPU_CODE_CTRL_ENQUEUE_DISABLE_OFFSET  1
	#define L2_CPU_CODE_CTRL_ENQUEUE_DISABLE_LEN     1
	#define L2_CPU_CODE_CTRL_ENQUEUE_DISABLE_DEFAULT 0x0

struct l2_cpu_code_ctrl {
	a_uint32_t  exception_fmt_ctrl_en:1;
	a_uint32_t  enqueue_disable:1;
	a_uint32_t  _reserved0:30;
};

union l2_cpu_code_ctrl_u {
	a_uint32_t val;
	struct l2_cpu_code_ctrl bf;
};

/*[table] APP_CTRL*/
#define APP_CTRL
#define APP_CTRL_ADDRESS 0x3400
#define APP_CTRL_NUM     32
#define APP_CTRL_INC     0x10
#define APP_CTRL_TYPE    REG_TYPE_RW
#define APP_CTRL_DEFAULT 0x0
	/*[field] VALID*/
	#define APP_CTRL_VALID
	#define APP_CTRL_VALID_OFFSET  0
	#define APP_CTRL_VALID_LEN     1
	#define APP_CTRL_VALID_DEFAULT 0x0
	/*[field] RFDB_INCLUDE*/
	#define APP_CTRL_RFDB_INCLUDE
	#define APP_CTRL_RFDB_INCLUDE_OFFSET  1
	#define APP_CTRL_RFDB_INCLUDE_LEN     1
	#define APP_CTRL_RFDB_INCLUDE_DEFAULT 0x0
	/*[field] RFDB_INDEX_BITMAP*/
	#define APP_CTRL_RFDB_INDEX_BITMAP
	#define APP_CTRL_RFDB_INDEX_BITMAP_OFFSET  2
	#define APP_CTRL_RFDB_INDEX_BITMAP_LEN     32
	#define APP_CTRL_RFDB_INDEX_BITMAP_DEFAULT 0x0
	/*[field] PROTOCOL_INCLUDE*/
	#define APP_CTRL_PROTOCOL_INCLUDE
	#define APP_CTRL_PROTOCOL_INCLUDE_OFFSET  34
	#define APP_CTRL_PROTOCOL_INCLUDE_LEN     1
	#define APP_CTRL_PROTOCOL_INCLUDE_DEFAULT 0x0
	/*[field] PROTOCOL_BITMAP*/
	#define APP_CTRL_PROTOCOL_BITMAP
	#define APP_CTRL_PROTOCOL_BITMAP_OFFSET  35
	#define APP_CTRL_PROTOCOL_BITMAP_LEN     26
	#define APP_CTRL_PROTOCOL_BITMAP_DEFAULT 0x0
	/*[field] ETHERTYPE_INCLUDE*/
	#define APP_CTRL_ETHERTYPE_INCLUDE
	#define APP_CTRL_ETHERTYPE_INCLUDE_OFFSET  61
	#define APP_CTRL_ETHERTYPE_INCLUDE_LEN     1
	#define APP_CTRL_ETHERTYPE_INCLUDE_DEFAULT 0x0
	/*[field] ETHERTYPE_INDEX_BITMAP*/
	#define APP_CTRL_ETHERTYPE_INDEX_BITMAP
	#define APP_CTRL_ETHERTYPE_INDEX_BITMAP_OFFSET  62
	#define APP_CTRL_ETHERTYPE_INDEX_BITMAP_LEN     4
	#define APP_CTRL_ETHERTYPE_INDEX_BITMAP_DEFAULT 0x0
	/*[field] PORTBITMAP_INCLUDE*/
	#define APP_CTRL_PORTBITMAP_INCLUDE
	#define APP_CTRL_PORTBITMAP_INCLUDE_OFFSET  66
	#define APP_CTRL_PORTBITMAP_INCLUDE_LEN     1
	#define APP_CTRL_PORTBITMAP_INCLUDE_DEFAULT 0x0
	/*[field] PORTBITMAP*/
	#define APP_CTRL_PORTBITMAP
	#define APP_CTRL_PORTBITMAP_OFFSET  67
	#define APP_CTRL_PORTBITMAP_LEN     9
	#define APP_CTRL_PORTBITMAP_DEFAULT 0x0
	/*[field] IN_VLAN_FLTR_BYP*/
	#define APP_CTRL_IN_VLAN_FLTR_BYP
	#define APP_CTRL_IN_VLAN_FLTR_BYP_OFFSET  76
	#define APP_CTRL_IN_VLAN_FLTR_BYP_LEN     1
	#define APP_CTRL_IN_VLAN_FLTR_BYP_DEFAULT 0x0
	/*[field] IN_STG_BYP*/
	#define APP_CTRL_IN_STG_BYP
	#define APP_CTRL_IN_STG_BYP_OFFSET  77
	#define APP_CTRL_IN_STG_BYP_LEN     1
	#define APP_CTRL_IN_STG_BYP_DEFAULT 0x0
	/*[field] L2_SEC_BYP*/
	#define APP_CTRL_L2_SEC_BYP
	#define APP_CTRL_L2_SEC_BYP_OFFSET  78
	#define APP_CTRL_L2_SEC_BYP_LEN     1
	#define APP_CTRL_L2_SEC_BYP_DEFAULT 0x0
	/*[field] SG_BYP*/
	#define APP_CTRL_SG_BYP
	#define APP_CTRL_SG_BYP_OFFSET  79
	#define APP_CTRL_SG_BYP_LEN     1
	#define APP_CTRL_SG_BYP_DEFAULT 0x0
	/*[field] CMD*/
	#define APP_CTRL_CMD
	#define APP_CTRL_CMD_OFFSET  80
	#define APP_CTRL_CMD_LEN     2
	#define APP_CTRL_CMD_DEFAULT 0x0
	/*[field] PORT_TYPE*/
	#define APP_CTRL_PORT_TYPE
	#define APP_CTRL_PORT_TYPE_OFFSET  82
	#define APP_CTRL_PORT_TYPE_LEN     2
	#define APP_CTRL_PORT_TYPE_DEFAULT 0x0

struct app_ctrl {
	a_uint32_t  valid:1;
	a_uint32_t  rfdb_include:1;
	a_uint32_t  rfdb_index_bitmap_0:30;
	a_uint32_t  rfdb_index_bitmap_1:2;
	a_uint32_t  protocol_include:1;
	a_uint32_t  protocol_bitmap:26;
	a_uint32_t  ethertype_include:1;
	a_uint32_t  ethertype_index_bitmap_0:2;
	a_uint32_t  ethertype_index_bitmap_1:2;
	a_uint32_t  portbitmap_include:1;
	a_uint32_t  portbitmap:9;
	a_uint32_t  in_vlan_fltr_byp:1;
	a_uint32_t  in_stg_byp:1;
	a_uint32_t  l2_sec_byp:1;
	a_uint32_t  sg_byp:1;
	a_uint32_t  cmd:2;
	a_uint32_t  port_type:2;
	a_uint32_t  _reserved0:12;
};

union app_ctrl_u {
	a_uint32_t val[3];
	struct app_ctrl bf;
};

#endif
