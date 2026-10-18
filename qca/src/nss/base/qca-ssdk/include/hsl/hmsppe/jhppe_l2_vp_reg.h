/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_L2_VP_REG_H_
#define _JHPPE_L2_VP_REG_H_

/*[register] EG_SRC_PORT_REMAP*/
#define EG_SRC_PORT_REMAP
#define EG_SRC_PORT_REMAP_ADDRESS 0x240
#define EG_SRC_PORT_REMAP_NUM     9
#define EG_SRC_PORT_REMAP_INC     0x4
#define EG_SRC_PORT_REMAP_TYPE    REG_TYPE_RW
#define EG_SRC_PORT_REMAP_DEFAULT 0x0
	/*[field] SRC_PORT_NUM*/
	#define EG_SRC_PORT_REMAP_SRC_PORT_NUM
	#define EG_SRC_PORT_REMAP_SRC_PORT_NUM_OFFSET  0
	#define EG_SRC_PORT_REMAP_SRC_PORT_NUM_LEN     8
	#define EG_SRC_PORT_REMAP_SRC_PORT_NUM_DEFAULT 0x0

struct eg_src_port_remap {
	a_uint32_t  src_port_num:8;
	a_uint32_t  _reserved0:24;
};

union eg_src_port_remap_u {
	a_uint32_t val;
	struct eg_src_port_remap bf;
};

/*[register] PORT_ISOL_ACTION*/
#define PORT_ISOL_ACTION
#define PORT_ISOL_ACTION_ADDRESS 0x1840
#define PORT_ISOL_ACTION_NUM     9
#define PORT_ISOL_ACTION_INC     0x4
#define PORT_ISOL_ACTION_TYPE    REG_TYPE_RW
#define PORT_ISOL_ACTION_DEFAULT 0x0
	/*[field] ACTION*/
	#define PORT_ISOL_ACTION_ACTION
	#define PORT_ISOL_ACTION_ACTION_OFFSET  0
	#define PORT_ISOL_ACTION_ACTION_LEN     18
	#define PORT_ISOL_ACTION_ACTION_DEFAULT 0x0

struct port_isol_action {
	a_uint32_t  action:18;
	a_uint32_t  _reserved0:14;
};

union port_isol_action_u {
	a_uint32_t val;
	struct port_isol_action bf;
};

/*[table] L2_VP_PORT_POST_TBL*/
#define L2_VP_PORT_POST_TBL
#define L2_VP_PORT_POST_TBL_ADDRESS 0x26000
#define L2_VP_PORT_POST_TBL_NUM     256
#define L2_VP_PORT_POST_TBL_INC     0x10
#define L2_VP_PORT_POST_TBL_TYPE    REG_TYPE_RW
#define L2_VP_PORT_POST_TBL_DEFAULT 0x0
	/*[field] PHYSICAL_PORT*/
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_OFFSET  0
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_LEN     4
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_DEFAULT 0x0
	/*[field] EG_VLAN_FLTR_CMD*/
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FLTR_CMD
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FLTR_CMD_OFFSET  4
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FLTR_CMD_LEN     1
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FLTR_CMD_DEFAULT 0x0
	/*[field] ISOL_PROFILE*/
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_OFFSET  5
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_LEN     6
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_DEFAULT 0x0
	/*[field] ISOL_PROFILE_EN*/
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_EN
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_EN_OFFSET  11
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_EN_LEN     1
	#define L2_VP_PORT_POST_TBL_ISOL_PROFILE_EN_DEFAULT 0x0
	/*[field] VP_STATE_CHECK_EN*/
	#define L2_VP_PORT_POST_TBL_VP_STATE_CHECK_EN
	#define L2_VP_PORT_POST_TBL_VP_STATE_CHECK_EN_OFFSET  12
	#define L2_VP_PORT_POST_TBL_VP_STATE_CHECK_EN_LEN     1
	#define L2_VP_PORT_POST_TBL_VP_STATE_CHECK_EN_DEFAULT 0x0
	/*[field] VP_TYPE*/
	#define L2_VP_PORT_POST_TBL_VP_TYPE
	#define L2_VP_PORT_POST_TBL_VP_TYPE_OFFSET  13
	#define L2_VP_PORT_POST_TBL_VP_TYPE_LEN     1
	#define L2_VP_PORT_POST_TBL_VP_TYPE_DEFAULT 0x0
	/*[field] VP_CONTEXT_ACTIVE*/
	#define L2_VP_PORT_POST_TBL_VP_CONTEXT_ACTIVE
	#define L2_VP_PORT_POST_TBL_VP_CONTEXT_ACTIVE_OFFSET  14
	#define L2_VP_PORT_POST_TBL_VP_CONTEXT_ACTIVE_LEN     1
	#define L2_VP_PORT_POST_TBL_VP_CONTEXT_ACTIVE_DEFAULT 0x0
	/*[field] VP_EG_DATA_VALID*/
	#define L2_VP_PORT_POST_TBL_VP_EG_DATA_VALID
	#define L2_VP_PORT_POST_TBL_VP_EG_DATA_VALID_OFFSET  15
	#define L2_VP_PORT_POST_TBL_VP_EG_DATA_VALID_LEN     1
	#define L2_VP_PORT_POST_TBL_VP_EG_DATA_VALID_DEFAULT 0x0
	/*[field] PHYSICAL_PORT_MTU_CHECK_EN*/
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_MTU_CHECK_EN
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_MTU_CHECK_EN_OFFSET  16
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_MTU_CHECK_EN_LEN     1
	#define L2_VP_PORT_POST_TBL_PHYSICAL_PORT_MTU_CHECK_EN_DEFAULT 0x0
	/*[field] MTU_CHECK_TYPE*/
	#define L2_VP_PORT_POST_TBL_MTU_CHECK_TYPE
	#define L2_VP_PORT_POST_TBL_MTU_CHECK_TYPE_OFFSET  17
	#define L2_VP_PORT_POST_TBL_MTU_CHECK_TYPE_LEN     1
	#define L2_VP_PORT_POST_TBL_MTU_CHECK_TYPE_DEFAULT 0x0
	/*[field] EXTRA_HEADER_LEN*/
	#define L2_VP_PORT_POST_TBL_EXTRA_HEADER_LEN
	#define L2_VP_PORT_POST_TBL_EXTRA_HEADER_LEN_OFFSET  18
	#define L2_VP_PORT_POST_TBL_EXTRA_HEADER_LEN_LEN     8
	#define L2_VP_PORT_POST_TBL_EXTRA_HEADER_LEN_DEFAULT 0x0
	/*[field] EG_VLAN_FMT_VALID*/
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FMT_VALID
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FMT_VALID_OFFSET  26
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FMT_VALID_LEN     1
	#define L2_VP_PORT_POST_TBL_EG_VLAN_FMT_VALID_DEFAULT 0x0
	/*[field] EG_STAG_FMT*/
	#define L2_VP_PORT_POST_TBL_EG_STAG_FMT
	#define L2_VP_PORT_POST_TBL_EG_STAG_FMT_OFFSET  27
	#define L2_VP_PORT_POST_TBL_EG_STAG_FMT_LEN     1
	#define L2_VP_PORT_POST_TBL_EG_STAG_FMT_DEFAULT 0x0
	/*[field] EG_CTAG_FMT*/
	#define L2_VP_PORT_POST_TBL_EG_CTAG_FMT
	#define L2_VP_PORT_POST_TBL_EG_CTAG_FMT_OFFSET  28
	#define L2_VP_PORT_POST_TBL_EG_CTAG_FMT_LEN     1
	#define L2_VP_PORT_POST_TBL_EG_CTAG_FMT_DEFAULT 0x0
	/*[field] ENQ_SERVICE_CODE_EN*/
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_EN
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_EN_OFFSET  29
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_EN_LEN     1
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_EN_DEFAULT 0x0
	/*[field] ENQ_SERVICE_CODE*/
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_OFFSET  30
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_LEN     8
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_DEFAULT 0x0
	/*[field] ENQ_PHY_PORT*/
	#define L2_VP_PORT_POST_TBL_ENQ_PHY_PORT
	#define L2_VP_PORT_POST_TBL_ENQ_PHY_PORT_OFFSET  38
	#define L2_VP_PORT_POST_TBL_ENQ_PHY_PORT_LEN     4
	#define L2_VP_PORT_POST_TBL_ENQ_PHY_PORT_DEFAULT 0x0
	/*[field] MIRROR_EN*/
	#define L2_VP_PORT_POST_TBL_MIRROR_EN
	#define L2_VP_PORT_POST_TBL_MIRROR_EN_OFFSET  42
	#define L2_VP_PORT_POST_TBL_MIRROR_EN_LEN     1
	#define L2_VP_PORT_POST_TBL_MIRROR_EN_DEFAULT 0x0
	/*[field] ENQ_SERVICE_CODE_QUEUE_DIS*/
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_QUEUE_DIS
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_QUEUE_DIS_OFFSET  43
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_QUEUE_DIS_LEN     1
	#define L2_VP_PORT_POST_TBL_ENQ_SERVICE_CODE_QUEUE_DIS_DEFAULT 0x0
	/*[field] VP_PROFILE*/
	#define L2_VP_PORT_POST_TBL_VP_PROFILE
	#define L2_VP_PORT_POST_TBL_VP_PROFILE_OFFSET  44
	#define L2_VP_PORT_POST_TBL_VP_PROFILE_LEN     8
	#define L2_VP_PORT_POST_TBL_VP_PROFILE_DEFAULT 0x0

struct l2_vp_port_post_tbl {
	a_uint32_t  physical_port:4;
	a_uint32_t  eg_vlan_fltr_cmd:1;
	a_uint32_t  isol_profile:6;
	a_uint32_t  isol_profile_en:1;
	a_uint32_t  vp_state_check_en:1;
	a_uint32_t  vp_type:1;
	a_uint32_t  vp_context_active:1;
	a_uint32_t  vp_eg_data_valid:1;
	a_uint32_t  physical_port_mtu_check_en:1;
	a_uint32_t  mtu_check_type:1;
	a_uint32_t  extra_header_len:8;
	a_uint32_t  eg_vlan_fmt_valid:1;
	a_uint32_t  eg_stag_fmt:1;
	a_uint32_t  eg_ctag_fmt:1;
	a_uint32_t  enq_service_code_en:1;
	a_uint32_t  enq_service_code_0:2;
	a_uint32_t  enq_service_code_1:6;
	a_uint32_t  enq_phy_port:4;
	a_uint32_t  mirror_en:1;
	a_uint32_t  enq_service_code_queue_dis:1;
	a_uint32_t  vp_profile:8;
	a_uint32_t  _reserved0:12;
};

union l2_vp_port_post_tbl_u {
	a_uint32_t val[2];
	struct l2_vp_port_post_tbl bf;
};

/*[table] VP_ISOL_ACTION_TBL*/
#define VP_ISOL_ACTION_TBL
#define VP_ISOL_ACTION_TBL_ADDRESS 0x28800
#define VP_ISOL_ACTION_TBL_NUM     64
#define VP_ISOL_ACTION_TBL_INC     0x10
#define VP_ISOL_ACTION_TBL_TYPE    REG_TYPE_RW
#define VP_ISOL_ACTION_TBL_DEFAULT 0x0
	/*[field] VP_PROFILE_ACTION*/
	#define VP_ISOL_ACTION_TBL_VP_PROFILE_ACTION
	#define VP_ISOL_ACTION_TBL_VP_PROFILE_ACTION_OFFSET  0
	#define VP_ISOL_ACTION_TBL_VP_PROFILE_ACTION_LEN     128
	#define VP_ISOL_ACTION_TBL_VP_PROFILE_ACTION_DEFAULT 0x0

struct vp_isol_action_tbl {
	a_uint32_t  vp_profile_action_0:32;
	a_uint32_t  vp_profile_action_1:32;
	a_uint32_t  vp_profile_action_2:32;
	a_uint32_t  vp_profile_action_3:32;
};

union vp_isol_action_tbl_u {
	a_uint32_t val[4];
	struct vp_isol_action_tbl bf;
};

#endif
