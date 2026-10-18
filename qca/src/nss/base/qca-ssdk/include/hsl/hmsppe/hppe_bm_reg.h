/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_BM_REG_H_
#define _HPPE_BM_REG_H_

/*[register] PORT_FC_MODE*/
#define PORT_FC_MODE
#define PORT_FC_MODE_ADDRESS 0x100
#define PORT_FC_MODE_NUM     40
#define PORT_FC_MODE_INC     0x4
#define PORT_FC_MODE_TYPE    REG_TYPE_RW
#define PORT_FC_MODE_DEFAULT 0x0
	/*[field] FC_EN*/
	#define PORT_FC_MODE_FC_EN
	#define PORT_FC_MODE_FC_EN_OFFSET  0
	#define PORT_FC_MODE_FC_EN_LEN     1
	#define PORT_FC_MODE_FC_EN_DEFAULT 0x0

struct port_fc_mode {
	a_uint32_t  fc_en:1;
	a_uint32_t  _reserved0:31;
};

union port_fc_mode_u {
	a_uint32_t val;
	struct port_fc_mode bf;
};

/*[register] PORT_GROUP_ID*/
#define PORT_GROUP_ID
#define PORT_GROUP_ID_ADDRESS 0x240
#define PORT_GROUP_ID_NUM     40
#define PORT_GROUP_ID_INC     0x4
#define PORT_GROUP_ID_TYPE    REG_TYPE_RW
#define PORT_GROUP_ID_DEFAULT 0x0
	/*[field] PORT_SHARED_GROUP_ID*/
	#define PORT_GROUP_ID_PORT_SHARED_GROUP_ID
	#define PORT_GROUP_ID_PORT_SHARED_GROUP_ID_OFFSET  0
	#define PORT_GROUP_ID_PORT_SHARED_GROUP_ID_LEN     2
	#define PORT_GROUP_ID_PORT_SHARED_GROUP_ID_DEFAULT 0x0

struct port_group_id {
	a_uint32_t  port_shared_group_id:2;
	a_uint32_t  _reserved0:30;
};

union port_group_id_u {
	a_uint32_t val;
	struct port_group_id bf;
};

/*[register] PORT_CNT*/
#define PORT_CNT
#define PORT_CNT_ADDRESS 0x2e0
#define PORT_CNT_NUM     40
#define PORT_CNT_INC     0x4
#define PORT_CNT_TYPE    REG_TYPE_RO
#define PORT_CNT_DEFAULT 0x0
	/*[field] PORT_CNT*/
	#define PORT_CNT_PORT_CNT
	#define PORT_CNT_PORT_CNT_OFFSET  0
	#define PORT_CNT_PORT_CNT_LEN     12
	#define PORT_CNT_PORT_CNT_DEFAULT 0x0

struct port_cnt {
	a_uint32_t  port_cnt:12;
	a_uint32_t  _reserved0:20;
};

union port_cnt_u {
	a_uint32_t val;
	struct port_cnt bf;
};

/*[register] PORT_REACTED_CNT*/
#define PORT_REACTED_CNT
#define PORT_REACTED_CNT_ADDRESS 0x380
#define PORT_REACTED_CNT_NUM     40
#define PORT_REACTED_CNT_INC     0x4
#define PORT_REACTED_CNT_TYPE    REG_TYPE_RO
#define PORT_REACTED_CNT_DEFAULT 0x0
	/*[field] PORT_REACTED_CNT*/
	#define PORT_REACTED_CNT_PORT_REACTED_CNT
	#define PORT_REACTED_CNT_PORT_REACTED_CNT_OFFSET  0
	#define PORT_REACTED_CNT_PORT_REACTED_CNT_LEN     10
	#define PORT_REACTED_CNT_PORT_REACTED_CNT_DEFAULT 0x0

struct port_reacted_cnt {
	a_uint32_t  port_reacted_cnt:10;
	a_uint32_t  _reserved0:22;
};

union port_reacted_cnt_u {
	a_uint32_t val;
	struct port_reacted_cnt bf;
};

/*[register] SHARED_GROUP_CFG*/
#define SHARED_GROUP_CFG
#define SHARED_GROUP_CFG_ADDRESS 0x460
#define SHARED_GROUP_CFG_NUM     4
#define SHARED_GROUP_CFG_INC     0x4
#define SHARED_GROUP_CFG_TYPE    REG_TYPE_RW
#define SHARED_GROUP_CFG_DEFAULT 0x0
	/*[field] SHARED_GROUP_LIMIT*/
	#define SHARED_GROUP_CFG_SHARED_GROUP_LIMIT
	#define SHARED_GROUP_CFG_SHARED_GROUP_LIMIT_OFFSET  0
	#define SHARED_GROUP_CFG_SHARED_GROUP_LIMIT_LEN     12
	#define SHARED_GROUP_CFG_SHARED_GROUP_LIMIT_DEFAULT 0x0

struct shared_group_cfg {
	a_uint32_t  shared_group_limit:12;
	a_uint32_t  _reserved0:20;
};

union shared_group_cfg_u {
	a_uint32_t val;
	struct shared_group_cfg bf;
};

/*[table] PORT_FC_CFG*/
#define PORT_FC_CFG
#define PORT_FC_CFG_ADDRESS 0x1000
#define PORT_FC_CFG_NUM     40
#define PORT_FC_CFG_INC     0x10
#define PORT_FC_CFG_TYPE    REG_TYPE_RW
#define PORT_FC_CFG_DEFAULT 0x0
	/*[field] PORT_REACT_LIMIT*/
	#define PORT_FC_CFG_PORT_REACT_LIMIT
	#define PORT_FC_CFG_PORT_REACT_LIMIT_OFFSET  0
	#define PORT_FC_CFG_PORT_REACT_LIMIT_LEN     10
	#define PORT_FC_CFG_PORT_REACT_LIMIT_DEFAULT 0x0
	/*[field] PORT_RESUME_FLOOR_TH*/
	#define PORT_FC_CFG_PORT_RESUME_FLOOR_TH
	#define PORT_FC_CFG_PORT_RESUME_FLOOR_TH_OFFSET  10
	#define PORT_FC_CFG_PORT_RESUME_FLOOR_TH_LEN     9
	#define PORT_FC_CFG_PORT_RESUME_FLOOR_TH_DEFAULT 0x0
	/*[field] PORT_RESUME_OFFSET*/
	#define PORT_FC_CFG_PORT_RESUME_OFFSET
	#define PORT_FC_CFG_PORT_RESUME_OFFSET_OFFSET  19
	#define PORT_FC_CFG_PORT_RESUME_OFFSET_LEN     12
	#define PORT_FC_CFG_PORT_RESUME_OFFSET_DEFAULT 0x0
	/*[field] PORT_SHARED_CEILING*/
	#define PORT_FC_CFG_PORT_SHARED_CEILING
	#define PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET  31
	#define PORT_FC_CFG_PORT_SHARED_CEILING_LEN     12
	#define PORT_FC_CFG_PORT_SHARED_CEILING_DEFAULT 0x0
	/*[field] PORT_SHARED_WEIGHT*/
	#define PORT_FC_CFG_PORT_SHARED_WEIGHT
	#define PORT_FC_CFG_PORT_SHARED_WEIGHT_OFFSET  43
	#define PORT_FC_CFG_PORT_SHARED_WEIGHT_LEN     3
	#define PORT_FC_CFG_PORT_SHARED_WEIGHT_DEFAULT 0x0
	/*[field] PORT_SHARED_DYNAMIC*/
	#define PORT_FC_CFG_PORT_SHARED_DYNAMIC
	#define PORT_FC_CFG_PORT_SHARED_DYNAMIC_OFFSET  46
	#define PORT_FC_CFG_PORT_SHARED_DYNAMIC_LEN     1
	#define PORT_FC_CFG_PORT_SHARED_DYNAMIC_DEFAULT 0x0
	/*[field] PORT_PRE_ALLOC*/
	#define PORT_FC_CFG_PORT_PRE_ALLOC
	#define PORT_FC_CFG_PORT_PRE_ALLOC_OFFSET  47
	#define PORT_FC_CFG_PORT_PRE_ALLOC_LEN     12
	#define PORT_FC_CFG_PORT_PRE_ALLOC_DEFAULT 0x0

struct port_fc_cfg {
	a_uint32_t  port_react_limit:10;
	a_uint32_t  port_resume_floor_th:9;
	a_uint32_t  port_resume_offset:12;
	a_uint32_t  port_shared_ceiling_0:1;
	a_uint32_t  port_shared_ceiling_1:11;
	a_uint32_t  port_shared_weight:3;
	a_uint32_t  port_shared_dynamic:1;
	a_uint32_t  port_pre_alloc:12;
	a_uint32_t  _reserved0:5;
};

union port_fc_cfg_u {
	a_uint32_t val[2];
	struct port_fc_cfg bf;
};

#endif
