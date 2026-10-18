/*
 * Copyright (c) 2012, 2016-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: GPL-2.0
 */

#ifndef _NSS_PPE_V4_H_
#define _NSS_PPE_V4_H_

/* ============================================================================
 * SECTION 1: #DEFINE CONSTANTS
 * ============================================================================
 */
/* VP Port Table Definitions */
#define VP_PORT_TBL_PHYSICAL_PORT_MASK		0xFF
#define VP_PORT_TBL_PHYSICAL_PORT_SHIFT		0
#define VP_PORT_TBL_DST_PORT_ID_VALID		BIT(9)
#define VP_PORT_TBL_DST_PORT_ID_MASK		0x3FF
#define VP_PORT_TBL_DST_PORT_ID_SHIFT		10

/* Queue Offset Definitions */
#define SERVICE_CODE_QUEUE_OFFSET   2048
#define CPU_CODE_QUEUE_OFFSET       1024
#define VP_PORT_QUEUE_OFFSET        0
#define VP_PORT_ISRAM_QUEUE_OFFSET  3072

/* Register Offsets */
#define UCAST_QUEUE_MAP_TBL_OFFSET  0x40000

/* Maximum Values */
#define PPE_MAX_SERVICE_CODE_NUM   256
#define PPE_MAX_CPU_CODE_NUM       256
#define PPE_L1SCHEDULER_CFG_MAX    64
#define PPE_L0SCHEDULER_CFG_MAX    300
#define PPE_PRI_MAX                16
#define PPE_CPU_PRI_NUM            1
#define PPE_MGMT_ARP_REP_CPU_CODE  101

/* Flow Map Table */
#define L0_FLOW_MAP_TBL_MAX_ENTRY   300
#define L1_FLOW_MAP_TBL_MAX_ENTRY   64
#define UCAST_QUEUE_ID_MAX          256

/* Port ID Macro */
#define FAL_PORT_ID_VALUE(port_id)  ((port_id) & 0xFF)

/* Field Offset Macro */
#define SW_FIELD_OFFSET_IN_WORD(N)  (32 - (N) % 32)

/* Shorter aliases for commonly used field offset constants */
#define YEL_MIN_SHIFT		\
	SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET)
#define GRN_RESUME_SHIFT	\
	SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET)
#define RED_RESUME_SHIFT	\
	SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET)
#define CEILING_SHIFT		\
	SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET)
#define DP_THRD_SHIFT		SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_DP_THRD_OFFSET)
#define GRP_YEL_RESUME_SHIFT	\
	SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_OFFSET)
#define MUL_RED_RESUME_SHIFT	\
	SW_FIELD_OFFSET_IN_WORD(AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET)
#define MUL_GAP_GRN_YEL_SHIFT	\
	SW_FIELD_OFFSET_IN_WORD(AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_OFFSET)
#define PALLOC_SHIFT		SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET)
#define PORT_CEILING_SHIFT	SW_FIELD_OFFSET_IN_WORD(PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET)

/* AC Group Configuration */
#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET  87

/* ACL Rule Types */
#define ADPT_ACL_HPPE_MAC_DA_RULE   0
#define ADPT_ACL_HPPE_MAC_SA_RULE   1
#define ADPT_ACL_HPPE_IPV4_DIP_RULE 4

#define MAX_RULE    512

/* IPO (Ingress Policy) Base Addresses */
#define IPO_CSR_BASE_ADDR           0x0b0000
#define IPO_RULE_REG_ADDRESS        0x0
#define IPO_RULE_REG_INC            0x10
#define IPO_MASK_REG_ADDRESS        0x2000
#define IPO_MASK_REG_INC            0x10
#define IPO_ACTION_ADDRESS          0x8000
#define IPO_ACTION_INC              0x20

/* CST (Common Spanning Tree) State Register */
#define CST_STATE_ADDRESS           0x100
#define CST_STATE_INC               0x4

/* QM (Queue Manager) Configuration */
#define IPQ_PPE_QM_UCAST_CEILING    2200
#define IPQ_PPE_QM_UCAST_WEIGHT     7
#define IPQ_PPE_QM_RESUME_OFFSET    36
#define IPQ_PPE_QM_MCAST_GREEN_MAX  250
#define IPQ_PPE_UCAST_QUEUE_MAX     256
#define IPQ_PPE_L0SCHEDULER_CFG_MAX 300

/* AC (Admission Control) Queue Configuration Table */
#define AC_UNI_QUEUE_CFG_TBL_OFFSET 0x4e000
#define AC_UNI_QUEUE_CFG_TBL_INC    0x20
#define AC_MUL_QUEUE_CFG_TBL_OFFSET 0x50000
#define AC_MUL_QUEUE_CFG_TBL_INC    0x10

/* AC Unicast Queue Configuration Field Offsets */
#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET  22
#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_LEN     12
#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET  118
#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_LEN     12
#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET 58
#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET 94

/* AC Group Configuration Field Offsets */
#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_OFFSET 27
#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_OFFSET 63

/* AC Multicast Queue Configuration Field Offsets */
#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET 53
#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_OFFSET 29

/* Buffer Manager Port Configuration */
#define PPE_BM_PORT_NUM         40
#define PPE_BM_PHY_PORT_MAX     39
#define PPE_BM_PHY_PORT_OFFSET  32

/* Port Definitions */
#define PORT_CPU		0
#define PORT_ETH_START		1
#define PORT_ETH_END		6
#define PORT_LOOPBACK		7
#define IPQ5210_NUM_PORTS	8

/* ============================================================================
 * SECTION 3: ENUM DEFINITIONS
 * ============================================================================
 */

/* Port Counter Mode */
enum fal_port_cnt_mode_t {
	FAL_PORT_CNT_MODE_IP_PKT,	/* the outer IP header + inner packet counted */
	FAL_PORT_CNT_MODE_FULL_PKT,	/* for RX, inner packet length,
					 * for TX vport, the full packet
					 * (outer header + inner packet)
					 * length counter
					 */
	FAL_PORT_CNT_MODE_BUTT,
};

/* SRAM Queue Type */
enum fal_sram_queue_type {
	FAL_ESRAM_QUEUE = 0,
	FAL_ISRAM_QUEUE,
};

/* QoS DRR Frame Mode */
enum fal_qos_drr_frame_mode {
	FAL_DRR_IPG_PREAMBLE_FRAME_CRC = 0,	/* IPG + Preamble + Frame + CRC */
	FAL_DRR_FRAME_CRC,			/* Frame + CRC */
	FAL_DRR_L3_EXCLUDE_CRC			/* after Ethernet type exclude CRC */
};

/* AC Type */
enum fal_ac_type_t {
	FAL_AC_QUEUE = 0,
	FAL_AC_GROUP
};

/* AC Object */
struct fal_ac_obj_t {
	enum fal_ac_type_t type;
	u32 index;
};

/* AC Control */
struct fal_ac_ctrl_t {
	bool ac_en;	/* 0 for disable and 1 for enable */
	bool ac_fc_en;	/* ac for flow control packets */
};

/* ============================================================================
 * SECTION 2: STRUCTURE DEFINITIONS
 * ============================================================================
 */

/* Port Counter Configuration */
struct fal_port_cnt_cfg_t {
	bool rx_cnt_en;		/* Enable/disable port rx counter */
	bool tl_rx_cnt_en;	/* Enable/disable tunnel source port rx counter */
	bool uc_tx_cnt_en;	/* Enable/disable port unicast tx and l3_if tx counter */
	bool mc_tx_cnt_en;	/* Enable/disable physical port multicast tx counter */
	enum fal_port_cnt_mode_t rx_cnt_mode;	/* as described as fal_port_cnt_mode_t */
	enum fal_port_cnt_mode_t tx_cnt_mode;	/* as described as fal_port_cnt_mode_t */
};

/* MRU/MTU Control Table Union */
union mru_mtu_ctrl_tbl_u {
	u32 val[3];
	struct {
		u32 mru:14;
		u32 mru_cmd:2;
		u32 mtu:14;
		u32 mtu_cmd:2;
		u32 rx_cnt_en:1;
		u32 tx_cnt_en:1;
		u32 src_profile:2;
		u32 pcp_qos_group_id:1;
		u32 dscp_qos_group_id:1;
		u32 pcp_res_prec_force:1;
		u32 dscp_res_prec_force:1;
		u32 preheader_res_prec:3;
		u32 pcp_res_prec:3;
		u32 dscp_res_prec:3;
		u32 flow_res_prec:3;
		u32 pre_acl_res_prec:3;
		u32 post_acl_res_prec:3;
		u32 source_filtering_bypass:1;
		u32 source_filtering_mode:1;
		u32 pre_ipo_outer_res_prec:3;
		u32 pre_ipo_inner_res_prec_0:1;
		u32 pre_ipo_inner_res_prec_1:2;
		u32 pcp_qos_mode:1;
		u32 default_pcp_dei:4;
		u32 _reserved0:25;
	} bf;
};

/* MC MTU Control Table Union */
union mc_mtu_ctrl_tbl_u {
	u32 val;
	struct {
		u32 mtu:14;
		u32 mtu_cmd:2;
		u32 tx_cnt_en:1;
		u32 _reserved0:15;
	} bf;
};

/* Port Egress VLAN Union */
union port_eg_vlan_u {
	u32 val;
	struct {
		u32 port_vlan_type:1;
		u32 port_eg_vlan_ctag_mode:2;
		u32 port_eg_vlan_stag_mode:2;
		u32 vsi_tag_mode_en:1;
		u32 port_eg_pcp_prop_cmd:1;
		u32 port_eg_dei_prop_cmd:1;
		u32 tx_counting_en:1;
		u32 _reserved0:23;
	} bf;
};

/* Unicast Queue Destination */
struct ppe_ucast_queue_dest {
	u8 src_profile;
	int service_code_en;
	u16 service_code;
	int cpu_code_en;
	u16 cpu_code;
	u32 dst_port;
};

/* Unicast Queue Map Table Union */
union ucast_queue_map_tbl_u {
	u32 val;
	struct {
		u32 profile_id:4;
		u32 queue_id:8;
		u32 _reserved0:20;
	} bf;
};

/* Unicast Priority Map Table Union */
union ucast_priority_map_tbl_u {
	u32 val;
	struct {
		u32 class:4;
		u32 _reserved0:28;
	} bf;
};

/* Egress Bridge Configuration Union */
union eg_bridge_config_u {
	u32 val;
	struct {
		u32 bridge_type:1;
		u32 pkt_l2_edit_en:1;
		u32 queue_cnt_en:1;
		u32 _reserved0:5;
		u32 ppe_eip_rsv_w4_3130:2;
		u32 field_update_enable:1;
		u32 passthrough_cpu_code0:8;
		u32 passthrough_cpu_code1:8;
		u32 _reserved1:5;
	} bf;
};

/* QoS Scheduler Configuration */
struct fal_qos_scheduler_cfg {
	u8 sp_id;		/* SP id L0:0~63 L1:0~7 */
	u8 e_pri;		/* SP priority for E path:0~7 low to high */
	u8 c_pri;		/* SP priority for C path: 0~7 low to high */
	u8 c_drr_id;		/* C DRR ID L0:0~159 L1:0~35 */
	u8 e_drr_id;		/* E DRR ID L0:0~159 L1:0~35 */
	u16 e_drr_wt;		/* DRR weight in E DRR: 0~1023 */
	u16 c_drr_wt;		/* DRR weight in C DRR: 0~1023 */
	u8 c_drr_unit;		/* 0:byte based; 1:packet based */
	u8 e_drr_unit;		/* 0:byte based; 1:packet based */
	enum fal_qos_drr_frame_mode drr_frame_mode;
};

/* L1 Flow Map Table Union */
union l1_flow_map_tbl_u {
	u32 val[2];
	struct {
		u32 sp_id:6;
		u32 c_pri:3;
		u32 e_pri:3;
		u32 c_drr_wt:10;
		u32 e_drr_wt:10;
		u32 c_drr_id:6;
		u32 e_drr_id:6;
		u32 c_drr_credit_unit:1;
		u32 e_drr_credit_unit:1;
		u32 _reserved0:18;
	} bf;
};

/* L1 Flow Port Map Table Union */
union l1_flow_port_map_tbl_u {
	u32 val;
	struct {
		u32 port_num:4;
		u32 _reserved0:28;
	} bf;
};

/* L1 Component Configuration Table Union */
union l1_comp_cfg_tbl_u {
	u32 val;
	struct {
		u32 shaper_meter_len:2;
		u32 drr_meter_len:2;
		u32 _reserved0:28;
	} bf;
};

/* L0 Flow Map Table Union */
union l0_flow_map_tbl_u {
	u32 val[2];
	struct {
		u32 sp_id:6;
		u32 c_pri:3;
		u32 e_pri:3;
		u32 c_drr_wt:10;
		u32 e_drr_wt:10;
		u32 c_drr_id:8;
		u32 e_drr_id:8;
		u32 c_drr_credit_unit:1;
		u32 e_drr_credit_unit:1;
		u32 _reserved0:14;
	} bf;
};

/* L0 Flow Port Map Table Union */
union l0_flow_port_map_tbl_u {
	u32 val;
	struct {
		u32 port_num:4;
		u32 _reserved0:28;
	} bf;
};

/* L0 Component Configuration Table Union */
union l0_comp_cfg_tbl_u {
	u32 val;
	struct {
		u32 shaper_meter_len:2;
		u32 drr_meter_len:2;
		u32 _reserved0:28;
	} bf;
};

/* AC Group Configuration Table Union */
union ac_grp_cfg_tbl_u {
	u32 val[4];
	struct {
		u32 ac_cfg_ac_en:1;
		u32 ac_cfg_force_ac_en:1;
		u32 ac_cfg_color_aware:1;
		u32 ac_grp_gap_grn_red:12;
		u32 ac_grp_gap_grn_yel:12;
		u32 ac_grp_dp_thrd_0:5;
		u32 ac_grp_dp_thrd_1:7;
		u32 ac_grp_limit:12;
		u32 ac_grp_red_resume_offset:12;
		u32 ac_grp_yel_resume_offset_0:1;
		u32 ac_grp_yel_resume_offset_1:11;
		u32 ac_grp_grn_resume_offset:12;
		u32 ac_grp_palloc_limit_0:9;
		u32 ac_grp_palloc_limit_1:3;
		u32 _reserved0:29;
	} bf;
};

/* ACL Rule Configuration */
struct ppe_acl_rule {
	phys_addr_t reg_base;
	u32 rule_id;
	u32 rule_type;
	u32 field0;
	u32 field1;
	u32 mask;
	u32 permit;
	u32 deny;
	u32 ipo_cnt;
};

/* AC Unicast Queue Configuration Table Union */
union ac_uni_queue_cfg_tbl_u {
	u32 val[5];
	struct {
		u32 ac_cfg_ac_en:1;
		u32 ac_cfg_wred_en:1;
		u32 ac_cfg_force_ac_en:1;
		u32 ac_cfg_color_aware:1;
		u32 ac_cfg_grp_id:2;
		u32 ac_cfg_pre_alloc_limit:12;
		u32 ac_cfg_shared_dynamic:1;
		u32 ac_cfg_shared_weight:3;
		u32 ac_cfg_shared_ceiling_0:10;
		u32 ac_cfg_shared_ceiling_1:2;
		u32 ac_cfg_gap_grn_grn_min:12;
		u32 ac_cfg_gap_grn_yel_max:12;
		u32 ac_cfg_gap_grn_yel_min_0:6;
		u32 ac_cfg_gap_grn_yel_min_1:6;
		u32 ac_cfg_gap_grn_red_max:12;
		u32 ac_cfg_gap_grn_red_min:12;
		u32 ac_cfg_red_resume_offset_0:2;
		u32 ac_cfg_red_resume_offset_1:10;
		u32 ac_cfg_yel_resume_offset:12;
		u32 ac_cfg_grn_resume_offset_0:10;
		u32 ac_cfg_grn_resume_offset_1:2;
		u32 _reserved0:30;
	} bf;
};

/* AC Multicast Queue Configuration Table Union */
union ac_mul_queue_cfg_tbl_u {
	u32 val[3];
	struct {
		u32 ac_cfg_ac_en:1;
		u32 ac_cfg_force_ac_en:1;
		u32 ac_cfg_color_aware:1;
		u32 ac_cfg_grp_id:2;
		u32 ac_cfg_pre_alloc_limit:12;
		u32 ac_cfg_shared_ceiling:12;
		u32 ac_cfg_gap_grn_yel_0:3;
		u32 ac_cfg_gap_grn_yel_1:9;
		u32 ac_cfg_gap_grn_red:12;
		u32 ac_cfg_red_resume_offset_0:11;
		u32 ac_cfg_red_resume_offset_1:1;
		u32 ac_cfg_yel_resume_offset:12;
		u32 ac_cfg_grn_resume_offset:12;
		u32 _reserved0:7;
	} bf;
};

/* Port Flow Control Mode Union */
union port_fc_mode_u {
	u32 val;
	struct {
		u32 fc_en:1;
		u32 _reserved0:31;
	} bf;
};

/* Port Group ID Union */
union port_group_id_u {
	u32 val;
	struct {
		u32 port_shared_group_id:2;
		u32 _reserved0:30;
	} bf;
};

/* Shared Group Configuration Union */
union shared_group_cfg_u {
	u32 val;
	struct {
		u32 shared_group_limit:12;
		u32 _reserved0:20;
	} bf;
};

/* Port Flow Control Configuration Union */
union port_fc_cfg_u {
	u32 val[2];
	struct {
		u32 port_react_limit:10;
		u32 port_resume_floor_th:9;
		u32 port_resume_offset:12;
		u32 port_shared_ceiling_0:1;
		u32 port_shared_ceiling_1:11;
		u32 port_shared_weight:3;
		u32 port_shared_dynamic:1;
		u32 port_pre_alloc:12;
		u32 _reserved0:5;
	} bf;
};

/* Port Flow Control Configuration Field Offset */
#define PORT_FC_CFG_PORT_SHARED_CEILING_OFFSET 31

/* CST (Common Spanning Tree) State Union */
union cst_state_u {
	u32 val;
	struct cst_state {
		u32 port_state:2;	/* Bits [1:0] - Port state */
		u32 _reserved0:30;	/* Bits [31:2] - Reserved */
	} bf;
};

/* L2 Global Configuration Register Union (from hppe_fdb_reg.h) */
union l2_global_conf_u {
	u32 val;
	struct l2_global_conf {
		u32 fdb_hash_mode_0:2;		/* [1:0]   - FDB hash mode 0 */
		u32 fdb_hash_mode_1:2;		/* [3:2]   - FDB hash mode 1 */
		u32 fdb_hash_full_fwd_cmd:2;	/* [5:4]   - FDB hash full forward command */
		u32 lrn_en:1;			/* [6]     - Learning enable */
		u32 age_en:1;			/* [7]     - Aging enable */
		u32 lrn_ctrl_mode:1;		/* [8]     - Learning control mode */
		u32 age_ctrl_mode:1;		/* [9]     - Aging control mode */
		u32 failover_en:1;		/* [10]    - Failover enable */
		u32 service_code_loop:1;	/* [11]    - Service code loop */
		u32 flow_cpy_escape:1;		/* [12]    - Flow copy escape */
		u32 mc_pvlan_isol_en:1;		/* [13]    - Multicast PVLAN isolation enable */
		u32 bc_pvlan_isol_en:1;		/* [14]    - Broadcast PVLAN isolation enable */
		u32 ipmc_en:1;			/* [15]    - IP multicast enable */
		u32 ipmc_hash_mode_0:2;		/* [17:16] - IPMC hash mode 0 */
		u32 ipmc_hash_mode_1:2;		/* [19:18] - IPMC hash mode 1 */
		u32 mc_vlan_match_mode:1;	/* [20]    - Multicast VLAN match mode */
		u32 mc_dmac_check_en:1;		/* [21]    - Multicast DMAC check enable */
		u32 ipmc_mismatch_act:2;	/* [23:22] - IPMC mismatch action */
		u32 dot1p_mapper_vlan_mode:2;	/* [25:24] - 802.1p mapper VLAN mode */
		u32 dot1p_mapper_pcp_mode:2;	/* [27:26] - 802.1p mapper PCP mode */
		u32 _reserved0:4;		/* [31:28] - Reserved */
	} bf;
};

/* Buffer Manager Dynamic Configuration */
struct bm_dynamic_cfg {
	u8 weight;		/* port weight in the shared group */
	u16 shared_ceiling;	/* Maximum shared buffers */
	u16 resume_off;		/* resume offset */
	u16 resume_min_thresh;	/* Minimum thresh for resume */
};

/* AC Dynamic Threshold Configuration */
struct fal_ac_dynamic_threshold {
	bool color_enable;	/* 1 for color aware and 0 for color-blind */
	bool wred_enable;	/* 1 for wred and 0 for tail drop */
	u8 shared_weight;	/* weight in the shared group */
	u16 green_min_off;	/* gap between green max and green min */
	u16 yel_max_off;	/* gap between green max and yel max */
	u16 yel_min_off;	/* gap between green max and yel min */
	u16 red_max_off;	/* gap between green max and red max */
	u16 red_min_off;	/* gap between green max and red min */
	u16 green_resume_off;	/* green resume offset */
	u16 yel_resume_off;	/* yellow resume offset */
	u16 red_resume_off;	/* red resume offset */
	u16 ceiling;		/* shared ceiling */
	bool status;		/* dynamic threshold enabled or not */
};

/* AC Static Threshold Configuration */
struct fal_ac_static_threshold {
	bool color_enable;	/* 1 for color aware and 0 for color-blind */
	bool wred_enable;	/* 1 for wred and 0 for tail drop */
	u16 green_max;
	u16 green_min_off;	/* gap between green max and green min */
	u16 yel_max_off;	/* gap between green max and yel max */
	u16 yel_min_off;	/* gap between green max and yel min */
	u16 red_max_off;	/* gap between green max and red max */
	u16 red_min_off;	/* gap between green max and red min */
	u16 green_resume_off;	/* green resume offset */
	u16 yel_resume_off;	/* yellow resume offset */
	u16 red_resume_off;	/* red resume offset */
	bool status;		/* static threshold enabled or not */
};

/* ============================================================================
 * PPE V4 Specific Definitions
 * ============================================================================
 */

/* Layout V4 - 9-bit isolation bitmap */
#define PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP_V4  0x1ff00  /* Bits 16:8 (9 bits) */
#define PPE_PORT_BRIDGE_CTRL_TXMAC_EN_V4            0x20000  /* Bit 17 */
#define PPE_PORT_BRIDGE_CTRL_PROMISC_EN_V4          0x40000  /* Bit 18 */

#endif /* _NSS_SWITCH_TYPES_H_ */
