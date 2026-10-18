/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_PORTVLAN_REG_H_
#define _HPPE_PORTVLAN_REG_H_

/*[register] PORT_PARSING_REG*/
#define PORT_PARSING_REG
#define PORT_PARSING_REG_ADDRESS 0x0
#define PORT_PARSING_REG_NUM     9
#define PORT_PARSING_REG_INC     0x4
#define PORT_PARSING_REG_TYPE    REG_TYPE_RW
#define PORT_PARSING_REG_DEFAULT 0x0
	/*[field] PORT_ROLE*/
	#define PORT_PARSING_REG_PORT_ROLE
	#define PORT_PARSING_REG_PORT_ROLE_OFFSET  0
	#define PORT_PARSING_REG_PORT_ROLE_LEN     1
	#define PORT_PARSING_REG_PORT_ROLE_DEFAULT 0x0
	/*[field] SRC_PORT_SEL*/
	#define PORT_PARSING_REG_SRC_PORT_SEL
	#define PORT_PARSING_REG_SRC_PORT_SEL_OFFSET  1
	#define PORT_PARSING_REG_SRC_PORT_SEL_LEN     1
	#define PORT_PARSING_REG_SRC_PORT_SEL_DEFAULT 0x0
	/*[field] VLAN_MODE*/
	#define PORT_PARSING_REG_VLAN_MODE
	#define PORT_PARSING_REG_VLAN_MODE_OFFSET  2
	#define PORT_PARSING_REG_VLAN_MODE_LEN     1
	#define PORT_PARSING_REG_VLAN_MODE_DEFAULT 0x0

struct port_parsing_reg {
	a_uint32_t  port_role:1;
	a_uint32_t  src_port_sel:1;
	a_uint32_t  vlan_mode:1;
	a_uint32_t  _reserved0:29;
};

union port_parsing_reg_u {
	a_uint32_t val;
	struct port_parsing_reg bf;
};

/*[register] VLAN_TPID_REG*/
#define VLAN_TPID_REG
#define VLAN_TPID_REG_ADDRESS 0x40
#define VLAN_TPID_REG_NUM     1
#define VLAN_TPID_REG_INC     0x4
#define VLAN_TPID_REG_TYPE    REG_TYPE_RW
#define VLAN_TPID_REG_DEFAULT 0x88a88100
	/*[field] CTAG_TPID*/
	#define VLAN_TPID_REG_CTAG_TPID
	#define VLAN_TPID_REG_CTAG_TPID_OFFSET  0
	#define VLAN_TPID_REG_CTAG_TPID_LEN     16
	#define VLAN_TPID_REG_CTAG_TPID_DEFAULT 0x8100
	/*[field] STAG_TPID*/
	#define VLAN_TPID_REG_STAG_TPID
	#define VLAN_TPID_REG_STAG_TPID_OFFSET  16
	#define VLAN_TPID_REG_STAG_TPID_LEN     16
	#define VLAN_TPID_REG_STAG_TPID_DEFAULT 0x88a8

struct vlan_tpid_reg {
	a_uint32_t  ctag_tpid:16;
	a_uint32_t  stag_tpid:16;
};

union vlan_tpid_reg_u {
	a_uint32_t val;
	struct vlan_tpid_reg bf;
};

/*[register] BRIDGE_CONFIG*/
#define BRIDGE_CONFIG
#define BRIDGE_CONFIG_ADDRESS 0x0
#define BRIDGE_CONFIG_NUM     1
#define BRIDGE_CONFIG_INC     0x4
#define BRIDGE_CONFIG_TYPE    REG_TYPE_RW
#define BRIDGE_CONFIG_DEFAULT 0x0
	/*[field] BRIDGE_TYPE*/
	#define BRIDGE_CONFIG_BRIDGE_TYPE
	#define BRIDGE_CONFIG_BRIDGE_TYPE_OFFSET  0
	#define BRIDGE_CONFIG_BRIDGE_TYPE_LEN     1
	#define BRIDGE_CONFIG_BRIDGE_TYPE_DEFAULT 0x0

struct bridge_config {
	a_uint32_t  bridge_type:1;
	a_uint32_t  _reserved0:31;
};

union bridge_config_u {
	a_uint32_t val;
	struct bridge_config bf;
};

/*[register] PORT_DEF_VID*/
#define PORT_DEF_VID
#define PORT_DEF_VID_ADDRESS 0x10
#define PORT_DEF_VID_NUM     9
#define PORT_DEF_VID_INC     0x4
#define PORT_DEF_VID_TYPE    REG_TYPE_RW
#define PORT_DEF_VID_DEFAULT 0x0
	/*[field] PORT_DEF_SVID*/
	#define PORT_DEF_VID_PORT_DEF_SVID
	#define PORT_DEF_VID_PORT_DEF_SVID_OFFSET  0
	#define PORT_DEF_VID_PORT_DEF_SVID_LEN     12
	#define PORT_DEF_VID_PORT_DEF_SVID_DEFAULT 0x0
	/*[field] PORT_DEF_SVID_EN*/
	#define PORT_DEF_VID_PORT_DEF_SVID_EN
	#define PORT_DEF_VID_PORT_DEF_SVID_EN_OFFSET  12
	#define PORT_DEF_VID_PORT_DEF_SVID_EN_LEN     1
	#define PORT_DEF_VID_PORT_DEF_SVID_EN_DEFAULT 0x0
	/*[field] PORT_DEF_CVID*/
	#define PORT_DEF_VID_PORT_DEF_CVID
	#define PORT_DEF_VID_PORT_DEF_CVID_OFFSET  16
	#define PORT_DEF_VID_PORT_DEF_CVID_LEN     12
	#define PORT_DEF_VID_PORT_DEF_CVID_DEFAULT 0x0
	/*[field] PORT_DEF_CVID_EN*/
	#define PORT_DEF_VID_PORT_DEF_CVID_EN
	#define PORT_DEF_VID_PORT_DEF_CVID_EN_OFFSET  28
	#define PORT_DEF_VID_PORT_DEF_CVID_EN_LEN     1
	#define PORT_DEF_VID_PORT_DEF_CVID_EN_DEFAULT 0x0

struct port_def_vid {
	a_uint32_t  port_def_svid:12;
	a_uint32_t  port_def_svid_en:1;
	a_uint32_t  _reserved0:3;
	a_uint32_t  port_def_cvid:12;
	a_uint32_t  port_def_cvid_en:1;
	a_uint32_t  _reserved1:3;
};

union port_def_vid_u {
	a_uint32_t val;
	struct port_def_vid bf;
};

/*[register] PORT_DEF_PCP*/
#define PORT_DEF_PCP
#define PORT_DEF_PCP_ADDRESS 0x50
#define PORT_DEF_PCP_NUM     9
#define PORT_DEF_PCP_INC     0x4
#define PORT_DEF_PCP_TYPE    REG_TYPE_RW
#define PORT_DEF_PCP_DEFAULT 0x0
	/*[field] PORT_DEF_SPCP*/
	#define PORT_DEF_PCP_PORT_DEF_SPCP
	#define PORT_DEF_PCP_PORT_DEF_SPCP_OFFSET  0
	#define PORT_DEF_PCP_PORT_DEF_SPCP_LEN     3
	#define PORT_DEF_PCP_PORT_DEF_SPCP_DEFAULT 0x0
	/*[field] PORT_DEF_SDEI*/
	#define PORT_DEF_PCP_PORT_DEF_SDEI
	#define PORT_DEF_PCP_PORT_DEF_SDEI_OFFSET  3
	#define PORT_DEF_PCP_PORT_DEF_SDEI_LEN     1
	#define PORT_DEF_PCP_PORT_DEF_SDEI_DEFAULT 0x0
	/*[field] PORT_DEF_CPCP*/
	#define PORT_DEF_PCP_PORT_DEF_CPCP
	#define PORT_DEF_PCP_PORT_DEF_CPCP_OFFSET  4
	#define PORT_DEF_PCP_PORT_DEF_CPCP_LEN     3
	#define PORT_DEF_PCP_PORT_DEF_CPCP_DEFAULT 0x0
	/*[field] PORT_DEF_CDEI*/
	#define PORT_DEF_PCP_PORT_DEF_CDEI
	#define PORT_DEF_PCP_PORT_DEF_CDEI_OFFSET  7
	#define PORT_DEF_PCP_PORT_DEF_CDEI_LEN     1
	#define PORT_DEF_PCP_PORT_DEF_CDEI_DEFAULT 0x0

struct port_def_pcp {
	a_uint32_t  port_def_spcp:3;
	a_uint32_t  port_def_sdei:1;
	a_uint32_t  port_def_cpcp:3;
	a_uint32_t  port_def_cdei:1;
	a_uint32_t  _reserved0:24;
};

union port_def_pcp_u {
	a_uint32_t val;
	struct port_def_pcp bf;
};

/*[register] PORT_VLAN_CONFIG*/
#define PORT_VLAN_CONFIG
#define PORT_VLAN_CONFIG_ADDRESS 0x90
#define PORT_VLAN_CONFIG_NUM     9
#define PORT_VLAN_CONFIG_INC     0x4
#define PORT_VLAN_CONFIG_TYPE    REG_TYPE_RW
#define PORT_VLAN_CONFIG_DEFAULT 0x0
	/*[field] PORT_IN_PCP_PROP_CMD*/
	#define PORT_VLAN_CONFIG_PORT_IN_PCP_PROP_CMD
	#define PORT_VLAN_CONFIG_PORT_IN_PCP_PROP_CMD_OFFSET  0
	#define PORT_VLAN_CONFIG_PORT_IN_PCP_PROP_CMD_LEN     1
	#define PORT_VLAN_CONFIG_PORT_IN_PCP_PROP_CMD_DEFAULT 0x0
	/*[field] PORT_IN_DEI_PROP_CMD*/
	#define PORT_VLAN_CONFIG_PORT_IN_DEI_PROP_CMD
	#define PORT_VLAN_CONFIG_PORT_IN_DEI_PROP_CMD_OFFSET  1
	#define PORT_VLAN_CONFIG_PORT_IN_DEI_PROP_CMD_LEN     1
	#define PORT_VLAN_CONFIG_PORT_IN_DEI_PROP_CMD_DEFAULT 0x0
	/*[field] PORT_UNTAG_FLTR_CMD*/
	#define PORT_VLAN_CONFIG_PORT_UNTAG_FLTR_CMD
	#define PORT_VLAN_CONFIG_PORT_UNTAG_FLTR_CMD_OFFSET  2
	#define PORT_VLAN_CONFIG_PORT_UNTAG_FLTR_CMD_LEN     1
	#define PORT_VLAN_CONFIG_PORT_UNTAG_FLTR_CMD_DEFAULT 0x0
	/*[field] PORT_PRI_TAG_FLTR_CMD*/
	#define PORT_VLAN_CONFIG_PORT_PRI_TAG_FLTR_CMD
	#define PORT_VLAN_CONFIG_PORT_PRI_TAG_FLTR_CMD_OFFSET  3
	#define PORT_VLAN_CONFIG_PORT_PRI_TAG_FLTR_CMD_LEN     1
	#define PORT_VLAN_CONFIG_PORT_PRI_TAG_FLTR_CMD_DEFAULT 0x0
	/*[field] PORT_TAG_FLTR_CMD*/
	#define PORT_VLAN_CONFIG_PORT_TAG_FLTR_CMD
	#define PORT_VLAN_CONFIG_PORT_TAG_FLTR_CMD_OFFSET  4
	#define PORT_VLAN_CONFIG_PORT_TAG_FLTR_CMD_LEN     1
	#define PORT_VLAN_CONFIG_PORT_TAG_FLTR_CMD_DEFAULT 0x0
	/*[field] PORT_VLAN_XLT_MISS_FWD_CMD*/
	#define PORT_VLAN_CONFIG_PORT_VLAN_XLT_MISS_FWD_CMD
	#define PORT_VLAN_CONFIG_PORT_VLAN_XLT_MISS_FWD_CMD_OFFSET  5
	#define PORT_VLAN_CONFIG_PORT_VLAN_XLT_MISS_FWD_CMD_LEN     2
	#define PORT_VLAN_CONFIG_PORT_VLAN_XLT_MISS_FWD_CMD_DEFAULT 0x0
	/*[field] PORT_IN_VLAN_FLTR_CMD*/
	#define PORT_VLAN_CONFIG_PORT_IN_VLAN_FLTR_CMD
	#define PORT_VLAN_CONFIG_PORT_IN_VLAN_FLTR_CMD_OFFSET  7
	#define PORT_VLAN_CONFIG_PORT_IN_VLAN_FLTR_CMD_LEN     1
	#define PORT_VLAN_CONFIG_PORT_IN_VLAN_FLTR_CMD_DEFAULT 0x0

struct port_vlan_config {
	a_uint32_t  port_in_pcp_prop_cmd:1;
	a_uint32_t  port_in_dei_prop_cmd:1;
	a_uint32_t  port_untag_fltr_cmd:1;
	a_uint32_t  port_pri_tag_fltr_cmd:1;
	a_uint32_t  port_tag_fltr_cmd:1;
	a_uint32_t  port_vlan_xlt_miss_fwd_cmd:2;
	a_uint32_t  port_in_vlan_fltr_cmd:1;
	a_uint32_t  _reserved0:24;
};

union port_vlan_config_u {
	a_uint32_t val;
	struct port_vlan_config bf;
};

/*[table] XLT_RULE_TBL*/
#define XLT_RULE_TBL
#define XLT_RULE_TBL_ADDRESS 0x2000
#define XLT_RULE_TBL_NUM     256
#define XLT_RULE_TBL_INC     0x10
#define XLT_RULE_TBL_TYPE    REG_TYPE_RW
#define XLT_RULE_TBL_DEFAULT 0x0
	/*[field] VALID*/
	#define XLT_RULE_TBL_VALID
	#define XLT_RULE_TBL_VALID_OFFSET  0
	#define XLT_RULE_TBL_VALID_LEN     1
	#define XLT_RULE_TBL_VALID_DEFAULT 0x0
	/*[field] PORT_BITMAP reuse PORT_TYPE[0]*/
	#define XLT_RULE_TBL_PORT_BITMAP
	#define XLT_RULE_TBL_PORT_BITMAP_OFFSET  1
	#define XLT_RULE_TBL_PORT_BITMAP_LEN     9
	#define XLT_RULE_TBL_PORT_BITMAP_DEFAULT 0x0
	/*[field] PORT_VP_ID reuse PORT_TYPE[1]*/
	#define XLT_RULE_TBL_PORT_VP_ID
	#define XLT_RULE_TBL_PORT_VP_ID_OFFSET  1
	#define XLT_RULE_TBL_PORT_VP_ID_LEN     9
	#define XLT_RULE_TBL_PORT_VP_ID_DEFAULT 0x0
	/*[field] VP_PROFILE reuse PORT_TYPE[2]*/
	#define XLT_RULE_TBL_VP_PROFILE
	#define XLT_RULE_TBL_VP_PROFILE_OFFSET  1
	#define XLT_RULE_TBL_VP_PROFILE_LEN     9
	#define XLT_RULE_TBL_VP_PROFILE_DEFAULT 0x0
	/*[field] GEM_PORT_ID reuse PORT_TYPE[3]*/
	#define XLT_RULE_TBL_GEM_PORT_ID
	#define XLT_RULE_TBL_GEM_PORT_ID_OFFSET  1
	#define XLT_RULE_TBL_GEM_PORT_ID_LEN     9
	#define XLT_RULE_TBL_GEM_PORT_ID_DEFAULT 0x0
	/*[field] SKEY_FMT*/
	#define XLT_RULE_TBL_SKEY_FMT
	#define XLT_RULE_TBL_SKEY_FMT_OFFSET  10
	#define XLT_RULE_TBL_SKEY_FMT_LEN     3
	#define XLT_RULE_TBL_SKEY_FMT_DEFAULT 0x0
	/*[field] SKEY_VID_INCL*/
	#define XLT_RULE_TBL_SKEY_VID_INCL
	#define XLT_RULE_TBL_SKEY_VID_INCL_OFFSET  13
	#define XLT_RULE_TBL_SKEY_VID_INCL_LEN     1
	#define XLT_RULE_TBL_SKEY_VID_INCL_DEFAULT 0x0
	/*[field] SKEY_VID*/
	#define XLT_RULE_TBL_SKEY_VID
	#define XLT_RULE_TBL_SKEY_VID_OFFSET  14
	#define XLT_RULE_TBL_SKEY_VID_LEN     12
	#define XLT_RULE_TBL_SKEY_VID_DEFAULT 0x0
	/*[field] SKEY_PCP_INCL*/
	#define XLT_RULE_TBL_SKEY_PCP_INCL
	#define XLT_RULE_TBL_SKEY_PCP_INCL_OFFSET  26
	#define XLT_RULE_TBL_SKEY_PCP_INCL_LEN     1
	#define XLT_RULE_TBL_SKEY_PCP_INCL_DEFAULT 0x0
	/*[field] SKEY_PCP*/
	#define XLT_RULE_TBL_SKEY_PCP
	#define XLT_RULE_TBL_SKEY_PCP_OFFSET  27
	#define XLT_RULE_TBL_SKEY_PCP_LEN     3
	#define XLT_RULE_TBL_SKEY_PCP_DEFAULT 0x0
	/*[field] SKEY_DEI_INCL*/
	#define XLT_RULE_TBL_SKEY_DEI_INCL
	#define XLT_RULE_TBL_SKEY_DEI_INCL_OFFSET  30
	#define XLT_RULE_TBL_SKEY_DEI_INCL_LEN     1
	#define XLT_RULE_TBL_SKEY_DEI_INCL_DEFAULT 0x0
	/*[field] SKEY_DEI*/
	#define XLT_RULE_TBL_SKEY_DEI
	#define XLT_RULE_TBL_SKEY_DEI_OFFSET  31
	#define XLT_RULE_TBL_SKEY_DEI_LEN     1
	#define XLT_RULE_TBL_SKEY_DEI_DEFAULT 0x0
	/*[field] CKEY_FMT*/
	#define XLT_RULE_TBL_CKEY_FMT
	#define XLT_RULE_TBL_CKEY_FMT_OFFSET  32
	#define XLT_RULE_TBL_CKEY_FMT_LEN     3
	#define XLT_RULE_TBL_CKEY_FMT_DEFAULT 0x0
	/*[field] CKEY_VID_INCL*/
	#define XLT_RULE_TBL_CKEY_VID_INCL
	#define XLT_RULE_TBL_CKEY_VID_INCL_OFFSET  35
	#define XLT_RULE_TBL_CKEY_VID_INCL_LEN     1
	#define XLT_RULE_TBL_CKEY_VID_INCL_DEFAULT 0x0
	/*[field] CKEY_VID*/
	#define XLT_RULE_TBL_CKEY_VID
	#define XLT_RULE_TBL_CKEY_VID_OFFSET  36
	#define XLT_RULE_TBL_CKEY_VID_LEN     12
	#define XLT_RULE_TBL_CKEY_VID_DEFAULT 0x0
	/*[field] CKEY_PCP_INCL*/
	#define XLT_RULE_TBL_CKEY_PCP_INCL
	#define XLT_RULE_TBL_CKEY_PCP_INCL_OFFSET  48
	#define XLT_RULE_TBL_CKEY_PCP_INCL_LEN     1
	#define XLT_RULE_TBL_CKEY_PCP_INCL_DEFAULT 0x0
	/*[field] CKEY_PCP*/
	#define XLT_RULE_TBL_CKEY_PCP
	#define XLT_RULE_TBL_CKEY_PCP_OFFSET  49
	#define XLT_RULE_TBL_CKEY_PCP_LEN     3
	#define XLT_RULE_TBL_CKEY_PCP_DEFAULT 0x0
	/*[field] CKEY_DEI_INCL*/
	#define XLT_RULE_TBL_CKEY_DEI_INCL
	#define XLT_RULE_TBL_CKEY_DEI_INCL_OFFSET  52
	#define XLT_RULE_TBL_CKEY_DEI_INCL_LEN     1
	#define XLT_RULE_TBL_CKEY_DEI_INCL_DEFAULT 0x0
	/*[field] CKEY_DEI*/
	#define XLT_RULE_TBL_CKEY_DEI
	#define XLT_RULE_TBL_CKEY_DEI_OFFSET  53
	#define XLT_RULE_TBL_CKEY_DEI_LEN     1
	#define XLT_RULE_TBL_CKEY_DEI_DEFAULT 0x0
	/*[field] VNI_RESV_INCL*/
	#define XLT_RULE_TBL_VNI_RESV_INCL
	#define XLT_RULE_TBL_VNI_RESV_INCL_OFFSET  54
	#define XLT_RULE_TBL_VNI_RESV_INCL_LEN     1
	#define XLT_RULE_TBL_VNI_RESV_INCL_DEFAULT 0x0
	/*[field] VNI_RESV_TYPE*/
	#define XLT_RULE_TBL_VNI_RESV_TYPE
	#define XLT_RULE_TBL_VNI_RESV_TYPE_OFFSET  55
	#define XLT_RULE_TBL_VNI_RESV_TYPE_LEN     1
	#define XLT_RULE_TBL_VNI_RESV_TYPE_DEFAULT 0x0
	/*[field] VNI_RESV*/
	#define XLT_RULE_TBL_VNI_RESV
	#define XLT_RULE_TBL_VNI_RESV_OFFSET  56
	#define XLT_RULE_TBL_VNI_RESV_LEN     32
	#define XLT_RULE_TBL_VNI_RESV_DEFAULT 0x0
	/*[field] FRM_TYPE_INCL*/
	#define XLT_RULE_TBL_FRM_TYPE_INCL
	#define XLT_RULE_TBL_FRM_TYPE_INCL_OFFSET  88
	#define XLT_RULE_TBL_FRM_TYPE_INCL_LEN     1
	#define XLT_RULE_TBL_FRM_TYPE_INCL_DEFAULT 0x0
	/*[field] FRM_TYPE*/
	#define XLT_RULE_TBL_FRM_TYPE
	#define XLT_RULE_TBL_FRM_TYPE_OFFSET  89
	#define XLT_RULE_TBL_FRM_TYPE_LEN     2
	#define XLT_RULE_TBL_FRM_TYPE_DEFAULT 0x0
	/*[field] PROT_INCL*/
	#define XLT_RULE_TBL_PROT_INCL
	#define XLT_RULE_TBL_PROT_INCL_OFFSET  91
	#define XLT_RULE_TBL_PROT_INCL_LEN     1
	#define XLT_RULE_TBL_PROT_INCL_DEFAULT 0x0
	/*[field] PROT_VALUE*/
	#define XLT_RULE_TBL_PROT_VALUE
	#define XLT_RULE_TBL_PROT_VALUE_OFFSET  92
	#define XLT_RULE_TBL_PROT_VALUE_LEN     16
	#define XLT_RULE_TBL_PROT_VALUE_DEFAULT 0x0
	/*[field] PORT_TYPE*/
	#define XLT_RULE_TBL_PORT_TYPE
	#define XLT_RULE_TBL_PORT_TYPE_OFFSET  108
	#define XLT_RULE_TBL_PORT_TYPE_LEN     2
	#define XLT_RULE_TBL_PORT_TYPE_DEFAULT 0x0
	/*[field] STPID_INCL*/
	#define XLT_RULE_TBL_STPID_INCL
	#define XLT_RULE_TBL_STPID_INCL_OFFSET  110
	#define XLT_RULE_TBL_STPID_INCL_LEN     1
	#define XLT_RULE_TBL_STPID_INCL_DEFAULT 0x0
	/*[field] STPID_INDEX*/
	#define XLT_RULE_TBL_STPID_INDEX
	#define XLT_RULE_TBL_STPID_INDEX_OFFSET  111
	#define XLT_RULE_TBL_STPID_INDEX_LEN     2
	#define XLT_RULE_TBL_STPID_INDEX_DEFAULT 0x0
	/*[field] CTPID_INCL*/
	#define XLT_RULE_TBL_CTPID_INCL
	#define XLT_RULE_TBL_CTPID_INCL_OFFSET  113
	#define XLT_RULE_TBL_CTPID_INCL_LEN     1
	#define XLT_RULE_TBL_CTPID_INCL_DEFAULT 0x0
	/*[field] CTPID_INDEX*/
	#define XLT_RULE_TBL_CTPID_INDEX
	#define XLT_RULE_TBL_CTPID_INDEX_OFFSET  114
	#define XLT_RULE_TBL_CTPID_INDEX_LEN     2
	#define XLT_RULE_TBL_CTPID_INDEX_DEFAULT 0x0
	/*[field] DHCP_TYPE*/
	#define XLT_RULE_TBL_DHCP_TYPE
	#define XLT_RULE_TBL_DHCP_TYPE_OFFSET  116
	#define XLT_RULE_TBL_DHCP_TYPE_LEN     3
	#define XLT_RULE_TBL_DHCP_TYPE_DEFAULT 0x0
	/*[field] MC_TYPE*/
	#define XLT_RULE_TBL_MC_TYPE
	#define XLT_RULE_TBL_MC_TYPE_OFFSET  119
	#define XLT_RULE_TBL_MC_TYPE_LEN     3
	#define XLT_RULE_TBL_MC_TYPE_DEFAULT 0x0

struct xlt_rule_tbl_0 {
	a_uint32_t  valid:1;
	a_uint32_t  port_bitmap:9;
	a_uint32_t  skey_fmt:3;
	a_uint32_t  skey_vid_incl:1;
	a_uint32_t  skey_vid:12;
	a_uint32_t  skey_pcp_incl:1;
	a_uint32_t  skey_pcp:3;
	a_uint32_t  skey_dei_incl:1;
	a_uint32_t  skey_dei:1;
	a_uint32_t  ckey_fmt:3;
	a_uint32_t  ckey_vid_incl:1;
	a_uint32_t  ckey_vid:12;
	a_uint32_t  ckey_pcp_incl:1;
	a_uint32_t  ckey_pcp:3;
	a_uint32_t  ckey_dei_incl:1;
	a_uint32_t  ckey_dei:1;
	a_uint32_t  vni_resv_incl:1;
	a_uint32_t  vni_resv_type:1;
	a_uint32_t  vni_resv_0:8;
	a_uint32_t  vni_resv_1:24;
	a_uint32_t  frm_type_incl:1;
	a_uint32_t  frm_type:2;
	a_uint32_t  prot_incl:1;
	a_uint32_t  prot_value_0:4;
	a_uint32_t  prot_value_1:12;
	a_uint32_t  port_type:2;
	a_uint32_t  stpid_incl:1;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_incl:1;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dhcp_type:3;
	a_uint32_t  mc_type:3;
	a_uint32_t  _reserved0:6;
};

struct xlt_rule_tbl_1 {
	a_uint32_t  valid:1;
	a_uint32_t  port_vp_id:9;
	a_uint32_t  skey_fmt:3;
	a_uint32_t  skey_vid_incl:1;
	a_uint32_t  skey_vid:12;
	a_uint32_t  skey_pcp_incl:1;
	a_uint32_t  skey_pcp:3;
	a_uint32_t  skey_dei_incl:1;
	a_uint32_t  skey_dei:1;
	a_uint32_t  ckey_fmt:3;
	a_uint32_t  ckey_vid_incl:1;
	a_uint32_t  ckey_vid:12;
	a_uint32_t  ckey_pcp_incl:1;
	a_uint32_t  ckey_pcp:3;
	a_uint32_t  ckey_dei_incl:1;
	a_uint32_t  ckey_dei:1;
	a_uint32_t  vni_resv_incl:1;
	a_uint32_t  vni_resv_type:1;
	a_uint32_t  vni_resv_0:8;
	a_uint32_t  vni_resv_1:24;
	a_uint32_t  frm_type_incl:1;
	a_uint32_t  frm_type:2;
	a_uint32_t  prot_incl:1;
	a_uint32_t  prot_value_0:4;
	a_uint32_t  prot_value_1:12;
	a_uint32_t  port_type:2;
	a_uint32_t  stpid_incl:1;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_incl:1;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dhcp_type:3;
	a_uint32_t  mc_type:3;
	a_uint32_t  _reserved0:6;
};

struct xlt_rule_tbl_2 {
	a_uint32_t  valid:1;
	a_uint32_t  vp_profile:9;
	a_uint32_t  skey_fmt:3;
	a_uint32_t  skey_vid_incl:1;
	a_uint32_t  skey_vid:12;
	a_uint32_t  skey_pcp_incl:1;
	a_uint32_t  skey_pcp:3;
	a_uint32_t  skey_dei_incl:1;
	a_uint32_t  skey_dei:1;
	a_uint32_t  ckey_fmt:3;
	a_uint32_t  ckey_vid_incl:1;
	a_uint32_t  ckey_vid:12;
	a_uint32_t  ckey_pcp_incl:1;
	a_uint32_t  ckey_pcp:3;
	a_uint32_t  ckey_dei_incl:1;
	a_uint32_t  ckey_dei:1;
	a_uint32_t  vni_resv_incl:1;
	a_uint32_t  vni_resv_type:1;
	a_uint32_t  vni_resv_0:8;
	a_uint32_t  vni_resv_1:24;
	a_uint32_t  frm_type_incl:1;
	a_uint32_t  frm_type:2;
	a_uint32_t  prot_incl:1;
	a_uint32_t  prot_value_0:4;
	a_uint32_t  prot_value_1:12;
	a_uint32_t  port_type:2;
	a_uint32_t  stpid_incl:1;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_incl:1;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dhcp_type:3;
	a_uint32_t  mc_type:3;
	a_uint32_t  _reserved0:6;
};

struct xlt_rule_tbl_3 {
	a_uint32_t  valid:1;
	a_uint32_t  gem_port_id:9;
	a_uint32_t  skey_fmt:3;
	a_uint32_t  skey_vid_incl:1;
	a_uint32_t  skey_vid:12;
	a_uint32_t  skey_pcp_incl:1;
	a_uint32_t  skey_pcp:3;
	a_uint32_t  skey_dei_incl:1;
	a_uint32_t  skey_dei:1;
	a_uint32_t  ckey_fmt:3;
	a_uint32_t  ckey_vid_incl:1;
	a_uint32_t  ckey_vid:12;
	a_uint32_t  ckey_pcp_incl:1;
	a_uint32_t  ckey_pcp:3;
	a_uint32_t  ckey_dei_incl:1;
	a_uint32_t  ckey_dei:1;
	a_uint32_t  vni_resv_incl:1;
	a_uint32_t  vni_resv_type:1;
	a_uint32_t  vni_resv_0:8;
	a_uint32_t  vni_resv_1:24;
	a_uint32_t  frm_type_incl:1;
	a_uint32_t  frm_type:2;
	a_uint32_t  prot_incl:1;
	a_uint32_t  prot_value_0:4;
	a_uint32_t  prot_value_1:12;
	a_uint32_t  port_type:2;
	a_uint32_t  stpid_incl:1;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_incl:1;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dhcp_type:3;
	a_uint32_t  mc_type:3;
	a_uint32_t  _reserved0:6;
};

union xlt_rule_tbl_u {
	a_uint32_t val[4];
	struct xlt_rule_tbl_0 bf;
	struct xlt_rule_tbl_1 bf1;
	struct xlt_rule_tbl_2 bf2;
	struct xlt_rule_tbl_3 bf3;
};

/*[table] XLT_ACTION_TBL*/
#define XLT_ACTION_TBL
#define XLT_ACTION_TBL_ADDRESS 0x4000
#define XLT_ACTION_TBL_NUM     256
#define XLT_ACTION_TBL_INC     0x10
#define XLT_ACTION_TBL_TYPE    REG_TYPE_RW
#define XLT_ACTION_TBL_DEFAULT 0x0
	/*[field] VID_SWAP_CMD*/
	#define XLT_ACTION_TBL_VID_SWAP_CMD
	#define XLT_ACTION_TBL_VID_SWAP_CMD_OFFSET  0
	#define XLT_ACTION_TBL_VID_SWAP_CMD_LEN     1
	#define XLT_ACTION_TBL_VID_SWAP_CMD_DEFAULT 0x0
	/*[field] XLT_SVID_CMD*/
	#define XLT_ACTION_TBL_XLT_SVID_CMD
	#define XLT_ACTION_TBL_XLT_SVID_CMD_OFFSET  1
	#define XLT_ACTION_TBL_XLT_SVID_CMD_LEN     3
	#define XLT_ACTION_TBL_XLT_SVID_CMD_DEFAULT 0x0
	/*[field] XLT_SVID*/
	#define XLT_ACTION_TBL_XLT_SVID
	#define XLT_ACTION_TBL_XLT_SVID_OFFSET  4
	#define XLT_ACTION_TBL_XLT_SVID_LEN     12
	#define XLT_ACTION_TBL_XLT_SVID_DEFAULT 0x0
	/*[field] XLT_CVID_CMD*/
	#define XLT_ACTION_TBL_XLT_CVID_CMD
	#define XLT_ACTION_TBL_XLT_CVID_CMD_OFFSET  16
	#define XLT_ACTION_TBL_XLT_CVID_CMD_LEN     3
	#define XLT_ACTION_TBL_XLT_CVID_CMD_DEFAULT 0x0
	/*[field] XLT_CVID*/
	#define XLT_ACTION_TBL_XLT_CVID
	#define XLT_ACTION_TBL_XLT_CVID_OFFSET  19
	#define XLT_ACTION_TBL_XLT_CVID_LEN     12
	#define XLT_ACTION_TBL_XLT_CVID_DEFAULT 0x0
	/*[field] PCP_SWAP_CMD*/
	#define XLT_ACTION_TBL_PCP_SWAP_CMD
	#define XLT_ACTION_TBL_PCP_SWAP_CMD_OFFSET  31
	#define XLT_ACTION_TBL_PCP_SWAP_CMD_LEN     1
	#define XLT_ACTION_TBL_PCP_SWAP_CMD_DEFAULT 0x0
	/*[field] XLT_SPCP_CMD*/
	#define XLT_ACTION_TBL_XLT_SPCP_CMD
	#define XLT_ACTION_TBL_XLT_SPCP_CMD_OFFSET  32
	#define XLT_ACTION_TBL_XLT_SPCP_CMD_LEN     4
	#define XLT_ACTION_TBL_XLT_SPCP_CMD_DEFAULT 0x0
	/*[field] XLT_SPCP*/
	#define XLT_ACTION_TBL_XLT_SPCP
	#define XLT_ACTION_TBL_XLT_SPCP_OFFSET  36
	#define XLT_ACTION_TBL_XLT_SPCP_LEN     3
	#define XLT_ACTION_TBL_XLT_SPCP_DEFAULT 0x0
	/*[field] XLT_CPCP_CMD*/
	#define XLT_ACTION_TBL_XLT_CPCP_CMD
	#define XLT_ACTION_TBL_XLT_CPCP_CMD_OFFSET  39
	#define XLT_ACTION_TBL_XLT_CPCP_CMD_LEN     4
	#define XLT_ACTION_TBL_XLT_CPCP_CMD_DEFAULT 0x0
	/*[field] XLT_CPCP*/
	#define XLT_ACTION_TBL_XLT_CPCP
	#define XLT_ACTION_TBL_XLT_CPCP_OFFSET  43
	#define XLT_ACTION_TBL_XLT_CPCP_LEN     3
	#define XLT_ACTION_TBL_XLT_CPCP_DEFAULT 0x0
	/*[field] DEI_SWAP_CMD*/
	#define XLT_ACTION_TBL_DEI_SWAP_CMD
	#define XLT_ACTION_TBL_DEI_SWAP_CMD_OFFSET  46
	#define XLT_ACTION_TBL_DEI_SWAP_CMD_LEN     1
	#define XLT_ACTION_TBL_DEI_SWAP_CMD_DEFAULT 0x0
	/*[field] XLT_SDEI_CMD*/
	#define XLT_ACTION_TBL_XLT_SDEI_CMD
	#define XLT_ACTION_TBL_XLT_SDEI_CMD_OFFSET  47
	#define XLT_ACTION_TBL_XLT_SDEI_CMD_LEN     2
	#define XLT_ACTION_TBL_XLT_SDEI_CMD_DEFAULT 0x0
	/*[field] XLT_SDEI*/
	#define XLT_ACTION_TBL_XLT_SDEI
	#define XLT_ACTION_TBL_XLT_SDEI_OFFSET  49
	#define XLT_ACTION_TBL_XLT_SDEI_LEN     1
	#define XLT_ACTION_TBL_XLT_SDEI_DEFAULT 0x0
	/*[field] XLT_CDEI_CMD*/
	#define XLT_ACTION_TBL_XLT_CDEI_CMD
	#define XLT_ACTION_TBL_XLT_CDEI_CMD_OFFSET  50
	#define XLT_ACTION_TBL_XLT_CDEI_CMD_LEN     2
	#define XLT_ACTION_TBL_XLT_CDEI_CMD_DEFAULT 0x0
	/*[field] XLT_CDEI*/
	#define XLT_ACTION_TBL_XLT_CDEI
	#define XLT_ACTION_TBL_XLT_CDEI_OFFSET  52
	#define XLT_ACTION_TBL_XLT_CDEI_LEN     1
	#define XLT_ACTION_TBL_XLT_CDEI_DEFAULT 0x0
	/*[field] TAGS_TO_REMOVE*/
	#define XLT_ACTION_TBL_TAGS_TO_REMOVE
	#define XLT_ACTION_TBL_TAGS_TO_REMOVE_OFFSET  53
	#define XLT_ACTION_TBL_TAGS_TO_REMOVE_LEN     2
	#define XLT_ACTION_TBL_TAGS_TO_REMOVE_DEFAULT 0x0
	/*[field] STPID_CMD*/
	#define XLT_ACTION_TBL_STPID_CMD
	#define XLT_ACTION_TBL_STPID_CMD_OFFSET  55
	#define XLT_ACTION_TBL_STPID_CMD_LEN     2
	#define XLT_ACTION_TBL_STPID_CMD_DEFAULT 0x0
	/*[field] STPID_INDEX*/
	#define XLT_ACTION_TBL_STPID_INDEX
	#define XLT_ACTION_TBL_STPID_INDEX_OFFSET  57
	#define XLT_ACTION_TBL_STPID_INDEX_LEN     2
	#define XLT_ACTION_TBL_STPID_INDEX_DEFAULT 0x0
	/*[field] CTPID_CMD*/
	#define XLT_ACTION_TBL_CTPID_CMD
	#define XLT_ACTION_TBL_CTPID_CMD_OFFSET  59
	#define XLT_ACTION_TBL_CTPID_CMD_LEN     2
	#define XLT_ACTION_TBL_CTPID_CMD_DEFAULT 0x0
	/*[field] CTPID_INDEX*/
	#define XLT_ACTION_TBL_CTPID_INDEX
	#define XLT_ACTION_TBL_CTPID_INDEX_OFFSET  61
	#define XLT_ACTION_TBL_CTPID_INDEX_LEN     2
	#define XLT_ACTION_TBL_CTPID_INDEX_DEFAULT 0x0
	/*[field] DSCP_PBIT_MAPPING_INDEX*/
	#define XLT_ACTION_TBL_DSCP_PBIT_MAPPING_INDEX
	#define XLT_ACTION_TBL_DSCP_PBIT_MAPPING_INDEX_OFFSET  63
	#define XLT_ACTION_TBL_DSCP_PBIT_MAPPING_INDEX_LEN     1
	#define XLT_ACTION_TBL_DSCP_PBIT_MAPPING_INDEX_DEFAULT 0x0
	/*[field] COUNTER_EN*/
	#define XLT_ACTION_TBL_COUNTER_EN
	#define XLT_ACTION_TBL_COUNTER_EN_OFFSET  64
	#define XLT_ACTION_TBL_COUNTER_EN_LEN     1
	#define XLT_ACTION_TBL_COUNTER_EN_DEFAULT 0x0
	/*[field] COUNTER_ID*/
	#define XLT_ACTION_TBL_COUNTER_ID
	#define XLT_ACTION_TBL_COUNTER_ID_OFFSET  65
	#define XLT_ACTION_TBL_COUNTER_ID_LEN     7
	#define XLT_ACTION_TBL_COUNTER_ID_DEFAULT 0x0
	/*[field] COUNTER_MODE*/
	#define XLT_ACTION_TBL_COUNTER_MODE
	#define XLT_ACTION_TBL_COUNTER_MODE_OFFSET  72
	#define XLT_ACTION_TBL_COUNTER_MODE_LEN     1
	#define XLT_ACTION_TBL_COUNTER_MODE_DEFAULT 0x0
	/*[field] VSI_CMD*/
	#define XLT_ACTION_TBL_VSI_CMD
	#define XLT_ACTION_TBL_VSI_CMD_OFFSET  73
	#define XLT_ACTION_TBL_VSI_CMD_LEN     1
	#define XLT_ACTION_TBL_VSI_CMD_DEFAULT 0x0
	/*[field] VSI*/
	#define XLT_ACTION_TBL_VSI
	#define XLT_ACTION_TBL_VSI_OFFSET  74
	#define XLT_ACTION_TBL_VSI_LEN     6
	#define XLT_ACTION_TBL_VSI_DEFAULT 0x0
	/*[field] SRC_INFO_VALID*/
	#define XLT_ACTION_TBL_SRC_INFO_VALID
	#define XLT_ACTION_TBL_SRC_INFO_VALID_OFFSET  80
	#define XLT_ACTION_TBL_SRC_INFO_VALID_LEN     1
	#define XLT_ACTION_TBL_SRC_INFO_VALID_DEFAULT 0x0
	/*[field] SRC_INFO_TYPE*/
	#define XLT_ACTION_TBL_SRC_INFO_TYPE
	#define XLT_ACTION_TBL_SRC_INFO_TYPE_OFFSET  81
	#define XLT_ACTION_TBL_SRC_INFO_TYPE_LEN     1
	#define XLT_ACTION_TBL_SRC_INFO_TYPE_DEFAULT 0x0
	/*[field] SRC_INFO*/
	#define XLT_ACTION_TBL_SRC_INFO
	#define XLT_ACTION_TBL_SRC_INFO_OFFSET  82
	#define XLT_ACTION_TBL_SRC_INFO_LEN     8
	#define XLT_ACTION_TBL_SRC_INFO_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define XLT_ACTION_TBL_FWD_CMD
	#define XLT_ACTION_TBL_FWD_CMD_OFFSET  90
	#define XLT_ACTION_TBL_FWD_CMD_LEN     2
	#define XLT_ACTION_TBL_FWD_CMD_DEFAULT 0x0
	/*[field] DEST_INFO_VALID*/
	#define XLT_ACTION_TBL_DEST_INFO_VALID
	#define XLT_ACTION_TBL_DEST_INFO_VALID_OFFSET  92
	#define XLT_ACTION_TBL_DEST_INFO_VALID_LEN     1
	#define XLT_ACTION_TBL_DEST_INFO_VALID_DEFAULT 0x0
	/*[field] DEST_INFO*/
	#define XLT_ACTION_TBL_DEST_INFO
	#define XLT_ACTION_TBL_DEST_INFO_OFFSET  93
	#define XLT_ACTION_TBL_DEST_INFO_LEN     8
	#define XLT_ACTION_TBL_DEST_INFO_DEFAULT 0x0
	/*[field] SERVICE_CODE_EN*/
	#define XLT_ACTION_TBL_SERVICE_CODE_EN
	#define XLT_ACTION_TBL_SERVICE_CODE_EN_OFFSET  101
	#define XLT_ACTION_TBL_SERVICE_CODE_EN_LEN     1
	#define XLT_ACTION_TBL_SERVICE_CODE_EN_DEFAULT 0x0
	/*[field] SERVICE_CODE*/
	#define XLT_ACTION_TBL_SERVICE_CODE
	#define XLT_ACTION_TBL_SERVICE_CODE_OFFSET  102
	#define XLT_ACTION_TBL_SERVICE_CODE_LEN     8
	#define XLT_ACTION_TBL_SERVICE_CODE_DEFAULT 0x0

struct xlt_action_tbl {
	a_uint32_t  vid_swap_cmd:1;
	a_uint32_t  xlt_svid_cmd:3;
	a_uint32_t  xlt_svid:12;
	a_uint32_t  xlt_cvid_cmd:3;
	a_uint32_t  xlt_cvid:12;
	a_uint32_t  pcp_swap_cmd:1;
	a_uint32_t  xlt_spcp_cmd:4;
	a_uint32_t  xlt_spcp:3;
	a_uint32_t  xlt_cpcp_cmd:4;
	a_uint32_t  xlt_cpcp:3;
	a_uint32_t  dei_swap_cmd:1;
	a_uint32_t  xlt_sdei_cmd:2;
	a_uint32_t  xlt_sdei:1;
	a_uint32_t  xlt_cdei_cmd:2;
	a_uint32_t  xlt_cdei:1;
	a_uint32_t  tags_to_remove:2;
	a_uint32_t  stpid_cmd:2;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_cmd:2;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dscp_pbit_mapping_index:1;
	a_uint32_t  counter_en:1;
	a_uint32_t  counter_id:7;
	a_uint32_t  counter_mode:1;
	a_uint32_t  vsi_cmd:1;
	a_uint32_t  vsi:6;
	a_uint32_t  src_info_valid:1;
	a_uint32_t  src_info_type:1;
	a_uint32_t  src_info:8;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  dest_info_valid:1;
	a_uint32_t  dest_info_0:3;
	a_uint32_t  dest_info_1:5;
	a_uint32_t  service_code_en:1;
	a_uint32_t  service_code:8;
	a_uint32_t  _reserved0:18;
};

union xlt_action_tbl_u {
	a_uint32_t val[4];
	struct xlt_action_tbl bf;
};

/*[register] PORT_EG_DEF_VID*/
#define PORT_EG_DEF_VID
#define PORT_EG_DEF_VID_ADDRESS 0x0
#define PORT_EG_DEF_VID_NUM     9
#define PORT_EG_DEF_VID_INC     0x4
#define PORT_EG_DEF_VID_TYPE    REG_TYPE_RW
#define PORT_EG_DEF_VID_DEFAULT 0x0
	/*[field] PORT_DEF_SVID*/
	#define PORT_EG_DEF_VID_PORT_DEF_SVID
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_OFFSET  0
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_LEN     12
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_DEFAULT 0x0
	/*[field] PORT_DEF_SVID_EN*/
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_EN
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_EN_OFFSET  12
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_EN_LEN     1
	#define PORT_EG_DEF_VID_PORT_DEF_SVID_EN_DEFAULT 0x0
	/*[field] PORT_DEF_CVID*/
	#define PORT_EG_DEF_VID_PORT_DEF_CVID
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_OFFSET  16
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_LEN     12
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_DEFAULT 0x0
	/*[field] PORT_DEF_CVID_EN*/
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_EN
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_EN_OFFSET  28
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_EN_LEN     1
	#define PORT_EG_DEF_VID_PORT_DEF_CVID_EN_DEFAULT 0x0

struct port_eg_def_vid {
	a_uint32_t  port_def_svid:12;
	a_uint32_t  port_def_svid_en:1;
	a_uint32_t  _reserved0:3;
	a_uint32_t  port_def_cvid:12;
	a_uint32_t  port_def_cvid_en:1;
	a_uint32_t  _reserved1:3;
};

union port_eg_def_vid_u {
	a_uint32_t val;
	struct port_eg_def_vid bf;
};

/*[register] PORT_EG_VLAN*/
#define PORT_EG_VLAN
#define PORT_EG_VLAN_ADDRESS 0x40
#define PORT_EG_VLAN_NUM     9
#define PORT_EG_VLAN_INC     0x4
#define PORT_EG_VLAN_TYPE    REG_TYPE_RW
#define PORT_EG_VLAN_DEFAULT 0x34
	/*[field] PORT_VLAN_TYPE*/
	#define PORT_EG_VLAN_PORT_VLAN_TYPE
	#define PORT_EG_VLAN_PORT_VLAN_TYPE_OFFSET  0
	#define PORT_EG_VLAN_PORT_VLAN_TYPE_LEN     1
	#define PORT_EG_VLAN_PORT_VLAN_TYPE_DEFAULT 0x0
	/*[field] PORT_EG_VLAN_CTAG_MODE*/
	#define PORT_EG_VLAN_PORT_EG_VLAN_CTAG_MODE
	#define PORT_EG_VLAN_PORT_EG_VLAN_CTAG_MODE_OFFSET  1
	#define PORT_EG_VLAN_PORT_EG_VLAN_CTAG_MODE_LEN     2
	#define PORT_EG_VLAN_PORT_EG_VLAN_CTAG_MODE_DEFAULT 0x2
	/*[field] PORT_EG_VLAN_STAG_MODE*/
	#define PORT_EG_VLAN_PORT_EG_VLAN_STAG_MODE
	#define PORT_EG_VLAN_PORT_EG_VLAN_STAG_MODE_OFFSET  3
	#define PORT_EG_VLAN_PORT_EG_VLAN_STAG_MODE_LEN     2
	#define PORT_EG_VLAN_PORT_EG_VLAN_STAG_MODE_DEFAULT 0x2
	/*[field] VSI_TAG_MODE_EN*/
	#define PORT_EG_VLAN_VSI_TAG_MODE_EN
	#define PORT_EG_VLAN_VSI_TAG_MODE_EN_OFFSET  5
	#define PORT_EG_VLAN_VSI_TAG_MODE_EN_LEN     1
	#define PORT_EG_VLAN_VSI_TAG_MODE_EN_DEFAULT 0x1
	/*[field] PORT_EG_PCP_PROP_CMD*/
	#define PORT_EG_VLAN_PORT_EG_PCP_PROP_CMD
	#define PORT_EG_VLAN_PORT_EG_PCP_PROP_CMD_OFFSET  6
	#define PORT_EG_VLAN_PORT_EG_PCP_PROP_CMD_LEN     1
	#define PORT_EG_VLAN_PORT_EG_PCP_PROP_CMD_DEFAULT 0x0
	/*[field] PORT_EG_DEI_PROP_CMD*/
	#define PORT_EG_VLAN_PORT_EG_DEI_PROP_CMD
	#define PORT_EG_VLAN_PORT_EG_DEI_PROP_CMD_OFFSET  7
	#define PORT_EG_VLAN_PORT_EG_DEI_PROP_CMD_LEN     1
	#define PORT_EG_VLAN_PORT_EG_DEI_PROP_CMD_DEFAULT 0x0
	/*[field] TX_COUNTING_EN*/
	#define PORT_EG_VLAN_TX_COUNTING_EN
	#define PORT_EG_VLAN_TX_COUNTING_EN_OFFSET  8
	#define PORT_EG_VLAN_TX_COUNTING_EN_LEN     1
	#define PORT_EG_VLAN_TX_COUNTING_EN_DEFAULT 0x0

struct port_eg_vlan {
	a_uint32_t  port_vlan_type:1;
	a_uint32_t  port_eg_vlan_ctag_mode:2;
	a_uint32_t  port_eg_vlan_stag_mode:2;
	a_uint32_t  vsi_tag_mode_en:1;
	a_uint32_t  port_eg_pcp_prop_cmd:1;
	a_uint32_t  port_eg_dei_prop_cmd:1;
	a_uint32_t  tx_counting_en:1;
	a_uint32_t  _reserved0:23;
};

union port_eg_vlan_u {
	a_uint32_t val;
	struct port_eg_vlan bf;
};

/*[register] EG_VLAN_TPID*/
#define EG_VLAN_TPID
#define EG_VLAN_TPID_ADDRESS 0x80
#define EG_VLAN_TPID_NUM     1
#define EG_VLAN_TPID_INC     0x4
#define EG_VLAN_TPID_TYPE    REG_TYPE_RW
#define EG_VLAN_TPID_DEFAULT 0x810088a8
	/*[field] STPID*/
	#define EG_VLAN_TPID_STPID
	#define EG_VLAN_TPID_STPID_OFFSET  0
	#define EG_VLAN_TPID_STPID_LEN     16
	#define EG_VLAN_TPID_STPID_DEFAULT 0x88a8
	/*[field] CTPID*/
	#define EG_VLAN_TPID_CTPID
	#define EG_VLAN_TPID_CTPID_OFFSET  16
	#define EG_VLAN_TPID_CTPID_LEN     16
	#define EG_VLAN_TPID_CTPID_DEFAULT 0x8100

struct eg_vlan_tpid {
	a_uint32_t  stpid:16;
	a_uint32_t  ctpid:16;
};

union eg_vlan_tpid_u {
	a_uint32_t val;
	struct eg_vlan_tpid bf;
};

/*[register] EG_BRIDGE_CONFIG*/
#define EG_BRIDGE_CONFIG
#define EG_BRIDGE_CONFIG_ADDRESS 0x84
#define EG_BRIDGE_CONFIG_NUM     1
#define EG_BRIDGE_CONFIG_INC     0x4
#define EG_BRIDGE_CONFIG_TYPE    REG_TYPE_RW
#define EG_BRIDGE_CONFIG_DEFAULT 0x7cfd400
	/*[field] BRIDGE_TYPE*/
	#define EG_BRIDGE_CONFIG_BRIDGE_TYPE
	#define EG_BRIDGE_CONFIG_BRIDGE_TYPE_OFFSET  0
	#define EG_BRIDGE_CONFIG_BRIDGE_TYPE_LEN     1
	#define EG_BRIDGE_CONFIG_BRIDGE_TYPE_DEFAULT 0x0
	/*[field] PKT_L2_EDIT_EN*/
	#define EG_BRIDGE_CONFIG_PKT_L2_EDIT_EN
	#define EG_BRIDGE_CONFIG_PKT_L2_EDIT_EN_OFFSET  1
	#define EG_BRIDGE_CONFIG_PKT_L2_EDIT_EN_LEN     1
	#define EG_BRIDGE_CONFIG_PKT_L2_EDIT_EN_DEFAULT 0x0
	/*[field] QUEUE_CNT_EN*/
	#define EG_BRIDGE_CONFIG_QUEUE_CNT_EN
	#define EG_BRIDGE_CONFIG_QUEUE_CNT_EN_OFFSET  2
	#define EG_BRIDGE_CONFIG_QUEUE_CNT_EN_LEN     1
	#define EG_BRIDGE_CONFIG_QUEUE_CNT_EN_DEFAULT 0x0
	/*[field] PPE_EIP_RSV_W4_3130*/
	#define EG_BRIDGE_CONFIG_PPE_EIP_RSV_W4_3130
	#define EG_BRIDGE_CONFIG_PPE_EIP_RSV_W4_3130_OFFSET  8
	#define EG_BRIDGE_CONFIG_PPE_EIP_RSV_W4_3130_LEN     2
	#define EG_BRIDGE_CONFIG_PPE_EIP_RSV_W4_3130_DEFAULT 0x0
	/*[field] FIELD_UPDATE_ENABLE*/
	#define EG_BRIDGE_CONFIG_FIELD_UPDATE_ENABLE
	#define EG_BRIDGE_CONFIG_FIELD_UPDATE_ENABLE_OFFSET  10
	#define EG_BRIDGE_CONFIG_FIELD_UPDATE_ENABLE_LEN     1
	#define EG_BRIDGE_CONFIG_FIELD_UPDATE_ENABLE_DEFAULT 0x1
	/*[field] PASSTHROUGH_CPU_CODE0*/
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE0
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE0_OFFSET  11
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE0_LEN     8
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE0_DEFAULT 0xfa
	/*[field] PASSTHROUGH_CPU_CODE1*/
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE1
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE1_OFFSET  19
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE1_LEN     8
	#define EG_BRIDGE_CONFIG_PASSTHROUGH_CPU_CODE1_DEFAULT 0xf9

struct eg_bridge_config {
	a_uint32_t  bridge_type:1;
	a_uint32_t  pkt_l2_edit_en:1;
	a_uint32_t  queue_cnt_en:1;
	a_uint32_t  _reserved0:5;
	a_uint32_t  ppe_eip_rsv_w4_3130:2;
	a_uint32_t  field_update_enable:1;
	a_uint32_t  passthrough_cpu_code0:8;
	a_uint32_t  passthrough_cpu_code1:8;
	a_uint32_t  _reserved1:5;
};

union eg_bridge_config_u {
	a_uint32_t val;
	struct eg_bridge_config bf;
};

/*[table] EG_VSI_TAG*/
#define EG_VSI_TAG
#define EG_VSI_TAG_ADDRESS 0x800
#define EG_VSI_TAG_NUM     64
#define EG_VSI_TAG_INC     0x10
#define EG_VSI_TAG_TYPE    REG_TYPE_RW
#define EG_VSI_TAG_DEFAULT 0x0
	/*[field] TAGGED_MODE_PORT_BITMAP*/
	#define EG_VSI_TAG_TAGGED_MODE_PORT_BITMAP
	#define EG_VSI_TAG_TAGGED_MODE_PORT_BITMAP_OFFSET  0
	#define EG_VSI_TAG_TAGGED_MODE_PORT_BITMAP_LEN     18
	#define EG_VSI_TAG_TAGGED_MODE_PORT_BITMAP_DEFAULT 0x0

struct eg_vsi_tag {
	a_uint32_t  tagged_mode_port_bitmap:18;
	a_uint32_t  _reserved0:14;
};

union eg_vsi_tag_u {
	a_uint32_t val;
	struct eg_vsi_tag bf;
};

/*[table] EG_VLAN_XLT_ACTION*/
#define EG_VLAN_XLT_ACTION
#define EG_VLAN_XLT_ACTION_ADDRESS 0x1000
#define EG_VLAN_XLT_ACTION_NUM     128
#define EG_VLAN_XLT_ACTION_INC     0x10
#define EG_VLAN_XLT_ACTION_TYPE    REG_TYPE_RW
#define EG_VLAN_XLT_ACTION_DEFAULT 0x0
	/*[field] VID_SWAP_CMD*/
	#define EG_VLAN_XLT_ACTION_VID_SWAP_CMD
	#define EG_VLAN_XLT_ACTION_VID_SWAP_CMD_OFFSET  0
	#define EG_VLAN_XLT_ACTION_VID_SWAP_CMD_LEN     1
	#define EG_VLAN_XLT_ACTION_VID_SWAP_CMD_DEFAULT 0x0
	/*[field] XLT_SVID_CMD*/
	#define EG_VLAN_XLT_ACTION_XLT_SVID_CMD
	#define EG_VLAN_XLT_ACTION_XLT_SVID_CMD_OFFSET  1
	#define EG_VLAN_XLT_ACTION_XLT_SVID_CMD_LEN     3
	#define EG_VLAN_XLT_ACTION_XLT_SVID_CMD_DEFAULT 0x0
	/*[field] XLT_SVID*/
	#define EG_VLAN_XLT_ACTION_XLT_SVID
	#define EG_VLAN_XLT_ACTION_XLT_SVID_OFFSET  4
	#define EG_VLAN_XLT_ACTION_XLT_SVID_LEN     12
	#define EG_VLAN_XLT_ACTION_XLT_SVID_DEFAULT 0x0
	/*[field] XLT_CVID_CMD*/
	#define EG_VLAN_XLT_ACTION_XLT_CVID_CMD
	#define EG_VLAN_XLT_ACTION_XLT_CVID_CMD_OFFSET  16
	#define EG_VLAN_XLT_ACTION_XLT_CVID_CMD_LEN     3
	#define EG_VLAN_XLT_ACTION_XLT_CVID_CMD_DEFAULT 0x0
	/*[field] XLT_CVID*/
	#define EG_VLAN_XLT_ACTION_XLT_CVID
	#define EG_VLAN_XLT_ACTION_XLT_CVID_OFFSET  19
	#define EG_VLAN_XLT_ACTION_XLT_CVID_LEN     12
	#define EG_VLAN_XLT_ACTION_XLT_CVID_DEFAULT 0x0
	/*[field] PCP_SWAP_CMD*/
	#define EG_VLAN_XLT_ACTION_PCP_SWAP_CMD
	#define EG_VLAN_XLT_ACTION_PCP_SWAP_CMD_OFFSET  31
	#define EG_VLAN_XLT_ACTION_PCP_SWAP_CMD_LEN     1
	#define EG_VLAN_XLT_ACTION_PCP_SWAP_CMD_DEFAULT 0x0
	/*[field] XLT_SPCP_CMD*/
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_CMD
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_CMD_OFFSET  32
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_CMD_LEN     4
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_CMD_DEFAULT 0x0
	/*[field] XLT_SPCP*/
	#define EG_VLAN_XLT_ACTION_XLT_SPCP
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_OFFSET  36
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_LEN     3
	#define EG_VLAN_XLT_ACTION_XLT_SPCP_DEFAULT 0x0
	/*[field] XLT_CPCP_CMD*/
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_CMD
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_CMD_OFFSET  39
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_CMD_LEN     4
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_CMD_DEFAULT 0x0
	/*[field] XLT_CPCP*/
	#define EG_VLAN_XLT_ACTION_XLT_CPCP
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_OFFSET  43
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_LEN     3
	#define EG_VLAN_XLT_ACTION_XLT_CPCP_DEFAULT 0x0
	/*[field] DEI_SWAP_CMD*/
	#define EG_VLAN_XLT_ACTION_DEI_SWAP_CMD
	#define EG_VLAN_XLT_ACTION_DEI_SWAP_CMD_OFFSET  46
	#define EG_VLAN_XLT_ACTION_DEI_SWAP_CMD_LEN     1
	#define EG_VLAN_XLT_ACTION_DEI_SWAP_CMD_DEFAULT 0x0
	/*[field] XLT_SDEI_CMD*/
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_CMD
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_CMD_OFFSET  47
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_CMD_LEN     2
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_CMD_DEFAULT 0x0
	/*[field] XLT_SDEI*/
	#define EG_VLAN_XLT_ACTION_XLT_SDEI
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_OFFSET  49
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_LEN     1
	#define EG_VLAN_XLT_ACTION_XLT_SDEI_DEFAULT 0x0
	/*[field] XLT_CDEI_CMD*/
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_CMD
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_CMD_OFFSET  50
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_CMD_LEN     2
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_CMD_DEFAULT 0x0
	/*[field] XLT_CDEI*/
	#define EG_VLAN_XLT_ACTION_XLT_CDEI
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_OFFSET  52
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_LEN     1
	#define EG_VLAN_XLT_ACTION_XLT_CDEI_DEFAULT 0x0
	/*[field] TAGS_TO_REMOVE*/
	#define EG_VLAN_XLT_ACTION_TAGS_TO_REMOVE
	#define EG_VLAN_XLT_ACTION_TAGS_TO_REMOVE_OFFSET  53
	#define EG_VLAN_XLT_ACTION_TAGS_TO_REMOVE_LEN     2
	#define EG_VLAN_XLT_ACTION_TAGS_TO_REMOVE_DEFAULT 0x0
	/*[field] STPID_CMD*/
	#define EG_VLAN_XLT_ACTION_STPID_CMD
	#define EG_VLAN_XLT_ACTION_STPID_CMD_OFFSET  55
	#define EG_VLAN_XLT_ACTION_STPID_CMD_LEN     2
	#define EG_VLAN_XLT_ACTION_STPID_CMD_DEFAULT 0x0
	/*[field] STPID_INDEX*/
	#define EG_VLAN_XLT_ACTION_STPID_INDEX
	#define EG_VLAN_XLT_ACTION_STPID_INDEX_OFFSET  57
	#define EG_VLAN_XLT_ACTION_STPID_INDEX_LEN     2
	#define EG_VLAN_XLT_ACTION_STPID_INDEX_DEFAULT 0x0
	/*[field] CTPID_CMD*/
	#define EG_VLAN_XLT_ACTION_CTPID_CMD
	#define EG_VLAN_XLT_ACTION_CTPID_CMD_OFFSET  59
	#define EG_VLAN_XLT_ACTION_CTPID_CMD_LEN     2
	#define EG_VLAN_XLT_ACTION_CTPID_CMD_DEFAULT 0x0
	/*[field] CTPID_INDEX*/
	#define EG_VLAN_XLT_ACTION_CTPID_INDEX
	#define EG_VLAN_XLT_ACTION_CTPID_INDEX_OFFSET  61
	#define EG_VLAN_XLT_ACTION_CTPID_INDEX_LEN     2
	#define EG_VLAN_XLT_ACTION_CTPID_INDEX_DEFAULT 0x0
	/*[field] DSCP_PBIT_MAPPING_INDEX*/
	#define EG_VLAN_XLT_ACTION_DSCP_PBIT_MAPPING_INDEX
	#define EG_VLAN_XLT_ACTION_DSCP_PBIT_MAPPING_INDEX_OFFSET  63
	#define EG_VLAN_XLT_ACTION_DSCP_PBIT_MAPPING_INDEX_LEN     1
	#define EG_VLAN_XLT_ACTION_DSCP_PBIT_MAPPING_INDEX_DEFAULT 0x0
	/*[field] COUNTER_EN*/
	#define EG_VLAN_XLT_ACTION_COUNTER_EN
	#define EG_VLAN_XLT_ACTION_COUNTER_EN_OFFSET  64
	#define EG_VLAN_XLT_ACTION_COUNTER_EN_LEN     1
	#define EG_VLAN_XLT_ACTION_COUNTER_EN_DEFAULT 0x0
	/*[field] COUNTER_ID*/
	#define EG_VLAN_XLT_ACTION_COUNTER_ID
	#define EG_VLAN_XLT_ACTION_COUNTER_ID_OFFSET  65
	#define EG_VLAN_XLT_ACTION_COUNTER_ID_LEN     7
	#define EG_VLAN_XLT_ACTION_COUNTER_ID_DEFAULT 0x0
	/*[field] VNI_RESV_EN*/
	#define EG_VLAN_XLT_ACTION_VNI_RESV_EN
	#define EG_VLAN_XLT_ACTION_VNI_RESV_EN_OFFSET  72
	#define EG_VLAN_XLT_ACTION_VNI_RESV_EN_LEN     1
	#define EG_VLAN_XLT_ACTION_VNI_RESV_EN_DEFAULT 0x0
	/*[field] VNI_RESV*/
	#define EG_VLAN_XLT_ACTION_VNI_RESV
	#define EG_VLAN_XLT_ACTION_VNI_RESV_OFFSET  73
	#define EG_VLAN_XLT_ACTION_VNI_RESV_LEN     32
	#define EG_VLAN_XLT_ACTION_VNI_RESV_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define EG_VLAN_XLT_ACTION_FWD_CMD
	#define EG_VLAN_XLT_ACTION_FWD_CMD_OFFSET  105
	#define EG_VLAN_XLT_ACTION_FWD_CMD_LEN     2
	#define EG_VLAN_XLT_ACTION_FWD_CMD_DEFAULT 0x0

struct eg_vlan_xlt_action {
	a_uint32_t  vid_swap_cmd:1;
	a_uint32_t  xlt_svid_cmd:3;
	a_uint32_t  xlt_svid:12;
	a_uint32_t  xlt_cvid_cmd:3;
	a_uint32_t  xlt_cvid:12;
	a_uint32_t  pcp_swap_cmd:1;
	a_uint32_t  xlt_spcp_cmd:4;
	a_uint32_t  xlt_spcp:3;
	a_uint32_t  xlt_cpcp_cmd:4;
	a_uint32_t  xlt_cpcp:3;
	a_uint32_t  dei_swap_cmd:1;
	a_uint32_t  xlt_sdei_cmd:2;
	a_uint32_t  xlt_sdei:1;
	a_uint32_t  xlt_cdei_cmd:2;
	a_uint32_t  xlt_cdei:1;
	a_uint32_t  tags_to_remove:2;
	a_uint32_t  stpid_cmd:2;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_cmd:2;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dscp_pbit_mapping_index:1;
	a_uint32_t  counter_en:1;
	a_uint32_t  counter_id:7;
	a_uint32_t  vni_resv_en:1;
	a_uint32_t  vni_resv_0:23;
	a_uint32_t  vni_resv_1:9;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  _reserved0:21;
};

union eg_vlan_xlt_action_u {
	a_uint32_t val[4];
	struct eg_vlan_xlt_action bf;
};

/*[table] EG_VLAN_XLT_RULE*/
#define EG_VLAN_XLT_RULE
#define EG_VLAN_XLT_RULE_ADDRESS 0x7000
#define EG_VLAN_XLT_RULE_NUM     128
#define EG_VLAN_XLT_RULE_INC     0x10
#define EG_VLAN_XLT_RULE_TYPE    REG_TYPE_RW
#define EG_VLAN_XLT_RULE_DEFAULT 0x0
	/*[field] VALID*/
	#define EG_VLAN_XLT_RULE_VALID
	#define EG_VLAN_XLT_RULE_VALID_OFFSET  0
	#define EG_VLAN_XLT_RULE_VALID_LEN     1
	#define EG_VLAN_XLT_RULE_VALID_DEFAULT 0x0
	/*[field] PORT_BITMAP*/
	#define EG_VLAN_XLT_RULE_PORT_BITMAP
	#define EG_VLAN_XLT_RULE_PORT_BITMAP_OFFSET  1
	#define EG_VLAN_XLT_RULE_PORT_BITMAP_LEN     9
	#define EG_VLAN_XLT_RULE_PORT_BITMAP_DEFAULT 0x0
	/*[field] VSI_INCL*/
	#define EG_VLAN_XLT_RULE_VSI_INCL
	#define EG_VLAN_XLT_RULE_VSI_INCL_OFFSET  10
	#define EG_VLAN_XLT_RULE_VSI_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_VSI_INCL_DEFAULT 0x0
	/*[field] VSI*/
	#define EG_VLAN_XLT_RULE_VSI
	#define EG_VLAN_XLT_RULE_VSI_OFFSET  11
	#define EG_VLAN_XLT_RULE_VSI_LEN     6
	#define EG_VLAN_XLT_RULE_VSI_DEFAULT 0x0
	/*[field] VSI_VALID*/
	#define EG_VLAN_XLT_RULE_VSI_VALID
	#define EG_VLAN_XLT_RULE_VSI_VALID_OFFSET  17
	#define EG_VLAN_XLT_RULE_VSI_VALID_LEN     1
	#define EG_VLAN_XLT_RULE_VSI_VALID_DEFAULT 0x0
	/*[field] SKEY_FMT*/
	#define EG_VLAN_XLT_RULE_SKEY_FMT
	#define EG_VLAN_XLT_RULE_SKEY_FMT_OFFSET  18
	#define EG_VLAN_XLT_RULE_SKEY_FMT_LEN     3
	#define EG_VLAN_XLT_RULE_SKEY_FMT_DEFAULT 0x0
	/*[field] SKEY_VID_INCL*/
	#define EG_VLAN_XLT_RULE_SKEY_VID_INCL
	#define EG_VLAN_XLT_RULE_SKEY_VID_INCL_OFFSET  21
	#define EG_VLAN_XLT_RULE_SKEY_VID_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_SKEY_VID_INCL_DEFAULT 0x0
	/*[field] SKEY_VID*/
	#define EG_VLAN_XLT_RULE_SKEY_VID
	#define EG_VLAN_XLT_RULE_SKEY_VID_OFFSET  22
	#define EG_VLAN_XLT_RULE_SKEY_VID_LEN     12
	#define EG_VLAN_XLT_RULE_SKEY_VID_DEFAULT 0x0
	/*[field] SKEY_PCP_INCL*/
	#define EG_VLAN_XLT_RULE_SKEY_PCP_INCL
	#define EG_VLAN_XLT_RULE_SKEY_PCP_INCL_OFFSET  34
	#define EG_VLAN_XLT_RULE_SKEY_PCP_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_SKEY_PCP_INCL_DEFAULT 0x0
	/*[field] SKEY_PCP*/
	#define EG_VLAN_XLT_RULE_SKEY_PCP
	#define EG_VLAN_XLT_RULE_SKEY_PCP_OFFSET  35
	#define EG_VLAN_XLT_RULE_SKEY_PCP_LEN     3
	#define EG_VLAN_XLT_RULE_SKEY_PCP_DEFAULT 0x0
	/*[field] SKEY_DEI_INCL*/
	#define EG_VLAN_XLT_RULE_SKEY_DEI_INCL
	#define EG_VLAN_XLT_RULE_SKEY_DEI_INCL_OFFSET  38
	#define EG_VLAN_XLT_RULE_SKEY_DEI_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_SKEY_DEI_INCL_DEFAULT 0x0
	/*[field] SKEY_DEI*/
	#define EG_VLAN_XLT_RULE_SKEY_DEI
	#define EG_VLAN_XLT_RULE_SKEY_DEI_OFFSET  39
	#define EG_VLAN_XLT_RULE_SKEY_DEI_LEN     1
	#define EG_VLAN_XLT_RULE_SKEY_DEI_DEFAULT 0x0
	/*[field] CKEY_FMT*/
	#define EG_VLAN_XLT_RULE_CKEY_FMT
	#define EG_VLAN_XLT_RULE_CKEY_FMT_OFFSET  40
	#define EG_VLAN_XLT_RULE_CKEY_FMT_LEN     3
	#define EG_VLAN_XLT_RULE_CKEY_FMT_DEFAULT 0x0
	/*[field] CKEY_VID_INCL*/
	#define EG_VLAN_XLT_RULE_CKEY_VID_INCL
	#define EG_VLAN_XLT_RULE_CKEY_VID_INCL_OFFSET  43
	#define EG_VLAN_XLT_RULE_CKEY_VID_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_CKEY_VID_INCL_DEFAULT 0x0
	/*[field] CKEY_VID*/
	#define EG_VLAN_XLT_RULE_CKEY_VID
	#define EG_VLAN_XLT_RULE_CKEY_VID_OFFSET  44
	#define EG_VLAN_XLT_RULE_CKEY_VID_LEN     12
	#define EG_VLAN_XLT_RULE_CKEY_VID_DEFAULT 0x0
	/*[field] CKEY_PCP_INCL*/
	#define EG_VLAN_XLT_RULE_CKEY_PCP_INCL
	#define EG_VLAN_XLT_RULE_CKEY_PCP_INCL_OFFSET  56
	#define EG_VLAN_XLT_RULE_CKEY_PCP_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_CKEY_PCP_INCL_DEFAULT 0x0
	/*[field] CKEY_PCP*/
	#define EG_VLAN_XLT_RULE_CKEY_PCP
	#define EG_VLAN_XLT_RULE_CKEY_PCP_OFFSET  57
	#define EG_VLAN_XLT_RULE_CKEY_PCP_LEN     3
	#define EG_VLAN_XLT_RULE_CKEY_PCP_DEFAULT 0x0
	/*[field] CKEY_DEI_INCL*/
	#define EG_VLAN_XLT_RULE_CKEY_DEI_INCL
	#define EG_VLAN_XLT_RULE_CKEY_DEI_INCL_OFFSET  60
	#define EG_VLAN_XLT_RULE_CKEY_DEI_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_CKEY_DEI_INCL_DEFAULT 0x0
	/*[field] CKEY_DEI*/
	#define EG_VLAN_XLT_RULE_CKEY_DEI
	#define EG_VLAN_XLT_RULE_CKEY_DEI_OFFSET  61
	#define EG_VLAN_XLT_RULE_CKEY_DEI_LEN     1
	#define EG_VLAN_XLT_RULE_CKEY_DEI_DEFAULT 0x0
	/*[field] PORT_TYPE*/
	#define EG_VLAN_XLT_RULE_PORT_TYPE
	#define EG_VLAN_XLT_RULE_PORT_TYPE_OFFSET  62
	#define EG_VLAN_XLT_RULE_PORT_TYPE_LEN     2
	#define EG_VLAN_XLT_RULE_PORT_TYPE_DEFAULT 0x0
	/*[field] STPID_INCL*/
	#define EG_VLAN_XLT_RULE_STPID_INCL
	#define EG_VLAN_XLT_RULE_STPID_INCL_OFFSET  64
	#define EG_VLAN_XLT_RULE_STPID_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_STPID_INCL_DEFAULT 0x0
	/*[field] STPID_INDEX*/
	#define EG_VLAN_XLT_RULE_STPID_INDEX
	#define EG_VLAN_XLT_RULE_STPID_INDEX_OFFSET  65
	#define EG_VLAN_XLT_RULE_STPID_INDEX_LEN     2
	#define EG_VLAN_XLT_RULE_STPID_INDEX_DEFAULT 0x0
	/*[field] CTPID_INCL*/
	#define EG_VLAN_XLT_RULE_CTPID_INCL
	#define EG_VLAN_XLT_RULE_CTPID_INCL_OFFSET  67
	#define EG_VLAN_XLT_RULE_CTPID_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_CTPID_INCL_DEFAULT 0x0
	/*[field] CTPID_INDEX*/
	#define EG_VLAN_XLT_RULE_CTPID_INDEX
	#define EG_VLAN_XLT_RULE_CTPID_INDEX_OFFSET  68
	#define EG_VLAN_XLT_RULE_CTPID_INDEX_LEN     2
	#define EG_VLAN_XLT_RULE_CTPID_INDEX_DEFAULT 0x0
	/*[field] PROT_INCL*/
	#define EG_VLAN_XLT_RULE_PROT_INCL
	#define EG_VLAN_XLT_RULE_PROT_INCL_OFFSET  70
	#define EG_VLAN_XLT_RULE_PROT_INCL_LEN     1
	#define EG_VLAN_XLT_RULE_PROT_INCL_DEFAULT 0x0
	/*[field] PROT_VALUE*/
	#define EG_VLAN_XLT_RULE_PROT_VALUE
	#define EG_VLAN_XLT_RULE_PROT_VALUE_OFFSET  71
	#define EG_VLAN_XLT_RULE_PROT_VALUE_LEN     16
	#define EG_VLAN_XLT_RULE_PROT_VALUE_DEFAULT 0x0
	/*[field] DHCP_TYPE*/
	#define EG_VLAN_XLT_RULE_DHCP_TYPE
	#define EG_VLAN_XLT_RULE_DHCP_TYPE_OFFSET  87
	#define EG_VLAN_XLT_RULE_DHCP_TYPE_LEN     3
	#define EG_VLAN_XLT_RULE_DHCP_TYPE_DEFAULT 0x0
	/*[field] MC_TYPE*/
	#define EG_VLAN_XLT_RULE_MC_TYPE
	#define EG_VLAN_XLT_RULE_MC_TYPE_OFFSET  90
	#define EG_VLAN_XLT_RULE_MC_TYPE_LEN     3
	#define EG_VLAN_XLT_RULE_MC_TYPE_DEFAULT 0x0

struct eg_vlan_xlt_rule {
	a_uint32_t  valid:1;
	a_uint32_t  port_bitmap:9;
	a_uint32_t  vsi_incl:1;
	a_uint32_t  vsi:6;
	a_uint32_t  vsi_valid:1;
	a_uint32_t  skey_fmt:3;
	a_uint32_t  skey_vid_incl:1;
	a_uint32_t  skey_vid_0:10;
	a_uint32_t  skey_vid_1:2;
	a_uint32_t  skey_pcp_incl:1;
	a_uint32_t  skey_pcp:3;
	a_uint32_t  skey_dei_incl:1;
	a_uint32_t  skey_dei:1;
	a_uint32_t  ckey_fmt:3;
	a_uint32_t  ckey_vid_incl:1;
	a_uint32_t  ckey_vid:12;
	a_uint32_t  ckey_pcp_incl:1;
	a_uint32_t  ckey_pcp:3;
	a_uint32_t  ckey_dei_incl:1;
	a_uint32_t  ckey_dei:1;
	a_uint32_t  port_type:2;
	a_uint32_t  stpid_incl:1;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_incl:1;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  prot_incl:1;
	a_uint32_t  prot_value:16;
	a_uint32_t  dhcp_type:3;
	a_uint32_t  mc_type:3;
	a_uint32_t  _reserved0:3;
};

union eg_vlan_xlt_rule_u {
	a_uint32_t val[3];
	struct eg_vlan_xlt_rule bf;
};

/*[table] VLAN_DEV_TX_COUNTER_TBL*/
#define VLAN_DEV_TX_COUNTER_TBL
#define VLAN_DEV_TX_COUNTER_TBL_ADDRESS 0x18000
#define VLAN_DEV_TX_COUNTER_TBL_NUM     128
#define VLAN_DEV_TX_COUNTER_TBL_INC     0x20
#define VLAN_DEV_TX_COUNTER_TBL_TYPE    REG_TYPE_RW
#define VLAN_DEV_TX_COUNTER_TBL_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define VLAN_DEV_TX_COUNTER_TBL_TX_PACKETS
	#define VLAN_DEV_TX_COUNTER_TBL_TX_PACKETS_OFFSET  0
	#define VLAN_DEV_TX_COUNTER_TBL_TX_PACKETS_LEN     32
	#define VLAN_DEV_TX_COUNTER_TBL_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define VLAN_DEV_TX_COUNTER_TBL_TX_BYTES
	#define VLAN_DEV_TX_COUNTER_TBL_TX_BYTES_OFFSET  32
	#define VLAN_DEV_TX_COUNTER_TBL_TX_BYTES_LEN     40
	#define VLAN_DEV_TX_COUNTER_TBL_TX_BYTES_DEFAULT 0x0
	/*[field] DROP_PACKETS*/
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_PACKETS
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_PACKETS_OFFSET  72
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_PACKETS_LEN     32
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_PACKETS_DEFAULT 0x0
	/*[field] DROP_BYTES*/
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_BYTES
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_BYTES_OFFSET  104
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_BYTES_LEN     40
	#define VLAN_DEV_TX_COUNTER_TBL_DROP_BYTES_DEFAULT 0x0

struct vlan_dev_tx_counter_tbl {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  drop_packets_0:24;
	a_uint32_t  drop_packets_1:8;
	a_uint32_t  drop_bytes_0:24;
	a_uint32_t  drop_bytes_1:16;
	a_uint32_t  _reserved0:16;
};

union vlan_dev_tx_counter_tbl_u {
	a_uint32_t val[5];
	struct vlan_dev_tx_counter_tbl bf;
};

/*[register] EDMA_VLAN_TPID_REG*/
#define EDMA_VLAN_TPID_REG
#define EDMA_VLAN_TPID_REG_ADDRESS 0x8
#define EDMA_VLAN_TPID_REG_NUM     1
#define EDMA_VLAN_TPID_REG_INC     0x4
#define EDMA_VLAN_TPID_REG_TYPE    REG_TYPE_RW
#define EDMA_VLAN_TPID_REG_DEFAULT 0x810088a8
	/*[field] SVLAN_TPID*/
	#define EDMA_VLAN_TPID_REG_SVLAN_TPID
	#define EDMA_VLAN_TPID_REG_SVLAN_TPID_OFFSET  0
	#define EDMA_VLAN_TPID_REG_SVLAN_TPID_LEN     16
	#define EDMA_VLAN_TPID_REG_SVLAN_TPID_DEFAULT 0x88a8
	/*[field] CVLAN_TPID*/
	#define EDMA_VLAN_TPID_REG_CVLAN_TPID
	#define EDMA_VLAN_TPID_REG_CVLAN_TPID_OFFSET  16
	#define EDMA_VLAN_TPID_REG_CVLAN_TPID_LEN     16
	#define EDMA_VLAN_TPID_REG_CVLAN_TPID_DEFAULT 0x8100

struct edma_vlan_tpid_reg {
	a_uint32_t  svlan_tpid:16;
	a_uint32_t  cvlan_tpid:16;
};

union edma_vlan_tpid_reg_u {
	a_uint32_t val;
	struct edma_vlan_tpid_reg bf;
};

#endif
