/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_QM_REG_H_
#define _HPPE_QM_REG_H_

/*[table] QUEUE_TX_COUNTER_TBL*/
#define QUEUE_TX_COUNTER_TBL
#define QUEUE_TX_COUNTER_TBL_ADDRESS 0x1a000
#define QUEUE_TX_COUNTER_TBL_NUM     300
#define QUEUE_TX_COUNTER_TBL_INC     0x10
#define QUEUE_TX_COUNTER_TBL_TYPE    REG_TYPE_RW
#define QUEUE_TX_COUNTER_TBL_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS_OFFSET  0
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS_LEN     32
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES_OFFSET  32
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES_LEN     40
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES_DEFAULT 0x0

struct queue_tx_counter_tbl {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union queue_tx_counter_tbl_u {
	a_uint32_t val[3];
	struct queue_tx_counter_tbl bf;
};

/*[register] FLUSH_CFG*/
#define FLUSH_CFG
#define FLUSH_CFG_ADDRESS 0x0
#define FLUSH_CFG_NUM     1
#define FLUSH_CFG_INC     0x4
#define FLUSH_CFG_TYPE    REG_TYPE_RW
#define FLUSH_CFG_DEFAULT 0x80000
	/*[field] FLUSH_QID*/
	#define FLUSH_CFG_FLUSH_QID
	#define FLUSH_CFG_FLUSH_QID_OFFSET  0
	#define FLUSH_CFG_FLUSH_QID_LEN     9
	#define FLUSH_CFG_FLUSH_QID_DEFAULT 0x0
	/*[field] FLUSH_STATUS*/
	#define FLUSH_CFG_FLUSH_STATUS
	#define FLUSH_CFG_FLUSH_STATUS_OFFSET  10
	#define FLUSH_CFG_FLUSH_STATUS_LEN     1
	#define FLUSH_CFG_FLUSH_STATUS_DEFAULT 0x0
	/*[field] FLUSH_WT_TIME*/
	#define FLUSH_CFG_FLUSH_WT_TIME
	#define FLUSH_CFG_FLUSH_WT_TIME_OFFSET  11
	#define FLUSH_CFG_FLUSH_WT_TIME_LEN     10
	#define FLUSH_CFG_FLUSH_WT_TIME_DEFAULT 0x100
	/*[field] FLUSH_DST_PORT*/
	#define FLUSH_CFG_FLUSH_DST_PORT
	#define FLUSH_CFG_FLUSH_DST_PORT_OFFSET  21
	#define FLUSH_CFG_FLUSH_DST_PORT_LEN     4
	#define FLUSH_CFG_FLUSH_DST_PORT_DEFAULT 0x0
	/*[field] FLUSH_ALL_QUEUES*/
	#define FLUSH_CFG_FLUSH_ALL_QUEUES
	#define FLUSH_CFG_FLUSH_ALL_QUEUES_OFFSET  25
	#define FLUSH_CFG_FLUSH_ALL_QUEUES_LEN     1
	#define FLUSH_CFG_FLUSH_ALL_QUEUES_DEFAULT 0x0
	/*[field] FLUSH_BUSY*/
	#define FLUSH_CFG_FLUSH_BUSY
	#define FLUSH_CFG_FLUSH_BUSY_OFFSET  31
	#define FLUSH_CFG_FLUSH_BUSY_LEN     1
	#define FLUSH_CFG_FLUSH_BUSY_DEFAULT 0x0

struct flush_cfg {
	a_uint32_t  flush_qid:9;
	a_uint32_t  _reserved0:1;
	a_uint32_t  flush_status:1;
	a_uint32_t  flush_wt_time:10;
	a_uint32_t  flush_dst_port:4;
	a_uint32_t  flush_all_queues:1;
	a_uint32_t  _reserved1:5;
	a_uint32_t  flush_busy:1;
};

union flush_cfg_u {
	a_uint32_t val;
	struct flush_cfg bf;
};

/*[register] IN_MIRROR_PRIORITY_CTRL*/
#define IN_MIRROR_PRIORITY_CTRL
#define IN_MIRROR_PRIORITY_CTRL_ADDRESS 0x4
#define IN_MIRROR_PRIORITY_CTRL_NUM     1
#define IN_MIRROR_PRIORITY_CTRL_INC     0x4
#define IN_MIRROR_PRIORITY_CTRL_TYPE    REG_TYPE_RW
#define IN_MIRROR_PRIORITY_CTRL_DEFAULT 0x0
	/*[field] PRIORITY*/
	#define IN_MIRROR_PRIORITY_CTRL_PRIORITY
	#define IN_MIRROR_PRIORITY_CTRL_PRIORITY_OFFSET  0
	#define IN_MIRROR_PRIORITY_CTRL_PRIORITY_LEN     4
	#define IN_MIRROR_PRIORITY_CTRL_PRIORITY_DEFAULT 0x0

struct in_mirror_priority_ctrl {
	a_uint32_t  priority:4;
	a_uint32_t  _reserved0:28;
};

union in_mirror_priority_ctrl_u {
	a_uint32_t val;
	struct in_mirror_priority_ctrl bf;
};

/*[register] EG_MIRROR_PRIORITY_CTRL*/
#define EG_MIRROR_PRIORITY_CTRL
#define EG_MIRROR_PRIORITY_CTRL_ADDRESS 0x8
#define EG_MIRROR_PRIORITY_CTRL_NUM     1
#define EG_MIRROR_PRIORITY_CTRL_INC     0x4
#define EG_MIRROR_PRIORITY_CTRL_TYPE    REG_TYPE_RW
#define EG_MIRROR_PRIORITY_CTRL_DEFAULT 0x0
	/*[field] PRIORITY*/
	#define EG_MIRROR_PRIORITY_CTRL_PRIORITY
	#define EG_MIRROR_PRIORITY_CTRL_PRIORITY_OFFSET  0
	#define EG_MIRROR_PRIORITY_CTRL_PRIORITY_LEN     4
	#define EG_MIRROR_PRIORITY_CTRL_PRIORITY_DEFAULT 0x0

struct eg_mirror_priority_ctrl {
	a_uint32_t  priority:4;
	a_uint32_t  _reserved0:28;
};

union eg_mirror_priority_ctrl_u {
	a_uint32_t val;
	struct eg_mirror_priority_ctrl bf;
};

/*[register] UCAST_DEFAULT_HASH*/
#define UCAST_DEFAULT_HASH
#define UCAST_DEFAULT_HASH_ADDRESS 0x60
#define UCAST_DEFAULT_HASH_NUM     1
#define UCAST_DEFAULT_HASH_INC     0x4
#define UCAST_DEFAULT_HASH_TYPE    REG_TYPE_RW
#define UCAST_DEFAULT_HASH_DEFAULT 0x0
	/*[field] HASH*/
	#define UCAST_DEFAULT_HASH_HASH
	#define UCAST_DEFAULT_HASH_HASH_OFFSET  0
	#define UCAST_DEFAULT_HASH_HASH_LEN     8
	#define UCAST_DEFAULT_HASH_HASH_DEFAULT 0x0

struct ucast_default_hash {
	a_uint32_t  hash:8;
	a_uint32_t  _reserved0:24;
};

union ucast_default_hash_u {
	a_uint32_t val;
	struct ucast_default_hash bf;
};

/*[register] MCAST_PRIORITY_MAP0*/
#define MCAST_PRIORITY_MAP0
#define MCAST_PRIORITY_MAP0_ADDRESS 0x100
#define MCAST_PRIORITY_MAP0_NUM     16
#define MCAST_PRIORITY_MAP0_INC     0x4
#define MCAST_PRIORITY_MAP0_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP0_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP0_CLASS
	#define MCAST_PRIORITY_MAP0_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP0_CLASS_LEN     4
	#define MCAST_PRIORITY_MAP0_CLASS_DEFAULT 0x0

struct mcast_priority_map0 {
	a_uint32_t  class:4;
	a_uint32_t  _reserved0:28;
};

union mcast_priority_map0_u {
	a_uint32_t val;
	struct mcast_priority_map0 bf;
};

/*[register] MCAST_PRIORITY_MAP1*/
#define MCAST_PRIORITY_MAP1
#define MCAST_PRIORITY_MAP1_ADDRESS 0x140
#define MCAST_PRIORITY_MAP1_NUM     16
#define MCAST_PRIORITY_MAP1_INC     0x4
#define MCAST_PRIORITY_MAP1_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP1_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP1_CLASS
	#define MCAST_PRIORITY_MAP1_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP1_CLASS_LEN     2
	#define MCAST_PRIORITY_MAP1_CLASS_DEFAULT 0x0

struct mcast_priority_map1 {
	a_uint32_t  class:2;
	a_uint32_t  _reserved0:30;
};

union mcast_priority_map1_u {
	a_uint32_t val;
	struct mcast_priority_map1 bf;
};

/*[register] MCAST_PRIORITY_MAP2*/
#define MCAST_PRIORITY_MAP2
#define MCAST_PRIORITY_MAP2_ADDRESS 0x180
#define MCAST_PRIORITY_MAP2_NUM     16
#define MCAST_PRIORITY_MAP2_INC     0x4
#define MCAST_PRIORITY_MAP2_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP2_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP2_CLASS
	#define MCAST_PRIORITY_MAP2_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP2_CLASS_LEN     2
	#define MCAST_PRIORITY_MAP2_CLASS_DEFAULT 0x0

struct mcast_priority_map2 {
	a_uint32_t  class:2;
	a_uint32_t  _reserved0:30;
};

union mcast_priority_map2_u {
	a_uint32_t val;
	struct mcast_priority_map2 bf;
};

/*[register] MCAST_PRIORITY_MAP3*/
#define MCAST_PRIORITY_MAP3
#define MCAST_PRIORITY_MAP3_ADDRESS 0x1c0
#define MCAST_PRIORITY_MAP3_NUM     16
#define MCAST_PRIORITY_MAP3_INC     0x4
#define MCAST_PRIORITY_MAP3_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP3_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP3_CLASS
	#define MCAST_PRIORITY_MAP3_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP3_CLASS_LEN     2
	#define MCAST_PRIORITY_MAP3_CLASS_DEFAULT 0x0

struct mcast_priority_map3 {
	a_uint32_t  class:2;
	a_uint32_t  _reserved0:30;
};

union mcast_priority_map3_u {
	a_uint32_t val;
	struct mcast_priority_map3 bf;
};

/*[register] MCAST_PRIORITY_MAP4*/
#define MCAST_PRIORITY_MAP4
#define MCAST_PRIORITY_MAP4_ADDRESS 0x200
#define MCAST_PRIORITY_MAP4_NUM     16
#define MCAST_PRIORITY_MAP4_INC     0x4
#define MCAST_PRIORITY_MAP4_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP4_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP4_CLASS
	#define MCAST_PRIORITY_MAP4_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP4_CLASS_LEN     2
	#define MCAST_PRIORITY_MAP4_CLASS_DEFAULT 0x0

struct mcast_priority_map4 {
	a_uint32_t  class:2;
	a_uint32_t  _reserved0:30;
};

union mcast_priority_map4_u {
	a_uint32_t val;
	struct mcast_priority_map4 bf;
};

/*[register] MCAST_PRIORITY_MAP5*/
#define MCAST_PRIORITY_MAP5
#define MCAST_PRIORITY_MAP5_ADDRESS 0x240
#define MCAST_PRIORITY_MAP5_NUM     16
#define MCAST_PRIORITY_MAP5_INC     0x4
#define MCAST_PRIORITY_MAP5_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP5_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP5_CLASS
	#define MCAST_PRIORITY_MAP5_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP5_CLASS_LEN     2
	#define MCAST_PRIORITY_MAP5_CLASS_DEFAULT 0x0

struct mcast_priority_map5 {
	a_uint32_t  class:2;
	a_uint32_t  _reserved0:30;
};

union mcast_priority_map5_u {
	a_uint32_t val;
	struct mcast_priority_map5 bf;
};

/*[register] MCAST_PRIORITY_MAP6*/
#define MCAST_PRIORITY_MAP6
#define MCAST_PRIORITY_MAP6_ADDRESS 0x280
#define MCAST_PRIORITY_MAP6_NUM     16
#define MCAST_PRIORITY_MAP6_INC     0x4
#define MCAST_PRIORITY_MAP6_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP6_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP6_CLASS
	#define MCAST_PRIORITY_MAP6_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP6_CLASS_LEN     2
	#define MCAST_PRIORITY_MAP6_CLASS_DEFAULT 0x0

struct mcast_priority_map6 {
	a_uint32_t  class:2;
	a_uint32_t  _reserved0:30;
};

union mcast_priority_map6_u {
	a_uint32_t val;
	struct mcast_priority_map6 bf;
};

/*[register] MCAST_PRIORITY_MAP7*/
#define MCAST_PRIORITY_MAP7
#define MCAST_PRIORITY_MAP7_ADDRESS 0x2c0
#define MCAST_PRIORITY_MAP7_NUM     16
#define MCAST_PRIORITY_MAP7_INC     0x4
#define MCAST_PRIORITY_MAP7_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP7_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP7_CLASS
	#define MCAST_PRIORITY_MAP7_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP7_CLASS_LEN     1
	#define MCAST_PRIORITY_MAP7_CLASS_DEFAULT 0x0

struct mcast_priority_map7 {
	a_uint32_t  class:1;
	a_uint32_t  _reserved0:31;
};

union mcast_priority_map7_u {
	a_uint32_t val;
	struct mcast_priority_map7 bf;
};

/*[table] UCAST_QUEUE_MAP_TBL*/
#define UCAST_QUEUE_MAP_TBL
#define UCAST_QUEUE_MAP_TBL_ADDRESS 0x10000
#define UCAST_QUEUE_MAP_TBL_NUM     4096
#define UCAST_QUEUE_MAP_TBL_INC     0x10
#define UCAST_QUEUE_MAP_TBL_TYPE    REG_TYPE_RW
#define UCAST_QUEUE_MAP_TBL_DEFAULT 0x0
	/*[field] PROFILE_ID*/
	#define UCAST_QUEUE_MAP_TBL_PROFILE_ID
	#define UCAST_QUEUE_MAP_TBL_PROFILE_ID_OFFSET  0
	#define UCAST_QUEUE_MAP_TBL_PROFILE_ID_LEN     4
	#define UCAST_QUEUE_MAP_TBL_PROFILE_ID_DEFAULT 0x0
	/*[field] QUEUE_ID*/
	#define UCAST_QUEUE_MAP_TBL_QUEUE_ID
	#define UCAST_QUEUE_MAP_TBL_QUEUE_ID_OFFSET  4
	#define UCAST_QUEUE_MAP_TBL_QUEUE_ID_LEN     8
	#define UCAST_QUEUE_MAP_TBL_QUEUE_ID_DEFAULT 0x0

struct ucast_queue_map_tbl {
	a_uint32_t  profile_id:4;
	a_uint32_t  queue_id:8;
	a_uint32_t  _reserved0:20;
};

union ucast_queue_map_tbl_u {
	a_uint32_t val;
	struct ucast_queue_map_tbl bf;
};

/*[table] UCAST_HASH_MAP_TBL*/
#define UCAST_HASH_MAP_TBL
#define UCAST_HASH_MAP_TBL_ADDRESS 0x20000
#define UCAST_HASH_MAP_TBL_NUM     4096
#define UCAST_HASH_MAP_TBL_INC     0x10
#define UCAST_HASH_MAP_TBL_TYPE    REG_TYPE_RW
#define UCAST_HASH_MAP_TBL_DEFAULT 0x0
	/*[field] HASH*/
	#define UCAST_HASH_MAP_TBL_HASH
	#define UCAST_HASH_MAP_TBL_HASH_OFFSET  0
	#define UCAST_HASH_MAP_TBL_HASH_LEN     8
	#define UCAST_HASH_MAP_TBL_HASH_DEFAULT 0x0

struct ucast_hash_map_tbl {
	a_uint32_t  hash:8;
	a_uint32_t  _reserved0:24;
};

union ucast_hash_map_tbl_u {
	a_uint32_t val;
	struct ucast_hash_map_tbl bf;
};

/*[table] UCAST_PRIORITY_MAP_TBL*/
#define UCAST_PRIORITY_MAP_TBL
#define UCAST_PRIORITY_MAP_TBL_ADDRESS 0x30000
#define UCAST_PRIORITY_MAP_TBL_NUM     256
#define UCAST_PRIORITY_MAP_TBL_INC     0x10
#define UCAST_PRIORITY_MAP_TBL_TYPE    REG_TYPE_RW
#define UCAST_PRIORITY_MAP_TBL_DEFAULT 0x0
	/*[field] CLASS*/
	#define UCAST_PRIORITY_MAP_TBL_CLASS
	#define UCAST_PRIORITY_MAP_TBL_CLASS_OFFSET  0
	#define UCAST_PRIORITY_MAP_TBL_CLASS_LEN     4
	#define UCAST_PRIORITY_MAP_TBL_CLASS_DEFAULT 0x0

struct ucast_priority_map_tbl {
	a_uint32_t  class:4;
	a_uint32_t  _reserved0:28;
};

union ucast_priority_map_tbl_u {
	a_uint32_t val;
	struct ucast_priority_map_tbl bf;
};

/*[table] MCAST_QUEUE_MAP_TBL*/
#define MCAST_QUEUE_MAP_TBL
#define MCAST_QUEUE_MAP_TBL_ADDRESS 0x32000
#define MCAST_QUEUE_MAP_TBL_NUM     256
#define MCAST_QUEUE_MAP_TBL_INC     0x10
#define MCAST_QUEUE_MAP_TBL_TYPE    REG_TYPE_RW
#define MCAST_QUEUE_MAP_TBL_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_QUEUE_MAP_TBL_CLASS
	#define MCAST_QUEUE_MAP_TBL_CLASS_OFFSET  0
	#define MCAST_QUEUE_MAP_TBL_CLASS_LEN     4
	#define MCAST_QUEUE_MAP_TBL_CLASS_DEFAULT 0x0

struct mcast_queue_map_tbl {
	a_uint32_t  class:4;
	a_uint32_t  _reserved0:28;
};

union mcast_queue_map_tbl_u {
	a_uint32_t val;
	struct mcast_queue_map_tbl bf;
};

/*[table] AC_UNI_QUEUE_CFG_TBL*/
#define AC_UNI_QUEUE_CFG_TBL
#define AC_UNI_QUEUE_CFG_TBL_ADDRESS 0x4e000
#define AC_UNI_QUEUE_CFG_TBL_NUM     256
#define AC_UNI_QUEUE_CFG_TBL_INC     0x20
#define AC_UNI_QUEUE_CFG_TBL_TYPE    REG_TYPE_RW
#define AC_UNI_QUEUE_CFG_TBL_DEFAULT 0x0
	/*[field] AC_CFG_AC_EN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN_OFFSET  0
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_WRED_EN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN_OFFSET  1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN_DEFAULT 0x0
	/*[field] AC_CFG_FORCE_AC_EN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_OFFSET  2
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_COLOR_AWARE*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_OFFSET  3
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_DEFAULT 0x0
	/*[field] AC_CFG_GRP_ID*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID_OFFSET  4
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID_LEN     2
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID_DEFAULT 0x0
	/*[field] AC_CFG_PRE_ALLOC_LIMIT*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_OFFSET  6
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_DYNAMIC*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC_OFFSET  18
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_WEIGHT*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT_OFFSET  19
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT_LEN     3
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_CEILING*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET  22
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_GRN_MIN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN_OFFSET  34
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_YEL_MAX*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX_OFFSET  46
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_YEL_MIN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET  58
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_RED_MAX*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX_OFFSET  70
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_RED_MIN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN_OFFSET  82
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN_DEFAULT 0x0
	/*[field] AC_CFG_RED_RESUME_OFFSET*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET  94
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_YEL_RESUME_OFFSET*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_OFFSET  106
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_GRN_RESUME_OFFSET*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET  118
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_DEFAULT 0x0

struct ac_uni_queue_cfg_tbl {
	a_uint32_t  ac_cfg_ac_en:1;
	a_uint32_t  ac_cfg_wred_en:1;
	a_uint32_t  ac_cfg_force_ac_en:1;
	a_uint32_t  ac_cfg_color_aware:1;
	a_uint32_t  ac_cfg_grp_id:2;
	a_uint32_t  ac_cfg_pre_alloc_limit:12;
	a_uint32_t  ac_cfg_shared_dynamic:1;
	a_uint32_t  ac_cfg_shared_weight:3;
	a_uint32_t  ac_cfg_shared_ceiling_0:10;
	a_uint32_t  ac_cfg_shared_ceiling_1:2;
	a_uint32_t  ac_cfg_gap_grn_grn_min:12;
	a_uint32_t  ac_cfg_gap_grn_yel_max:12;
	a_uint32_t  ac_cfg_gap_grn_yel_min_0:6;
	a_uint32_t  ac_cfg_gap_grn_yel_min_1:6;
	a_uint32_t  ac_cfg_gap_grn_red_max:12;
	a_uint32_t  ac_cfg_gap_grn_red_min:12;
	a_uint32_t  ac_cfg_red_resume_offset_0:2;
	a_uint32_t  ac_cfg_red_resume_offset_1:10;
	a_uint32_t  ac_cfg_yel_resume_offset:12;
	a_uint32_t  ac_cfg_grn_resume_offset_0:10;
	a_uint32_t  ac_cfg_grn_resume_offset_1:2;
	a_uint32_t  _reserved0:30;
};

union ac_uni_queue_cfg_tbl_u {
	a_uint32_t val[5];
	struct ac_uni_queue_cfg_tbl bf;
};

/*[table] AC_MUL_QUEUE_CFG_TBL*/
#define AC_MUL_QUEUE_CFG_TBL
#define AC_MUL_QUEUE_CFG_TBL_ADDRESS 0x50000
#define AC_MUL_QUEUE_CFG_TBL_NUM     44
#define AC_MUL_QUEUE_CFG_TBL_INC     0x10
#define AC_MUL_QUEUE_CFG_TBL_TYPE    REG_TYPE_RW
#define AC_MUL_QUEUE_CFG_TBL_DEFAULT 0x0
	/*[field] AC_CFG_AC_EN*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN_OFFSET  0
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN_LEN     1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_FORCE_AC_EN*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_OFFSET  1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_LEN     1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_COLOR_AWARE*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_OFFSET  2
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_LEN     1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_DEFAULT 0x0
	/*[field] AC_CFG_GRP_ID*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID_OFFSET  3
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID_LEN     2
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID_DEFAULT 0x0
	/*[field] AC_CFG_PRE_ALLOC_LIMIT*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_OFFSET  5
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_CEILING*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET  17
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_YEL*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_OFFSET  29
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_RED*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_OFFSET  41
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_DEFAULT 0x0
	/*[field] AC_CFG_RED_RESUME_OFFSET*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET  53
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_YEL_RESUME_OFFSET*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_OFFSET  65
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_GRN_RESUME_OFFSET*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET  77
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_DEFAULT 0x0

struct ac_mul_queue_cfg_tbl {
	a_uint32_t  ac_cfg_ac_en:1;
	a_uint32_t  ac_cfg_force_ac_en:1;
	a_uint32_t  ac_cfg_color_aware:1;
	a_uint32_t  ac_cfg_grp_id:2;
	a_uint32_t  ac_cfg_pre_alloc_limit:12;
	a_uint32_t  ac_cfg_shared_ceiling:12;
	a_uint32_t  ac_cfg_gap_grn_yel_0:3;
	a_uint32_t  ac_cfg_gap_grn_yel_1:9;
	a_uint32_t  ac_cfg_gap_grn_red:12;
	a_uint32_t  ac_cfg_red_resume_offset_0:11;
	a_uint32_t  ac_cfg_red_resume_offset_1:1;
	a_uint32_t  ac_cfg_yel_resume_offset:12;
	a_uint32_t  ac_cfg_grn_resume_offset:12;
	a_uint32_t  _reserved0:7;
};

union ac_mul_queue_cfg_tbl_u {
	a_uint32_t val[3];
	struct ac_mul_queue_cfg_tbl bf;
};

/*[table] AC_GRP_CFG_TBL*/
#define AC_GRP_CFG_TBL
#define AC_GRP_CFG_TBL_ADDRESS 0x51000
#define AC_GRP_CFG_TBL_NUM     4
#define AC_GRP_CFG_TBL_INC     0x10
#define AC_GRP_CFG_TBL_TYPE    REG_TYPE_RW
#define AC_GRP_CFG_TBL_DEFAULT 0x0
	/*[field] AC_CFG_AC_EN*/
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN_OFFSET  0
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN_LEN     1
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_FORCE_AC_EN*/
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN_OFFSET  1
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN_LEN     1
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_COLOR_AWARE*/
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE_OFFSET  2
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE_LEN     1
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE_DEFAULT 0x0
	/*[field] AC_GRP_GAP_GRN_RED*/
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED_OFFSET  3
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED_DEFAULT 0x0
	/*[field] AC_GRP_GAP_GRN_YEL*/
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL_OFFSET  15
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL_DEFAULT 0x0
	/*[field] AC_GRP_DP_THRD*/
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_OFFSET  27
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_DEFAULT 0x0
	/*[field] AC_GRP_LIMIT*/
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT_OFFSET  39
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT_DEFAULT 0x0
	/*[field] AC_GRP_RED_RESUME_OFFSET*/
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET_OFFSET  51
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_GRP_YEL_RESUME_OFFSET*/
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_OFFSET  63
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_GRP_GRN_RESUME_OFFSET*/
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET_OFFSET  75
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_GRP_PALLOC_LIMIT*/
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET  87
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_DEFAULT 0x0

struct ac_grp_cfg_tbl {
	a_uint32_t  ac_cfg_ac_en:1;
	a_uint32_t  ac_cfg_force_ac_en:1;
	a_uint32_t  ac_cfg_color_aware:1;
	a_uint32_t  ac_grp_gap_grn_red:12;
	a_uint32_t  ac_grp_gap_grn_yel:12;
	a_uint32_t  ac_grp_dp_thrd_0:5;
	a_uint32_t  ac_grp_dp_thrd_1:7;
	a_uint32_t  ac_grp_limit:12;
	a_uint32_t  ac_grp_red_resume_offset:12;
	a_uint32_t  ac_grp_yel_resume_offset_0:1;
	a_uint32_t  ac_grp_yel_resume_offset_1:11;
	a_uint32_t  ac_grp_grn_resume_offset:12;
	a_uint32_t  ac_grp_palloc_limit_0:9;
	a_uint32_t  ac_grp_palloc_limit_1:3;
	a_uint32_t  _reserved0:29;
};

union ac_grp_cfg_tbl_u {
	a_uint32_t val[4];
	struct ac_grp_cfg_tbl bf;
};

/*[table] AC_UNI_QUEUE_CNT_TBL*/
#define AC_UNI_QUEUE_CNT_TBL
#define AC_UNI_QUEUE_CNT_TBL_ADDRESS 0x53000
#define AC_UNI_QUEUE_CNT_TBL_NUM     256
#define AC_UNI_QUEUE_CNT_TBL_INC     0x10
#define AC_UNI_QUEUE_CNT_TBL_TYPE    REG_TYPE_RW
#define AC_UNI_QUEUE_CNT_TBL_DEFAULT 0x0
	/*[field] AC_UNI_QUEUE_CNT*/
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT_OFFSET  0
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT_LEN     13
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT_DEFAULT 0x0

struct ac_uni_queue_cnt_tbl {
	a_uint32_t  ac_uni_queue_cnt:13;
	a_uint32_t  _reserved0:19;
};

union ac_uni_queue_cnt_tbl_u {
	a_uint32_t val;
	struct ac_uni_queue_cnt_tbl bf;
};

/*[table] AC_MUL_QUEUE_CNT_TBL*/
#define AC_MUL_QUEUE_CNT_TBL
#define AC_MUL_QUEUE_CNT_TBL_ADDRESS 0x54000
#define AC_MUL_QUEUE_CNT_TBL_NUM     44
#define AC_MUL_QUEUE_CNT_TBL_INC     0x10
#define AC_MUL_QUEUE_CNT_TBL_TYPE    REG_TYPE_RW
#define AC_MUL_QUEUE_CNT_TBL_DEFAULT 0x0
	/*[field] AC_MUL_QUEUE_CNT*/
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT_OFFSET  0
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT_LEN     14
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT_DEFAULT 0x0

struct ac_mul_queue_cnt_tbl {
	a_uint32_t  ac_mul_queue_cnt:14;
	a_uint32_t  _reserved0:18;
};

union ac_mul_queue_cnt_tbl_u {
	a_uint32_t val;
	struct ac_mul_queue_cnt_tbl bf;
};

/*[table] AC_UNI_QUEUE_DROP_STATE_TBL*/
#define AC_UNI_QUEUE_DROP_STATE_TBL
#define AC_UNI_QUEUE_DROP_STATE_TBL_ADDRESS 0x56000
#define AC_UNI_QUEUE_DROP_STATE_TBL_NUM     256
#define AC_UNI_QUEUE_DROP_STATE_TBL_INC     0x10
#define AC_UNI_QUEUE_DROP_STATE_TBL_TYPE    REG_TYPE_RW
#define AC_UNI_QUEUE_DROP_STATE_TBL_DEFAULT 0x0
	/*[field] RED_RESUME_THRD*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_OFFSET  0
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_LEN     12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_DEFAULT 0x0
	/*[field] YEL_RESUME_THRD*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_OFFSET  12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_LEN     12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_DEFAULT 0x0
	/*[field] GRN_RESUME_THRD*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_OFFSET  24
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_LEN     12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_DEFAULT 0x0
	/*[field] RED_DROP_STATE*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_OFFSET  36
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_LEN     4
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_DEFAULT 0x0
	/*[field] YEL_DROP_STATE*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_OFFSET  40
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_LEN     4
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_DEFAULT 0x0
	/*[field] GRN_DROP_STATE*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_OFFSET  44
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_LEN     4
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_DEFAULT 0x0

struct ac_uni_queue_drop_state_tbl {
	a_uint32_t  red_resume_thrd:12;
	a_uint32_t  yel_resume_thrd:12;
	a_uint32_t  grn_resume_thrd_0:8;
	a_uint32_t  grn_resume_thrd_1:4;
	a_uint32_t  red_drop_state:4;
	a_uint32_t  yel_drop_state:4;
	a_uint32_t  grn_drop_state:4;
	a_uint32_t  _reserved0:16;
};

union ac_uni_queue_drop_state_tbl_u {
	a_uint32_t val[2];
	struct ac_uni_queue_drop_state_tbl bf;
};

/*[table] AC_MUL_QUEUE_DROP_STATE_TBL*/
#define AC_MUL_QUEUE_DROP_STATE_TBL
#define AC_MUL_QUEUE_DROP_STATE_TBL_ADDRESS 0x58000
#define AC_MUL_QUEUE_DROP_STATE_TBL_NUM     44
#define AC_MUL_QUEUE_DROP_STATE_TBL_INC     0x10
#define AC_MUL_QUEUE_DROP_STATE_TBL_TYPE    REG_TYPE_RW
#define AC_MUL_QUEUE_DROP_STATE_TBL_DEFAULT 0x0
	/*[field] RED_RESUME_THRD*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_OFFSET  0
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_LEN     12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_DEFAULT 0x0
	/*[field] YEL_RESUME_THRD*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_OFFSET  12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_LEN     12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_DEFAULT 0x0
	/*[field] GRN_RESUME_THRD*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_OFFSET  24
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_LEN     12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_DEFAULT 0x0
	/*[field] RED_DROP_STATE*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_OFFSET  36
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_LEN     1
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_DEFAULT 0x0
	/*[field] YEL_DROP_STATE*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_OFFSET  37
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_LEN     1
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_DEFAULT 0x0
	/*[field] GRN_DROP_STATE*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_OFFSET  38
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_LEN     1
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_DEFAULT 0x0

struct ac_mul_queue_drop_state_tbl {
	a_uint32_t  red_resume_thrd:12;
	a_uint32_t  yel_resume_thrd:12;
	a_uint32_t  grn_resume_thrd_0:8;
	a_uint32_t  grn_resume_thrd_1:4;
	a_uint32_t  red_drop_state:1;
	a_uint32_t  yel_drop_state:1;
	a_uint32_t  grn_drop_state:1;
	a_uint32_t  _reserved0:25;
};

union ac_mul_queue_drop_state_tbl_u {
	a_uint32_t val[2];
	struct ac_mul_queue_drop_state_tbl bf;
};

/*[table] AC_GRP_DROP_STATE_TBL*/
#define AC_GRP_DROP_STATE_TBL
#define AC_GRP_DROP_STATE_TBL_ADDRESS 0x5a000
#define AC_GRP_DROP_STATE_TBL_NUM     4
#define AC_GRP_DROP_STATE_TBL_INC     0x10
#define AC_GRP_DROP_STATE_TBL_TYPE    REG_TYPE_RW
#define AC_GRP_DROP_STATE_TBL_DEFAULT 0x0
	/*[field] RED_RESUME_THRD*/
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD_OFFSET  0
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD_LEN     12
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD_DEFAULT 0x0
	/*[field] YEL_RESUME_THRD*/
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD_OFFSET  12
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD_LEN     12
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD_DEFAULT 0x0
	/*[field] GRN_RESUME_THRD*/
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD_OFFSET  24
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD_LEN     12
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD_DEFAULT 0x0
	/*[field] RED_DROP_STATE*/
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE_OFFSET  36
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE_LEN     1
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE_DEFAULT 0x0
	/*[field] YEL_DROP_STATE*/
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE_OFFSET  37
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE_LEN     1
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE_DEFAULT 0x0
	/*[field] GRN_DROP_STATE*/
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE_OFFSET  38
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE_LEN     1
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE_DEFAULT 0x0

struct ac_grp_drop_state_tbl {
	a_uint32_t  red_resume_thrd:12;
	a_uint32_t  yel_resume_thrd:12;
	a_uint32_t  grn_resume_thrd_0:8;
	a_uint32_t  grn_resume_thrd_1:4;
	a_uint32_t  red_drop_state:1;
	a_uint32_t  yel_drop_state:1;
	a_uint32_t  grn_drop_state:1;
	a_uint32_t  _reserved0:25;
};

union ac_grp_drop_state_tbl_u {
	a_uint32_t val[2];
	struct ac_grp_drop_state_tbl bf;
};

/*[table] OQ_ENQ_OPR_TBL*/
#define OQ_ENQ_OPR_TBL
#define OQ_ENQ_OPR_TBL_ADDRESS 0x5c000
#define OQ_ENQ_OPR_TBL_NUM     300
#define OQ_ENQ_OPR_TBL_INC     0x10
#define OQ_ENQ_OPR_TBL_TYPE    REG_TYPE_RW
#define OQ_ENQ_OPR_TBL_DEFAULT 0x0
	/*[field] ENQ_DISABLE*/
	#define OQ_ENQ_OPR_TBL_ENQ_DISABLE
	#define OQ_ENQ_OPR_TBL_ENQ_DISABLE_OFFSET  0
	#define OQ_ENQ_OPR_TBL_ENQ_DISABLE_LEN     1
	#define OQ_ENQ_OPR_TBL_ENQ_DISABLE_DEFAULT 0x0

struct oq_enq_opr_tbl {
	a_uint32_t  enq_disable:1;
	a_uint32_t  _reserved0:31;
};

union oq_enq_opr_tbl_u {
	a_uint32_t val;
	struct oq_enq_opr_tbl bf;
};

/*[table] OQ_DEQ_OPR_TBL*/
#define OQ_DEQ_OPR_TBL
#define OQ_DEQ_OPR_TBL_ADDRESS 0x64000
#define OQ_DEQ_OPR_TBL_NUM     300
#define OQ_DEQ_OPR_TBL_INC     0x10
#define OQ_DEQ_OPR_TBL_TYPE    REG_TYPE_RW
#define OQ_DEQ_OPR_TBL_DEFAULT 0x0
	/*[field] DEQ_DROP*/
	#define OQ_DEQ_OPR_TBL_DEQ_DROP
	#define OQ_DEQ_OPR_TBL_DEQ_DROP_OFFSET  0
	#define OQ_DEQ_OPR_TBL_DEQ_DROP_LEN     1
	#define OQ_DEQ_OPR_TBL_DEQ_DROP_DEFAULT 0x0
	/*[field] DEQ_HIGH_PRI*/
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI_OFFSET  1
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI_LEN     1
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI_DEFAULT 0x0

struct oq_deq_opr_tbl {
	a_uint32_t  deq_drop:1;
	a_uint32_t  deq_high_pri:1;
	a_uint32_t  _reserved0:30;
};

union oq_deq_opr_tbl_u {
	a_uint32_t val;
	struct oq_deq_opr_tbl bf;
};

/*[table] UNI_DROP_CNT_TBL*/
#define UNI_DROP_CNT_TBL
#define UNI_DROP_CNT_TBL_ADDRESS 0x1e0000
#define UNI_DROP_CNT_TBL_NUM     1536
#define UNI_DROP_CNT_TBL_INC     0x10
#define UNI_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define UNI_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] UNI_DROP_PKT*/
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT_OFFSET  0
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT_LEN     32
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT_DEFAULT 0x0
	/*[field] UNI_DROP_BYTE*/
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE_OFFSET  32
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE_LEN     40
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE_DEFAULT 0x0

struct uni_drop_cnt_tbl {
	a_uint32_t  uni_drop_pkt:32;
	a_uint32_t  uni_drop_byte_0:32;
	a_uint32_t  uni_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union uni_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct uni_drop_cnt_tbl bf;
};

/*[table] MUL_P0_DROP_CNT_TBL*/
#define MUL_P0_DROP_CNT_TBL
#define MUL_P0_DROP_CNT_TBL_ADDRESS 0x1f0000
#define MUL_P0_DROP_CNT_TBL_NUM     48
#define MUL_P0_DROP_CNT_TBL_INC     0x10
#define MUL_P0_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P0_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P0_DROP_PKT*/
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_PKT
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_PKT_OFFSET  0
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_PKT_LEN     32
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P0_DROP_BYTE*/
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_BYTE
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_BYTE_OFFSET  32
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_BYTE_LEN     40
	#define MUL_P0_DROP_CNT_TBL_MUL_P0_DROP_BYTE_DEFAULT 0x0

struct mul_p0_drop_cnt_tbl {
	a_uint32_t  mul_p0_drop_pkt:32;
	a_uint32_t  mul_p0_drop_byte_0:32;
	a_uint32_t  mul_p0_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p0_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p0_drop_cnt_tbl bf;
};

/*[table] MUL_P1_DROP_CNT_TBL*/
#define MUL_P1_DROP_CNT_TBL
#define MUL_P1_DROP_CNT_TBL_ADDRESS 0x1f1000
#define MUL_P1_DROP_CNT_TBL_NUM     12
#define MUL_P1_DROP_CNT_TBL_INC     0x10
#define MUL_P1_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P1_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P1_DROP_PKT*/
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_PKT
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_PKT_OFFSET  0
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_PKT_LEN     32
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P1_DROP_BYTE*/
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_BYTE
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_BYTE_OFFSET  32
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_BYTE_LEN     40
	#define MUL_P1_DROP_CNT_TBL_MUL_P1_DROP_BYTE_DEFAULT 0x0

struct mul_p1_drop_cnt_tbl {
	a_uint32_t  mul_p1_drop_pkt:32;
	a_uint32_t  mul_p1_drop_byte_0:32;
	a_uint32_t  mul_p1_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p1_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p1_drop_cnt_tbl bf;
};

/*[table] MUL_P2_DROP_CNT_TBL*/
#define MUL_P2_DROP_CNT_TBL
#define MUL_P2_DROP_CNT_TBL_ADDRESS 0x1f2000
#define MUL_P2_DROP_CNT_TBL_NUM     12
#define MUL_P2_DROP_CNT_TBL_INC     0x10
#define MUL_P2_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P2_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P2_DROP_PKT*/
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_PKT
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_PKT_OFFSET  0
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_PKT_LEN     32
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P2_DROP_BYTE*/
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_BYTE
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_BYTE_OFFSET  32
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_BYTE_LEN     40
	#define MUL_P2_DROP_CNT_TBL_MUL_P2_DROP_BYTE_DEFAULT 0x0

struct mul_p2_drop_cnt_tbl {
	a_uint32_t  mul_p2_drop_pkt:32;
	a_uint32_t  mul_p2_drop_byte_0:32;
	a_uint32_t  mul_p2_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p2_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p2_drop_cnt_tbl bf;
};

/*[table] MUL_P3_DROP_CNT_TBL*/
#define MUL_P3_DROP_CNT_TBL
#define MUL_P3_DROP_CNT_TBL_ADDRESS 0x1f3000
#define MUL_P3_DROP_CNT_TBL_NUM     12
#define MUL_P3_DROP_CNT_TBL_INC     0x10
#define MUL_P3_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P3_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P3_DROP_PKT*/
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_PKT
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_PKT_OFFSET  0
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_PKT_LEN     32
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P3_DROP_BYTE*/
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_BYTE
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_BYTE_OFFSET  32
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_BYTE_LEN     40
	#define MUL_P3_DROP_CNT_TBL_MUL_P3_DROP_BYTE_DEFAULT 0x0

struct mul_p3_drop_cnt_tbl {
	a_uint32_t  mul_p3_drop_pkt:32;
	a_uint32_t  mul_p3_drop_byte_0:32;
	a_uint32_t  mul_p3_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p3_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p3_drop_cnt_tbl bf;
};

/*[table] MUL_P4_DROP_CNT_TBL*/
#define MUL_P4_DROP_CNT_TBL
#define MUL_P4_DROP_CNT_TBL_ADDRESS 0x1f4000
#define MUL_P4_DROP_CNT_TBL_NUM     12
#define MUL_P4_DROP_CNT_TBL_INC     0x10
#define MUL_P4_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P4_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P4_DROP_PKT*/
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_PKT
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_PKT_OFFSET  0
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_PKT_LEN     32
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P4_DROP_BYTE*/
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_BYTE
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_BYTE_OFFSET  32
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_BYTE_LEN     40
	#define MUL_P4_DROP_CNT_TBL_MUL_P4_DROP_BYTE_DEFAULT 0x0

struct mul_p4_drop_cnt_tbl {
	a_uint32_t  mul_p4_drop_pkt:32;
	a_uint32_t  mul_p4_drop_byte_0:32;
	a_uint32_t  mul_p4_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p4_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p4_drop_cnt_tbl bf;
};

/*[table] MUL_P5_DROP_CNT_TBL*/
#define MUL_P5_DROP_CNT_TBL
#define MUL_P5_DROP_CNT_TBL_ADDRESS 0x1f5000
#define MUL_P5_DROP_CNT_TBL_NUM     12
#define MUL_P5_DROP_CNT_TBL_INC     0x10
#define MUL_P5_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P5_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P5_DROP_PKT*/
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT_OFFSET  0
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT_LEN     32
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P5_DROP_BYTE*/
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE_OFFSET  32
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE_LEN     40
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE_DEFAULT 0x0

struct mul_p5_drop_cnt_tbl {
	a_uint32_t  mul_p5_drop_pkt:32;
	a_uint32_t  mul_p5_drop_byte_0:32;
	a_uint32_t  mul_p5_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p5_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p5_drop_cnt_tbl bf;
};

/*[table] MUL_P6_DROP_CNT_TBL*/
#define MUL_P6_DROP_CNT_TBL
#define MUL_P6_DROP_CNT_TBL_ADDRESS 0x1f6000
#define MUL_P6_DROP_CNT_TBL_NUM     12
#define MUL_P6_DROP_CNT_TBL_INC     0x10
#define MUL_P6_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P6_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P6_DROP_PKT*/
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_PKT
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_PKT_OFFSET  0
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_PKT_LEN     32
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P6_DROP_BYTE*/
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_BYTE
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_BYTE_OFFSET  32
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_BYTE_LEN     40
	#define MUL_P6_DROP_CNT_TBL_MUL_P6_DROP_BYTE_DEFAULT 0x0

struct mul_p6_drop_cnt_tbl {
	a_uint32_t  mul_p6_drop_pkt:32;
	a_uint32_t  mul_p6_drop_byte_0:32;
	a_uint32_t  mul_p6_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p6_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p6_drop_cnt_tbl bf;
};

/*[table] MUL_P7_DROP_CNT_TBL*/
#define MUL_P7_DROP_CNT_TBL
#define MUL_P7_DROP_CNT_TBL_ADDRESS 0x1f7000
#define MUL_P7_DROP_CNT_TBL_NUM     6
#define MUL_P7_DROP_CNT_TBL_INC     0x10
#define MUL_P7_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P7_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P7_DROP_PKT*/
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_PKT
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_PKT_OFFSET  0
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_PKT_LEN     32
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P7_DROP_BYTE*/
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_BYTE
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_BYTE_OFFSET  32
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_BYTE_LEN     40
	#define MUL_P7_DROP_CNT_TBL_MUL_P7_DROP_BYTE_DEFAULT 0x0

struct mul_p7_drop_cnt_tbl {
	a_uint32_t  mul_p7_drop_pkt:32;
	a_uint32_t  mul_p7_drop_byte_0:32;
	a_uint32_t  mul_p7_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p7_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p7_drop_cnt_tbl bf;
};

#endif
