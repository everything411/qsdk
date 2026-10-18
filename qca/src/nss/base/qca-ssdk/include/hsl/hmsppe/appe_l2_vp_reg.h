/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_L2_VP_REG_H_
#define _APPE_L2_VP_REG_H_

/*[register] VP_LRN_LIMIT_COUNTER*/
#define VP_LRN_LIMIT_COUNTER
#define VP_LRN_LIMIT_COUNTER_ADDRESS 0xb00
#define VP_LRN_LIMIT_COUNTER_NUM     256
#define VP_LRN_LIMIT_COUNTER_INC     0x4
#define VP_LRN_LIMIT_COUNTER_TYPE    REG_TYPE_RO
#define VP_LRN_LIMIT_COUNTER_DEFAULT 0x0
	/*[field] LRN_CNT*/
	#define VP_LRN_LIMIT_COUNTER_LRN_CNT
	#define VP_LRN_LIMIT_COUNTER_LRN_CNT_OFFSET  0
	#define VP_LRN_LIMIT_COUNTER_LRN_CNT_LEN     12
	#define VP_LRN_LIMIT_COUNTER_LRN_CNT_DEFAULT 0x0

struct vp_lrn_limit_counter {
	a_uint32_t  lrn_cnt:12;
	a_uint32_t  _reserved0:20;
};

union vp_lrn_limit_counter_u {
	a_uint32_t val;
	struct vp_lrn_limit_counter bf;
};

/*[table] L2_VP_PORT_TBL*/
#define L2_VP_PORT_TBL
#define L2_VP_PORT_TBL_ADDRESS 0x23000
#define L2_VP_PORT_TBL_NUM     256
#define L2_VP_PORT_TBL_INC     0x10
#define L2_VP_PORT_TBL_TYPE    REG_TYPE_RW
#define L2_VP_PORT_TBL_DEFAULT 0x0
	/*[field] INVALID_VSI_FORWARDING_EN*/
	#define L2_VP_PORT_TBL_INVALID_VSI_FORWARDING_EN
	#define L2_VP_PORT_TBL_INVALID_VSI_FORWARDING_EN_OFFSET  0
	#define L2_VP_PORT_TBL_INVALID_VSI_FORWARDING_EN_LEN     1
	#define L2_VP_PORT_TBL_INVALID_VSI_FORWARDING_EN_DEFAULT 0x0
	/*[field] PROMISC_EN*/
	#define L2_VP_PORT_TBL_PROMISC_EN
	#define L2_VP_PORT_TBL_PROMISC_EN_OFFSET  1
	#define L2_VP_PORT_TBL_PROMISC_EN_LEN     1
	#define L2_VP_PORT_TBL_PROMISC_EN_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define L2_VP_PORT_TBL_DST_INFO
	#define L2_VP_PORT_TBL_DST_INFO_OFFSET  2
	#define L2_VP_PORT_TBL_DST_INFO_LEN     8
	#define L2_VP_PORT_TBL_DST_INFO_DEFAULT 0x0
	/*[field] NEW_ADDR_LRN_EN*/
	#define L2_VP_PORT_TBL_NEW_ADDR_LRN_EN
	#define L2_VP_PORT_TBL_NEW_ADDR_LRN_EN_OFFSET  10
	#define L2_VP_PORT_TBL_NEW_ADDR_LRN_EN_LEN     1
	#define L2_VP_PORT_TBL_NEW_ADDR_LRN_EN_DEFAULT 0x0
	/*[field] NEW_ADDR_FWD_CMD*/
	#define L2_VP_PORT_TBL_NEW_ADDR_FWD_CMD
	#define L2_VP_PORT_TBL_NEW_ADDR_FWD_CMD_OFFSET  11
	#define L2_VP_PORT_TBL_NEW_ADDR_FWD_CMD_LEN     2
	#define L2_VP_PORT_TBL_NEW_ADDR_FWD_CMD_DEFAULT 0x0
	/*[field] STATION_MOVE_LRN_EN*/
	#define L2_VP_PORT_TBL_STATION_MOVE_LRN_EN
	#define L2_VP_PORT_TBL_STATION_MOVE_LRN_EN_OFFSET  13
	#define L2_VP_PORT_TBL_STATION_MOVE_LRN_EN_LEN     1
	#define L2_VP_PORT_TBL_STATION_MOVE_LRN_EN_DEFAULT 0x0
	/*[field] STATION_MOVE_FWD_CMD*/
	#define L2_VP_PORT_TBL_STATION_MOVE_FWD_CMD
	#define L2_VP_PORT_TBL_STATION_MOVE_FWD_CMD_OFFSET  14
	#define L2_VP_PORT_TBL_STATION_MOVE_FWD_CMD_LEN     2
	#define L2_VP_PORT_TBL_STATION_MOVE_FWD_CMD_DEFAULT 0x0
	/*[field] LRN_LMT_CNT*/
	#define L2_VP_PORT_TBL_LRN_LMT_CNT
	#define L2_VP_PORT_TBL_LRN_LMT_CNT_OFFSET  16
	#define L2_VP_PORT_TBL_LRN_LMT_CNT_LEN     12
	#define L2_VP_PORT_TBL_LRN_LMT_CNT_DEFAULT 0x0
	/*[field] LRN_LMT_EN*/
	#define L2_VP_PORT_TBL_LRN_LMT_EN
	#define L2_VP_PORT_TBL_LRN_LMT_EN_OFFSET  28
	#define L2_VP_PORT_TBL_LRN_LMT_EN_LEN     1
	#define L2_VP_PORT_TBL_LRN_LMT_EN_DEFAULT 0x0
	/*[field] LRN_LMT_EXCEED_FWD*/
	#define L2_VP_PORT_TBL_LRN_LMT_EXCEED_FWD
	#define L2_VP_PORT_TBL_LRN_LMT_EXCEED_FWD_OFFSET  29
	#define L2_VP_PORT_TBL_LRN_LMT_EXCEED_FWD_LEN     2
	#define L2_VP_PORT_TBL_LRN_LMT_EXCEED_FWD_DEFAULT 0x0
	/*[field] PORT_ISOLATION_BITMAP*/
	#define L2_VP_PORT_TBL_PORT_ISOLATION_BITMAP
	#define L2_VP_PORT_TBL_PORT_ISOLATION_BITMAP_OFFSET  31
	#define L2_VP_PORT_TBL_PORT_ISOLATION_BITMAP_LEN     9
	#define L2_VP_PORT_TBL_PORT_ISOLATION_BITMAP_DEFAULT 0x0
	/*[field] ISOL_PROFILE*/
	#define L2_VP_PORT_TBL_ISOL_PROFILE
	#define L2_VP_PORT_TBL_ISOL_PROFILE_OFFSET  40
	#define L2_VP_PORT_TBL_ISOL_PROFILE_LEN     6
	#define L2_VP_PORT_TBL_ISOL_PROFILE_DEFAULT 0x0
	/*[field] ISOL_PROFILE_EN*/
	#define L2_VP_PORT_TBL_ISOL_PROFILE_EN
	#define L2_VP_PORT_TBL_ISOL_PROFILE_EN_OFFSET  46
	#define L2_VP_PORT_TBL_ISOL_PROFILE_EN_LEN     1
	#define L2_VP_PORT_TBL_ISOL_PROFILE_EN_DEFAULT 0x0
	/*[field] POLICER_EN*/
	#define L2_VP_PORT_TBL_POLICER_EN
	#define L2_VP_PORT_TBL_POLICER_EN_OFFSET  47
	#define L2_VP_PORT_TBL_POLICER_EN_LEN     1
	#define L2_VP_PORT_TBL_POLICER_EN_DEFAULT 0x0
	/*[field] POLICER_INDEX*/
	#define L2_VP_PORT_TBL_POLICER_INDEX
	#define L2_VP_PORT_TBL_POLICER_INDEX_OFFSET  48
	#define L2_VP_PORT_TBL_POLICER_INDEX_LEN     9
	#define L2_VP_PORT_TBL_POLICER_INDEX_DEFAULT 0x0
	/*[field] EXCEPTION_FMT_CTRL*/
	#define L2_VP_PORT_TBL_EXCEPTION_FMT_CTRL
	#define L2_VP_PORT_TBL_EXCEPTION_FMT_CTRL_OFFSET  57
	#define L2_VP_PORT_TBL_EXCEPTION_FMT_CTRL_LEN     1
	#define L2_VP_PORT_TBL_EXCEPTION_FMT_CTRL_DEFAULT 0x0
	/*[field] APP_CTRL_PROFILE*/
	#define L2_VP_PORT_TBL_APP_CTRL_PROFILE
	#define L2_VP_PORT_TBL_APP_CTRL_PROFILE_OFFSET  58
	#define L2_VP_PORT_TBL_APP_CTRL_PROFILE_LEN     8
	#define L2_VP_PORT_TBL_APP_CTRL_PROFILE_DEFAULT 0x0
	/*[field] MIRROR_EN*/
	#define L2_VP_PORT_TBL_MIRROR_EN
	#define L2_VP_PORT_TBL_MIRROR_EN_OFFSET  66
	#define L2_VP_PORT_TBL_MIRROR_EN_LEN     1
	#define L2_VP_PORT_TBL_MIRROR_EN_DEFAULT 0x0

struct l2_vp_port_tbl {
	a_uint32_t  invalid_vsi_forwarding_en:1;
	a_uint32_t  promisc_en:1;
	a_uint32_t  dst_info:8;
	a_uint32_t  new_addr_lrn_en:1;
	a_uint32_t  new_addr_fwd_cmd:2;
	a_uint32_t  station_move_lrn_en:1;
	a_uint32_t  station_move_fwd_cmd:2;
	a_uint32_t  lrn_lmt_cnt:12;
	a_uint32_t  lrn_lmt_en:1;
	a_uint32_t  lrn_lmt_exceed_fwd:2;
	a_uint32_t  port_isolation_bitmap_0:1;
	a_uint32_t  port_isolation_bitmap_1:8;
	a_uint32_t  isol_profile:6;
	a_uint32_t  isol_profile_en:1;
	a_uint32_t  policer_en:1;
	a_uint32_t  policer_index:9;
	a_uint32_t  exception_fmt_ctrl:1;
	a_uint32_t  app_ctrl_profile_0:6;
	a_uint32_t  app_ctrl_profile_1:2;
	a_uint32_t  mirror_en:1;
	a_uint32_t  _reserved0:29;
};

union l2_vp_port_tbl_u {
	a_uint32_t val[3];
	struct l2_vp_port_tbl bf;
};

#endif
