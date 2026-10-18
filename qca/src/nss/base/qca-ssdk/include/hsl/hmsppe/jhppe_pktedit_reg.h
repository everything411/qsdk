/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_PKTEDIT_REG_H_
#define _JHPPE_PKTEDIT_REG_H_

/*[register] TPR_PKT_LEN_CTRL*/
#define TPR_PKT_LEN_CTRL
#define TPR_PKT_LEN_CTRL_ADDRESS 0xe20
#define TPR_PKT_LEN_CTRL_NUM     1
#define TPR_PKT_LEN_CTRL_INC     0x4
#define TPR_PKT_LEN_CTRL_TYPE    REG_TYPE_RW
#define TPR_PKT_LEN_CTRL_DEFAULT 0x0
	/*[field] ORG_LEN_EN*/
	#define TPR_PKT_LEN_CTRL_ORG_LEN_EN
	#define TPR_PKT_LEN_CTRL_ORG_LEN_EN_OFFSET  0
	#define TPR_PKT_LEN_CTRL_ORG_LEN_EN_LEN     1
	#define TPR_PKT_LEN_CTRL_ORG_LEN_EN_DEFAULT 0x0
	/*[field] SNAP_OUI_00_CHECK*/
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_00_CHECK
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_00_CHECK_OFFSET  1
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_00_CHECK_LEN     1
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_00_CHECK_DEFAULT 0x0
	/*[field] SNAP_OUI_F8_CHECK*/
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_F8_CHECK
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_F8_CHECK_OFFSET  2
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_F8_CHECK_LEN     1
	#define TPR_PKT_LEN_CTRL_SNAP_OUI_F8_CHECK_DEFAULT 0x0

struct tpr_pkt_len_ctrl {
	a_uint32_t  org_len_en:1;
	a_uint32_t  snap_oui_00_check:1;
	a_uint32_t  snap_oui_f8_check:1;
	a_uint32_t  _reserved0:29;
};

union tpr_pkt_len_ctrl_u {
	a_uint32_t val;
	struct tpr_pkt_len_ctrl bf;
};

/*[register] PKT_LEN_CTRL*/
#define PKT_LEN_CTRL
#define PKT_LEN_CTRL_ADDRESS 0x530
#define PKT_LEN_CTRL_NUM     1
#define PKT_LEN_CTRL_INC     0x4
#define PKT_LEN_CTRL_TYPE    REG_TYPE_RW
#define PKT_LEN_CTRL_DEFAULT 0x0
	/*[field] ORG_LEN_EN*/
	#define PKT_LEN_CTRL_ORG_LEN_EN
	#define PKT_LEN_CTRL_ORG_LEN_EN_OFFSET  0
	#define PKT_LEN_CTRL_ORG_LEN_EN_LEN     1
	#define PKT_LEN_CTRL_ORG_LEN_EN_DEFAULT 0x0
	/*[field] SNAP_OUI_00_CHECK*/
	#define PKT_LEN_CTRL_SNAP_OUI_00_CHECK
	#define PKT_LEN_CTRL_SNAP_OUI_00_CHECK_OFFSET  1
	#define PKT_LEN_CTRL_SNAP_OUI_00_CHECK_LEN     1
	#define PKT_LEN_CTRL_SNAP_OUI_00_CHECK_DEFAULT 0x0
	/*[field] SNAP_OUI_F8_CHECK*/
	#define PKT_LEN_CTRL_SNAP_OUI_F8_CHECK
	#define PKT_LEN_CTRL_SNAP_OUI_F8_CHECK_OFFSET  2
	#define PKT_LEN_CTRL_SNAP_OUI_F8_CHECK_LEN     1
	#define PKT_LEN_CTRL_SNAP_OUI_F8_CHECK_DEFAULT 0x0

struct pkt_len_ctrl {
	a_uint32_t  org_len_en:1;
	a_uint32_t  snap_oui_00_check:1;
	a_uint32_t  snap_oui_f8_check:1;
	a_uint32_t  _reserved0:29;
};

union pkt_len_ctrl_u {
	a_uint32_t val;
	struct pkt_len_ctrl bf;
};

/*[register] TL_PKT_LEN_CTRL*/
#define TL_PKT_LEN_CTRL
#define TL_PKT_LEN_CTRL_ADDRESS 0x120
#define TL_PKT_LEN_CTRL_NUM     1
#define TL_PKT_LEN_CTRL_INC     0x4
#define TL_PKT_LEN_CTRL_TYPE    REG_TYPE_RW
#define TL_PKT_LEN_CTRL_DEFAULT 0x0
	/*[field] ORG_LEN_EN*/
	#define TL_PKT_LEN_CTRL_ORG_LEN_EN
	#define TL_PKT_LEN_CTRL_ORG_LEN_EN_OFFSET  0
	#define TL_PKT_LEN_CTRL_ORG_LEN_EN_LEN     1
	#define TL_PKT_LEN_CTRL_ORG_LEN_EN_DEFAULT 0x0

struct tl_pkt_len_ctrl {
	a_uint32_t  org_len_en:1;
	a_uint32_t  _reserved0:31;
};

union tl_pkt_len_ctrl_u {
	a_uint32_t val;
	struct tl_pkt_len_ctrl bf;
};

#endif
