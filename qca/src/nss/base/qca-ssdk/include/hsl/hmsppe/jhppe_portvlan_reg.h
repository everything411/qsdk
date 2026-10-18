/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_PORTVLAN_REG_H_
#define _JHPPE_PORTVLAN_REG_H_

/*[register] TPR_VLAN_TPID_EXT0*/
#define TPR_VLAN_TPID_EXT0
#define TPR_VLAN_TPID_EXT0_ADDRESS 0xe00
#define TPR_VLAN_TPID_EXT0_NUM     1
#define TPR_VLAN_TPID_EXT0_INC     0x4
#define TPR_VLAN_TPID_EXT0_TYPE    REG_TYPE_RW
#define TPR_VLAN_TPID_EXT0_DEFAULT 0x88a88100
	/*[field] CTAG_TPID*/
	#define TPR_VLAN_TPID_EXT0_CTAG_TPID
	#define TPR_VLAN_TPID_EXT0_CTAG_TPID_OFFSET  0
	#define TPR_VLAN_TPID_EXT0_CTAG_TPID_LEN     16
	#define TPR_VLAN_TPID_EXT0_CTAG_TPID_DEFAULT 0x8100
	/*[field] STAG_TPID*/
	#define TPR_VLAN_TPID_EXT0_STAG_TPID
	#define TPR_VLAN_TPID_EXT0_STAG_TPID_OFFSET  16
	#define TPR_VLAN_TPID_EXT0_STAG_TPID_LEN     16
	#define TPR_VLAN_TPID_EXT0_STAG_TPID_DEFAULT 0x88a8

struct tpr_vlan_tpid_ext0 {
	a_uint32_t  ctag_tpid:16;
	a_uint32_t  stag_tpid:16;
};

union tpr_vlan_tpid_ext0_u {
	a_uint32_t val;
	struct tpr_vlan_tpid_ext0 bf;
};

/*[register] TPR_VLAN_TPID_EXT1*/
#define TPR_VLAN_TPID_EXT1
#define TPR_VLAN_TPID_EXT1_ADDRESS 0xe10
#define TPR_VLAN_TPID_EXT1_NUM     1
#define TPR_VLAN_TPID_EXT1_INC     0x4
#define TPR_VLAN_TPID_EXT1_TYPE    REG_TYPE_RW
#define TPR_VLAN_TPID_EXT1_DEFAULT 0x21
	/*[field] CTAG_TPID_MAP*/
	#define TPR_VLAN_TPID_EXT1_CTAG_TPID_MAP
	#define TPR_VLAN_TPID_EXT1_CTAG_TPID_MAP_OFFSET  0
	#define TPR_VLAN_TPID_EXT1_CTAG_TPID_MAP_LEN     4
	#define TPR_VLAN_TPID_EXT1_CTAG_TPID_MAP_DEFAULT 0x1
	/*[field] STAG_TPID_MAP*/
	#define TPR_VLAN_TPID_EXT1_STAG_TPID_MAP
	#define TPR_VLAN_TPID_EXT1_STAG_TPID_MAP_OFFSET  4
	#define TPR_VLAN_TPID_EXT1_STAG_TPID_MAP_LEN     4
	#define TPR_VLAN_TPID_EXT1_STAG_TPID_MAP_DEFAULT 0x2

struct tpr_vlan_tpid_ext1 {
	a_uint32_t  ctag_tpid_map:4;
	a_uint32_t  stag_tpid_map:4;
	a_uint32_t  _reserved0:24;
};

union tpr_vlan_tpid_ext1_u {
	a_uint32_t val;
	struct tpr_vlan_tpid_ext1 bf;
};

/*[register] VLAN_TPID_REG_EXT0*/
#define VLAN_TPID_REG_EXT0
#define VLAN_TPID_REG_EXT0_ADDRESS 0x510
#define VLAN_TPID_REG_EXT0_NUM     1
#define VLAN_TPID_REG_EXT0_INC     0x4
#define VLAN_TPID_REG_EXT0_TYPE    REG_TYPE_RW
#define VLAN_TPID_REG_EXT0_DEFAULT 0x88a88100
	/*[field] CTAG_TPID*/
	#define VLAN_TPID_REG_EXT0_CTAG_TPID
	#define VLAN_TPID_REG_EXT0_CTAG_TPID_OFFSET  0
	#define VLAN_TPID_REG_EXT0_CTAG_TPID_LEN     16
	#define VLAN_TPID_REG_EXT0_CTAG_TPID_DEFAULT 0x8100
	/*[field] STAG_TPID*/
	#define VLAN_TPID_REG_EXT0_STAG_TPID
	#define VLAN_TPID_REG_EXT0_STAG_TPID_OFFSET  16
	#define VLAN_TPID_REG_EXT0_STAG_TPID_LEN     16
	#define VLAN_TPID_REG_EXT0_STAG_TPID_DEFAULT 0x88a8

struct vlan_tpid_reg_ext0 {
	a_uint32_t  ctag_tpid:16;
	a_uint32_t  stag_tpid:16;
};

union vlan_tpid_reg_ext0_u {
	a_uint32_t val;
	struct vlan_tpid_reg_ext0 bf;
};

/*[register] VLAN_TPID_REG_EXT1*/
#define VLAN_TPID_REG_EXT1
#define VLAN_TPID_REG_EXT1_ADDRESS 0x520
#define VLAN_TPID_REG_EXT1_NUM     1
#define VLAN_TPID_REG_EXT1_INC     0x4
#define VLAN_TPID_REG_EXT1_TYPE    REG_TYPE_RW
#define VLAN_TPID_REG_EXT1_DEFAULT 0x21
	/*[field] CTAG_TPID_MAP*/
	#define VLAN_TPID_REG_EXT1_CTAG_TPID_MAP
	#define VLAN_TPID_REG_EXT1_CTAG_TPID_MAP_OFFSET  0
	#define VLAN_TPID_REG_EXT1_CTAG_TPID_MAP_LEN     4
	#define VLAN_TPID_REG_EXT1_CTAG_TPID_MAP_DEFAULT 0x1
	/*[field] STAG_TPID_MAP*/
	#define VLAN_TPID_REG_EXT1_STAG_TPID_MAP
	#define VLAN_TPID_REG_EXT1_STAG_TPID_MAP_OFFSET  4
	#define VLAN_TPID_REG_EXT1_STAG_TPID_MAP_LEN     4
	#define VLAN_TPID_REG_EXT1_STAG_TPID_MAP_DEFAULT 0x2

struct vlan_tpid_reg_ext1 {
	a_uint32_t  ctag_tpid_map:4;
	a_uint32_t  stag_tpid_map:4;
	a_uint32_t  _reserved0:24;
};

union vlan_tpid_reg_ext1_u {
	a_uint32_t val;
	struct vlan_tpid_reg_ext1 bf;
};

/*[table] IN_VLAN_DSCP_PBIT_MAP_TBL*/
#define IN_VLAN_DSCP_PBIT_MAP_TBL
#define IN_VLAN_DSCP_PBIT_MAP_TBL_ADDRESS 0x8000
#define IN_VLAN_DSCP_PBIT_MAP_TBL_NUM     256
#define IN_VLAN_DSCP_PBIT_MAP_TBL_INC     0x4
#define IN_VLAN_DSCP_PBIT_MAP_TBL_TYPE    REG_TYPE_RW
#define IN_VLAN_DSCP_PBIT_MAP_TBL_DEFAULT 0x0
	/*[field] PCP_0*/
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_0
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_0_OFFSET  0
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_0_LEN     3
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_0_DEFAULT 0x0
	/*[field] PCP_1*/
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_1
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_1_OFFSET  3
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_1_LEN     3
	#define IN_VLAN_DSCP_PBIT_MAP_TBL_PCP_1_DEFAULT 0x0

struct in_vlan_dscp_pbit_map_tbl {
	a_uint32_t  pcp_0:3;
	a_uint32_t  pcp_1:3;
	a_uint32_t  _reserved0:26;
};

union in_vlan_dscp_pbit_map_tbl_u {
	a_uint32_t val;
	struct in_vlan_dscp_pbit_map_tbl bf;
};

/*[register] EG_VLAN_TPID_EXT0*/
#define EG_VLAN_TPID_EXT0
#define EG_VLAN_TPID_EXT0_ADDRESS 0x130
#define EG_VLAN_TPID_EXT0_NUM     1
#define EG_VLAN_TPID_EXT0_INC     0x4
#define EG_VLAN_TPID_EXT0_TYPE    REG_TYPE_RW
#define EG_VLAN_TPID_EXT0_DEFAULT 0x810088a8
	/*[field] STPID*/
	#define EG_VLAN_TPID_EXT0_STPID
	#define EG_VLAN_TPID_EXT0_STPID_OFFSET  0
	#define EG_VLAN_TPID_EXT0_STPID_LEN     16
	#define EG_VLAN_TPID_EXT0_STPID_DEFAULT 0x88a8
	/*[field] CTPID*/
	#define EG_VLAN_TPID_EXT0_CTPID
	#define EG_VLAN_TPID_EXT0_CTPID_OFFSET  16
	#define EG_VLAN_TPID_EXT0_CTPID_LEN     16
	#define EG_VLAN_TPID_EXT0_CTPID_DEFAULT 0x8100

struct eg_vlan_tpid_ext0 {
	a_uint32_t  stpid:16;
	a_uint32_t  ctpid:16;
};

union eg_vlan_tpid_ext0_u {
	a_uint32_t val;
	struct eg_vlan_tpid_ext0 bf;
};

/*[register] EG_VLAN_TPID_EXT1*/
#define EG_VLAN_TPID_EXT1
#define EG_VLAN_TPID_EXT1_ADDRESS 0x134
#define EG_VLAN_TPID_EXT1_NUM     1
#define EG_VLAN_TPID_EXT1_INC     0x4
#define EG_VLAN_TPID_EXT1_TYPE    REG_TYPE_RW
#define EG_VLAN_TPID_EXT1_DEFAULT 0x21
	/*[field] CTAG_TPID_MAP*/
	#define EG_VLAN_TPID_EXT1_CTAG_TPID_MAP
	#define EG_VLAN_TPID_EXT1_CTAG_TPID_MAP_OFFSET  0
	#define EG_VLAN_TPID_EXT1_CTAG_TPID_MAP_LEN     4
	#define EG_VLAN_TPID_EXT1_CTAG_TPID_MAP_DEFAULT 0x1
	/*[field] STAG_TPID_MAP*/
	#define EG_VLAN_TPID_EXT1_STAG_TPID_MAP
	#define EG_VLAN_TPID_EXT1_STAG_TPID_MAP_OFFSET  4
	#define EG_VLAN_TPID_EXT1_STAG_TPID_MAP_LEN     4
	#define EG_VLAN_TPID_EXT1_STAG_TPID_MAP_DEFAULT 0x2

struct eg_vlan_tpid_ext1 {
	a_uint32_t  ctag_tpid_map:4;
	a_uint32_t  stag_tpid_map:4;
	a_uint32_t  _reserved0:24;
};

union eg_vlan_tpid_ext1_u {
	a_uint32_t val;
	struct eg_vlan_tpid_ext1 bf;
};

/*[table] DSCP_PBIT_MAP_TBL*/
#define DSCP_PBIT_MAP_TBL
#define DSCP_PBIT_MAP_TBL_ADDRESS 0x28800
#define DSCP_PBIT_MAP_TBL_NUM     256
#define DSCP_PBIT_MAP_TBL_INC     0x4
#define DSCP_PBIT_MAP_TBL_TYPE    REG_TYPE_RW
#define DSCP_PBIT_MAP_TBL_DEFAULT 0x0
	/*[field] PCP_0*/
	#define DSCP_PBIT_MAP_TBL_PCP_0
	#define DSCP_PBIT_MAP_TBL_PCP_0_OFFSET  0
	#define DSCP_PBIT_MAP_TBL_PCP_0_LEN     3
	#define DSCP_PBIT_MAP_TBL_PCP_0_DEFAULT 0x0
	/*[field] PCP_1*/
	#define DSCP_PBIT_MAP_TBL_PCP_1
	#define DSCP_PBIT_MAP_TBL_PCP_1_OFFSET  3
	#define DSCP_PBIT_MAP_TBL_PCP_1_LEN     3
	#define DSCP_PBIT_MAP_TBL_PCP_1_DEFAULT 0x0

struct dscp_pbit_map_tbl {
	a_uint32_t  pcp_0:3;
	a_uint32_t  pcp_1:3;
	a_uint32_t  _reserved0:26;
};

union dscp_pbit_map_tbl_u {
	a_uint32_t val;
	struct dscp_pbit_map_tbl bf;
};

/*[register] VLAN_TO_PORT_MAPPING_CTRL*/
#define VLAN_TO_PORT_MAPPING_CTRL
#define VLAN_TO_PORT_MAPPING_CTRL_ADDRESS 0x104
#define VLAN_TO_PORT_MAPPING_CTRL_NUM     8
#define VLAN_TO_PORT_MAPPING_CTRL_INC     0x4
#define VLAN_TO_PORT_MAPPING_CTRL_TYPE    REG_TYPE_RW
#define VLAN_TO_PORT_MAPPING_CTRL_DEFAULT 0x0
	/*[field] TPID*/
	#define VLAN_TO_PORT_MAPPING_CTRL_TPID
	#define VLAN_TO_PORT_MAPPING_CTRL_TPID_OFFSET  0
	#define VLAN_TO_PORT_MAPPING_CTRL_TPID_LEN     16
	#define VLAN_TO_PORT_MAPPING_CTRL_TPID_DEFAULT 0x0
	/*[field] PRIVATE_TAG_EN*/
	#define VLAN_TO_PORT_MAPPING_CTRL_PRIVATE_TAG_EN
	#define VLAN_TO_PORT_MAPPING_CTRL_PRIVATE_TAG_EN_OFFSET  16
	#define VLAN_TO_PORT_MAPPING_CTRL_PRIVATE_TAG_EN_LEN     1
	#define VLAN_TO_PORT_MAPPING_CTRL_PRIVATE_TAG_EN_DEFAULT 0x0

struct vlan_to_port_mapping_ctrl {
	a_uint32_t  tpid:16;
	a_uint32_t  private_tag_en:1;
	a_uint32_t  _reserved0:15;
};

union vlan_to_port_mapping_ctrl_u {
	a_uint32_t val;
	struct vlan_to_port_mapping_ctrl bf;
};

/*[register] VLAN_TO_PORT_MAPPING_TBL*/
#define VLAN_TO_PORT_MAPPING_TBL
#define VLAN_TO_PORT_MAPPING_TBL_ADDRESS 0x200
#define VLAN_TO_PORT_MAPPING_TBL_NUM     32
#define VLAN_TO_PORT_MAPPING_TBL_INC     0x10
#define VLAN_TO_PORT_MAPPING_TBL_TYPE    REG_TYPE_RW
#define VLAN_TO_PORT_MAPPING_TBL_DEFAULT 0x0
	/*[field] TCI_VALUE*/
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_VALUE
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_VALUE_OFFSET  0
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_VALUE_LEN     16
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_VALUE_DEFAULT 0x0
	/*[field] TCI_MASK*/
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_MASK
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_MASK_OFFSET  16
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_MASK_LEN     16
	#define VLAN_TO_PORT_MAPPING_TBL_TCI_MASK_DEFAULT 0x0
	/*[field] PORT_VP*/
	#define VLAN_TO_PORT_MAPPING_TBL_PORT_VP
	#define VLAN_TO_PORT_MAPPING_TBL_PORT_VP_OFFSET  32
	#define VLAN_TO_PORT_MAPPING_TBL_PORT_VP_LEN     8
	#define VLAN_TO_PORT_MAPPING_TBL_PORT_VP_DEFAULT 0x0
	/*[field] VALID*/
	#define VLAN_TO_PORT_MAPPING_TBL_VALID
	#define VLAN_TO_PORT_MAPPING_TBL_VALID_OFFSET  40
	#define VLAN_TO_PORT_MAPPING_TBL_VALID_LEN     1
	#define VLAN_TO_PORT_MAPPING_TBL_VALID_DEFAULT 0x0

struct vlan_to_port_mapping_tbl {
	a_uint32_t  tci_value:16;
	a_uint32_t  tci_mask:16;
	a_uint32_t  port_vp:8;
	a_uint32_t  valid:1;
	a_uint32_t  _reserved0:23;
};

union vlan_to_port_mapping_tbl_u {
	a_uint32_t val[2];
	struct vlan_to_port_mapping_tbl bf;
};

#endif
